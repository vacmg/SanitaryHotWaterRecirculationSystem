/**
 * This example shows how to send & receive a message between 2 modules
 */

#include <MAX_RS485.h>
#include "Config.h"

const uint8_t receiverEnablePin =  5;  // HIGH = Driver / LOW = Receptor
const uint8_t driveEnablePin =  4;  // HIGH = Driver / LOW = Receptor
const uint8_t rxPin = 6; // Serial data in pin
const uint8_t txPin = 3; // Serial data out pin

#warning Change this pin to an actual relay
const uint8_t pumpRelayPin = 13;

MAX_RS485 rs485(rxPin, txPin, receiverEnablePin, driveEnablePin); // module constructor

char cmdBuffer[17] = "";

bool triggerVal = false; // TODO remove this

void setup() 
{
  pinMode(pumpRelayPin,OUTPUT);
  digitalWrite(pumpRelayPin,PDISABLED);

  Serial.begin(9600); // Used for debug purposes
  delay(1000);

  debugln(F("\nSanitaryHotWaterRecirculationSystem - Return side system successfully started!!!"));

  debugln(F("\nPress e or d to enable or disable trigger\n"));

  rs485.begin(9600,receivedMessageTimeout); // first argument is serial baud rate & second one is serial input timeout (to enable the use of the find function)

  delay(1000);
}

#warning Implement real trigger code
bool triggerActive()
{
  return triggerVal;
}

void triggerHandler()
{
  char buffer[17];

  
}

#warning Remove this debug interface
void serialEvent()
{
  char buffer[17];
  size_t len = Serial.readBytesUntil('\n',buffer,16);
  buffer[len] = '\0';

  Serial.print(F("The user input: ")); Serial.println(buffer);

  else if(strcmp(buffer,"e") == 0)
  {
    Serial.println(F("Enabling trigger"));
    triggerVal = true;
  }
  else if(strcmp(buffer,"d") == 0)
  {
    Serial.println(F("Disabling trigger"));
    triggerVal = false;
  }
}

void loop() 
{ 
  if(triggerActive())
  {
    triggerHandler();
  }
  delay(100);
}
