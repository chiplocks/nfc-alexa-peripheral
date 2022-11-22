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
  {0xD4, 0xC9, 0x46, 0x20, 0x00, 0x00, 0x00}, // Head
  {0x04, 0x21, 0xA0, 0x32, 0x0A, 0x54, 0x80}, // Shoulders
  {0x04, 0x5C, 0xD8, 0x6A, 0x81, 0x65, 0x81}, // Knees and toes, knees and toes.
};

#define MAXIMUM_INVALID_ATTEMPTS 13
uint8_t invalidAttempts = 0;
const uint8_t invalidDelays[MAXIMUM_INVALID_ATTEMPTS] = {1, 3, 4, 5, 8, 12, 17, 23, 30, 38, 47, 57, 68};

// beeper:
const int beepPin = 4;  

// setting PWM properties
const int freq = 2500;
const int ledChannel = 0;
const int resolution = 8;
const int dutyCycle = 128;


//////////// SINRIC PRO STUFF
#include <Arduino.h>
#include <WiFi.h>

#include "SinricPro.h"
#include "SinricProContactsensor.h"
#define WIFI_SSID         "your wifi"
#define WIFI_PASS         "wifi password"
#define APP_KEY           "taken from sinric"      // Should look like "de0bxxxx-1x3x-4x3x-ax2x-5dabxxxxxxxx"
#define APP_SECRET        "taken from sinric"   // Should look like "5f36xxxx-x3x7-4x3x-xexe-e86724a9xxxx-4c4axxxx-3x3x-x5xe-x9x3-333d65xxxxxx"
#define CONTACT_ID        "taken from sinric"    // Should look like "5dc1564130xxxxxxxxxxxxxx"

bool unlockedState = false;                     

//Custom delay function, so sinric's websockets don't lose their connection.
void myDelay(int ms){
for(int i=0;i<ms;i++){
SinricPro.handle();
delay(1);
  }
}

void beep(int duration)
{
  ledcWrite(ledChannel, dutyCycle); //PWM on
  myDelay(duration); //wait for n-ish milliseconds
  ledcWrite(ledChannel, 0); //PWM off
}

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
  // add device to SinricPro
  SinricProContactsensor& myContact = SinricPro[CONTACT_ID];
  // set callback function to device
      //myContact.onPowerState(onPowerState); //probably not neccesary.
  // setup SinricPro
  SinricPro.onConnected([](){ Serial.printf("Connected to SinricPro\r\n"); });
  SinricPro.onDisconnected([](){ Serial.printf("Disconnected from SinricPro\r\n"); });
  SinricPro.begin(APP_KEY, APP_SECRET);
}
// main setup function
void setup() {
  Serial.begin(115200); //Serial port start at 115200 baud
  nfc.begin(); //PN532 communication begins

  ledcSetup(ledChannel, freq, resolution); //Set up PWM channel for BEEP
  ledcWrite(ledChannel, 0); //Turn PWM down to 0
  ledcAttachPin(beepPin, ledChannel); //Assign PWM pin to D0

  

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
  nfc.setPassiveActivationRetries(0x11); //NOW lookie here!!!

  nfc.SAMConfig(); //Configure reader

  setupWiFi();
  setupSinricPro();

  Serial.println("Wifi connected... Waiting for an ISO14443A card... scan something dickhead");

  beep(100);

}


void loop() {

  boolean success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

  SinricPro.handle();
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength, 500);

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
        unlockedState =! unlockedState; //Flip the boolean: Locked->Unlocked, Unlocked->Locked, etc.
        SinricProContactsensor& myContact = SinricPro[CONTACT_ID]; // get contact sensor device
        myContact.sendContactEvent(unlockedState);      // send event with actual state
      beep(100);
      SinricPro.handle();
      invalidAttempts = 0;
      myDelay(2000);
    } 
    else {
      Serial.print(F("Unauthorised card"));

      
      beep(100);
      myDelay(500);
      beep(100);
      
      myDelay(invalidDelays[invalidAttempts] * 1000);
      if (invalidAttempts < MAXIMUM_INVALID_ATTEMPTS-1) {
        invalidAttempts++;
      }
    }
  }
}
