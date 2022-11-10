//////////// NFC STUFF:
#include <Wire.h>
#include <SPI.h> 
#include <UNIT_PN532.h> //esp32 modded library https://blog.uelectronics.com/wp-content/uploads/2021/10/UNIT-PN532.zip
//SPI to ESP32 conections
//#define PN532_SCK  (18)
//#define PN532_MOSI (23)
#define PN532_SS   (5)
//#define PN532_MISO (19)

UNIT_PN532 nfc(PN532_SS);// SS line for SPI communication

#define NUMBER_UIDS 3 //4 byte UIDs are padded with zeroes (0x00)


const uint8_t uids[NUMBER_UIDS][8] = {
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // uid 1
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // uid 2
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // uid 3
};

#define MAXIMUM_INVALID_ATTEMPTS 13
uint8_t invalidAttempts = 0;
const uint8_t invalidDelays[MAXIMUM_INVALID_ATTEMPTS] = {1, 3, 4, 5, 8, 12, 17, 23, 30, 38, 47, 57, 68};

///////////// Alexa stuff:

#include <Arduino.h>   
#include <WiFi.h> //ESP32 specific

#include "SinricPro.h"
#include "SinricProDoorbell.h"
#define WIFI_SSID         "your ssid"
#define WIFI_PASS         "your password"
#define APP_KEY           "taken from sinric"      // Should look like "de0bxxxx-1x3x-4x3x-ax2x-5dabxxxxxxxx"
#define APP_SECRET        "taken from sinric"   // Should look like "5f36xxxx-x3x7-4x3x-xexe-e86724a9xxxx-4c4axxxx-3x3x-x5xe-x9x3-333d65xxxxxx"
#define DOORBELL_ID       "taken from sinric"    // Should look like "5dc1564130xxxxxxxxxxxxxx"

// setup function for WiFi connection
void setupWiFi() {
  Serial.printf("\r\n[Wifi]: Connecting");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.printf(".");
    delay(250);
  }
  IPAddress localIP = WiFi.localIP();
  Serial.printf("connected!\r\n[WiFi]: IP-Address is %d.%d.%d.%d\r\n", localIP[0], localIP[1], localIP[2], localIP[3]);
}
// setup function for SinricPro
void setupSinricPro() {
  // add doorbell device to SinricPro
 
  SinricProDoorbell& myDoorbell = SinricPro[DOORBELL_ID];
  // setup SinricPro
  SinricPro.onConnected([](){ Serial.printf("Connected to SinricPro\r\n"); }); 
  SinricPro.onDisconnected([](){ Serial.printf("Disconnected from SinricPro\r\n"); });
  SinricPro.begin(APP_KEY, APP_SECRET);
}


void setup() {
  Serial.begin(115200); //Serial port start at 115200 baud
  nfc.begin(); //PN532 communication begins
  

  uint32_t versiondata = nfc.getFirmwareVersion();//Check pn532 connection 

  if (! versiondata) { //If communication with the board is not found--->
    Serial.print("PN53x is fucked, check your connections,");
    while (1); // if its not fucked,
  }
  Serial.print("Pn532 fucking work buddy! lets go!");
  Serial.println((versiondata >> 24) & 0xFF, HEX); //Print serial what version of firmware is it
  Serial.print("Firmware ver. ");
  Serial.print((versiondata >> 16) & 0xFF, DEC); // firmware version 
  Serial.print('.'); Serial.println((versiondata >> 8) & 0xFF, DEC);

  //Set the maximum number of retries to read from a card.
  //This prevents us from having to wait forever for a card,
  //which is the default behavior of the PN532.
  nfc.setPassiveActivationRetries(0x11);

  nfc.SAMConfig(); //Configure reader

  setupWiFi();
  setupSinricPro();

  Serial.println("Wifi connected... Waiting for an ISO14443A card... scan something dickhead");

  

  //add a beep here
}

void myDelay(int ms){
for(int i=0;i<ms;i++){
SinricPro.handle();
delay(1);
}}

void loop() {
  SinricPro.handle(); 
  boolean success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
Serial.println(millis());
success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength, 500);
Serial.println(millis());
  SinricPro.handle();  //TODO: The access denied delays might fuck with the SinricPro service routine, since it's supposed to run constantly...

  if (success) {
    Serial.println(F("Found a card!"));
    Serial.print(F("UID Length: "));Serial.print(uidLength, DEC);Serial.println(" bytes");
    Serial.print(F("UID Value: "));
    
    for (uint8_t i=0; i < uidLength; i++) 
    {
      Serial.print(" 0x");Serial.print(uid[i], HEX); 
    }
    Serial.println("");


    bool validUID = false;

    if (uidLength == 4) Serial.println("4B UID");
    else if (uidLength == 7) Serial.println("7B UID");
    
    for (int i = 0; i < NUMBER_UIDS; i++) {
        bool same = true;
        for (int j = 0; j < uidLength; j++) {
          if (uids[i][j] != uid[j]) {
            same = false;
          }
        }
        if (same) {
          validUID = true;
          //i=NUMBER_UIDS;
          break; //ignore the rest of the UIDs in the list.. we found a match, so why bother?
        }
      }
    
    if (validUID) {
      Serial.println("MATCH!!!");
      SinricPro.handle();
      SinricProDoorbell& myDoorbell = SinricPro[DOORBELL_ID];
    myDoorbell.sendDoorbellEvent();
      //add beep here .. uhm.. okay... *BEEEEP* ... there.. happy now?
SinricPro.handle();
      invalidAttempts = 0;
myDelay(2000);
    } else {
      Serial.print(F("Unauthorised card"));

      //add x2 beep here *BEEEP* *BEEEP*
      myDelay(invalidDelays[invalidAttempts] * 1000);
      if (invalidAttempts < MAXIMUM_INVALID_ATTEMPTS-1) {
        invalidAttempts++;
      }
    }
  }
}
