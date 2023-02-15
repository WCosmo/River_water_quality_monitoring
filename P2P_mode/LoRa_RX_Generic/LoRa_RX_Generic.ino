/*
 * Project: River monitoring with LoRa
 * Device: LoRa Generic RX
 * Author: Wilson Cosmo
 */

#include <SPI.h>
#include <LoRa.h>

String tx_id_1 = "T1";
String tx_id_2 = "T2";

void setup() {
  
  Serial.begin(9600);
  while (!Serial);

  Serial.println("=====================================");    
  Serial.println("Device: LoRa Generic RX");  
  Serial.println("=====================================");

  if (!LoRa.begin(915000000)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  LoRa.setSpreadingFactor(12);
  Serial.println("LoRa receiver ok"); 
}

void loop() {
  // try to parse packet
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    // received a packet
    String incoming = "";    

    // read packet
    while (LoRa.available()) {
      incoming += (char)LoRa.read();
    }
    
    if(incoming.startsWith(tx_id_1) || incoming.startsWith(tx_id_2)){
      Serial.print("Received packet: '");
      Serial.print(incoming);
      Serial.print(";");
      Serial.print(String(LoRa.packetRssi()));
      Serial.print(";");
      Serial.print(String(LoRa.packetSnr()));
      Serial.println("'"); 
    }
    

  
  }
}
