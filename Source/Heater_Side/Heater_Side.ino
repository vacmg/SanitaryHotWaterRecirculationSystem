/**
 * This example shows how to send & receive a message between 2 modules
 */

#include <MAX_RS485.h>
#include "Config.h"

#define PDISABLED 0
#define PENABLED 1

const uint8_t receiverEnablePin =  5;  // HIGH = Driver / LOW = Receptor
const uint8_t driveEnablePin =  4;  // HIGH = Driver / LOW = Receptor
const uint8_t rxPin = 6; // Serial data in pin
const uint8_t txPin = 3; // Serial data out pin

const uint8_t pumpRelayPin = 13;

MAX_RS485 rs485(rxPin, txPin, receiverEnablePin, driveEnablePin); // module constructor

char cmdBuffer[17] = "";
bool pumpEnabled = false;
unsigned long pumpPMillis = 0;

void setup() 
{
  pinMode(pumpRelayPin,OUTPUT);
  digitalWrite(pumpRelayPin,PDISABLED);
  Serial.begin(9600); // Used for debug purposes

  rs485.begin(9600,receivedMessageTimeout); // first argument is serial baud rate & second one is serial input timeout (to enable the use of the find function)

  delay(1000);
}

void autoDisablePumpIfTimeout()
{
  if(pumpEnabled && millis() - pumpPMillis > autoDisablePumpTimeout) // usar esta version para evitar overflow issues
  {
    pumpEnabled = false;
    digitalWrite(pumpRelayPin,PDISABLED);

    debugln(F("ERROR: TIMEOUT REACHED FOR PUMP. DISCONECTING IT..."));
  }
}

void handleRS485Event()
{
  if(rs485.available() && rs485.find(HEADER))
  {
    size_t dataSize = rs485.readBytesUntil('$', cmdBuffer, 16);
    cmdBuffer[dataSize] = '\0';
    debug(F("Command:\t")); debugln(cmdBuffer);

    if(strcmp(cmdBuffer,pumpCMD)==0)
    {
      debugln(F("PUMP CMD PARSED"));

      size_t dataSize = rs485.readBytesUntil('$', cmdBuffer, 16);
      cmdBuffer[dataSize] = '\0';
      debug(F("Arg1:\t")); debugln(cmdBuffer);

      if(atoi(cmdBuffer))
      {
        pumpPMillis = millis();
        pumpEnabled = true;
        digitalWrite(pumpRelayPin,PENABLED);

        debug(F("Starting pump at millis() = ")); debugln(pumpPMillis);
      }
      else
      {
        pumpEnabled = false;
        digitalWrite(pumpRelayPin,PDISABLED);
        debugln(F("Stoping pump"));
      }
    }
    else if(strcmp(cmdBuffer,tempCMD)==0)
    {
      debugln(F("TEMP CMD PARSED"));

      // TODO this
    }
    if(rs485.available())
    {
      debug(F("WARNING: Discarding extra RS485 input data:\t\""));
      while(rs485.available())
      {
        #if DEBUG
          debug(rs485.read());
        #else
          rs485.read();
        #endif
      }
      debug(F("\"\n"));
    }
  }
}

void loop() 
{ 
  
}
