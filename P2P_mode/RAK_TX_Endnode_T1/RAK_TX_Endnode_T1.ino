/*
 * Project: River monitoring with LoRa
 * Device: RAK LoRa Endnode T1
 * Author: Wilson Cosmo
 */

#include <Arduino.h>
#include <SX126x-RAK4630.h> //http://librarymanager/All#SX126x
#include <SPI.h>
#include <OneWire.h>
#include <DallasTemperature.h>

//for RAK5811 module:
#include <Wire.h>
#ifdef _VARIANT_RAK4630_
#include <Adafruit_TinyUSB.h>
#endif

#define NO_OF_SAMPLES 32

// Function declarations
void OnTxDone(void);
void OnTxTimeout(void);

#ifdef NRF52_SERIES
#define LED_BUILTIN 35
#endif

// Sensor port:
#define ONE_WIRE_BUS WB_IO2
#define SOLAR_OUTPUT WB_A1
OneWire oneWire(ONE_WIRE_BUS); 
DallasTemperature sensors(&oneWire);

// Define LoRa parameters
#define RF_FREQUENCY 915000000	// Hz
#define TX_OUTPUT_POWER 22		// dBm
#define LORA_BANDWIDTH 0		// [0: 125 kHz, 1: 250 kHz, 2: 500 kHz, 3: Reserved]
#define LORA_SPREADING_FACTOR 12 // [SF7..SF12]
#define LORA_CODINGRATE 4		// [1: 4/5, 2: 4/6,  3: 4/7,  4: 4/8]
#define LORA_PREAMBLE_LENGTH 8	// Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT 0	// Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false
#define RX_TIMEOUT_VALUE 3000
#define TX_TIMEOUT_VALUE 3000

static RadioEvents_t RadioEvents;

//battery------------------------------------------

#define PIN_VBAT WB_A0

uint32_t vbat_pin = PIN_VBAT;

#define VBAT_MV_PER_LSB (0.73242188F) // 3.0V ADC range and 12 - bit ADC resolution = 3000mV / 4096
#define VBAT_DIVIDER_COMP (1.73)      // Compensation factor for the VBAT divider, depend on the board
#define REAL_VBAT_MV_PER_LSB (VBAT_DIVIDER_COMP * VBAT_MV_PER_LSB)


float readVBAT(void) //Get RAW Battery Voltage
{
    float raw;

    // Get the raw 12-bit, 0..3000mV ADC value
    raw = analogRead(vbat_pin);

    return raw * REAL_VBAT_MV_PER_LSB;
}


uint8_t mvToPercent(float mvolts) //Convert from raw mv to percentage
{
    if (mvolts < 3300)
        return 0;

    if (mvolts < 3600)
    {
        mvolts -= 3300;
        return mvolts / 30;
    }

    mvolts -= 3600;
    return 10 + (mvolts * 0.15F); // thats mvolts /6.66666666
}


uint8_t mvToLoRaWanBattVal(float mvolts) //get LoRaWan Battery value
{
    if (mvolts < 3300)
        return 0;

    if (mvolts < 3600)
    {
        mvolts -= 3300;
        return mvolts / 30 * 2.55;
    }

    mvolts -= 3600;
    return (10 + (mvolts * 0.15F)) * 2.55;
}
//------------------------------------------


//other variables:
#define pkt_size 59 //24 default, 59 by datasheet

int cc = 0; //counter
uint8_t payload[pkt_size]; //payload
String s_payload = ""; //payload - String format
String d_id = "T1"; //ID - Endnode_01
float solar_v = 0; //solar output in volts
int t_send = 60000; //time between payloads, default of the project: 60000

void setup()
{
  
	// Initialize Serial for debug output
	time_t timeout = millis();
	Serial.begin(9600);
	while (!Serial)
	{
		if ((millis() - timeout) < 5000)
		{
            delay(100);
        }
        else
        {
            break;
        }
	}
 
	Serial.println("=====================================");    
  Serial.println("Device: RAK LoRa Endnode T1");  
  Serial.println("=====================================");

  /* WisBLOCK 5811 Power On*/
  pinMode(WB_IO1, OUTPUT);
  digitalWrite(WB_IO1, HIGH);
  /* WisBLOCK 5811 Power On*/

  pinMode(SOLAR_OUTPUT, INPUT_PULLDOWN); //solar sensor output
  analogReference(AR_INTERNAL_3_0);
  analogOversampling(128);
 
  
	// Initialize LoRa chip.
	lora_rak4630_init();
  
	// Initialize the Radio callbacks
	RadioEvents.TxDone = OnTxDone;
	RadioEvents.RxDone = NULL;
	RadioEvents.TxTimeout = OnTxTimeout;
	RadioEvents.RxTimeout = NULL;
	RadioEvents.RxError = NULL;
	RadioEvents.CadDone = NULL;

	// Initialize the Radio
	Radio.Init(&RadioEvents);

	// Set Radio channel
	Radio.SetChannel(RF_FREQUENCY);

	// Set Radio TX configuration
	Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
					  LORA_SPREADING_FACTOR, LORA_CODINGRATE,
					  LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
					  true, 0, 0, LORA_IQ_INVERSION_ON, TX_TIMEOUT_VALUE);
	
  // Set the analog reference to 3.0V (default = 3.6V)
  analogReference(AR_INTERNAL_3_0);

  // Set the resolution to 12-bit (0..4095)
  analogReadResolution(12); // Can be 8, 10, 12 or 14

  // Let the ADC settle
  delay(1);

  // Get a single ADC sample and throw it away
  readVBAT();  

  sensors.begin(); 

}

void loop()
{  
  float vbat_mv = readVBAT();  //tension in mV
  sensors.requestTemperatures(); //read temperature sensor
  solar_v = getSolarV(); //read solar panel tension  
  	
  cc = cc + 1;    
  s_payload = d_id + ";" + (cc+1) + ";" + String((vbat_mv/1000),2) + ";" + String(sensors.getTempCByIndex(0),1) + ";" + String(solar_v,2) + ";";

  send();
  delay(t_send);
}


void OnTxDone(void) //Function to be executed on Radio Tx Done event
{  
  Serial.print("Sent: ");
  Serial.println((char*)payload); //feedback in serial output  
}

void OnTxTimeout(void) //Function to be executed on Radio Tx Timeout event
{
	Serial.println("OnTxTimeout");
}

void send()
{  
  s_payload.getBytes(payload, pkt_size);
  Radio.Send(payload, pkt_size);
  
}

float getSolarV() //Function to read the Tension output from the Solar Panel
{
  int i;
  int mcu_ain_raw = 0;  
  int average_raw;
  float mcu_ain_voltage;
  float voltage_sensor;               // variable to store the value coming from the sensor
  float real_v;

  for (i = 0; i < NO_OF_SAMPLES; i++)
  {
    mcu_ain_raw += analogRead(SOLAR_OUTPUT);       // the input pin A1 for the potentiometer
  }
  
  average_raw = mcu_ain_raw / i;
  mcu_ain_voltage = average_raw * 3.0 / 1024;   //raef 3.0V / 10bit ADC
  voltage_sensor = mcu_ain_voltage / 0.6;     //WisBlock RAK5811 (0 ~ 5V).   Input signal reduced to 6/10 and output
  real_v = voltage_sensor/0.8; //based on tension division  

  return real_v;
}
