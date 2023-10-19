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
const uint8_t valveRelayPin = 13;

MAX_RS485 rs485(rxPin, txPin, receiverEnablePin, driveEnablePin); // module constructor

char cmdBuffer[17] = "";

bool triggerVal = false; // TODO remove this


void error()
{
  #warning TODO error handler
}

void setup() 
{
  pinMode(valveRelayPin,OUTPUT);
  digitalWrite(valveRelayPin,PDISABLED);

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

void triggerHandler() // TODO rehacer con FSM (estados)
{
  char buffer[17];

  sprintf(buffer, "%s%s$", HEADER, tempCMD);
  rs485.print(buffer);

  if(rs485.find(HEADER))
  {
    size_t dataSize = rs485.readBytesUntil('$', cmdBuffer, 16);
    cmdBuffer[dataSize] = '\0';
    debug(F("Command Answer:\t")); debugln(buffer);

    if(strcmp(cmdBuffer,tempCMD)==0)
    {
      debugln(F("TEMP CMD PARSED"));

      size_t dataSize = rs485.readBytesUntil('$', buffer, 16);
      cmdBuffer[dataSize] = '\0';
      debug(F("Temp:\t")); debugln(buffer);

      if(atoi(cmdBuffer))
      {
        pumpPMillis = millis();
        pumpEnabled = true;
        digitalWrite(pumpRelayPin,PENABLED);

        debug(F("Starting pump at millis() = ")); debugln(pumpPMillis);
      }
      else
      {
        digitalWrite(pumpRelayPin,PDISABLED);

        debug(F("Stoping pump... ")); 
        #if DEBUG
          if(pumpEnabled)
          {
            debug(F("Elapsed time = "));
            debug(millis() - pumpPMillis);debug(F("ms)"));
          }
        #endif
        debugln();

        pumpEnabled = false;
      }

      sprintf(cmdBuffer,"%s%s$",HEADER,OKCMD);

      debug(F("Sending OK CMD: ")); debugln(cmdBuffer);

      rs485.print(cmdBuffer);
    }
    else if(strcmp(cmdBuffer,tempCMD)==0);
  }
  else
  {
    debugln("ERROR TIMEOUT AT TEMP RESPONSE");
    error();
  }
}

#warning Remove this debug interface
void serialEvent()
{
  char buffer[17];
  size_t len = Serial.readBytesUntil('\n',buffer,16);
  buffer[len] = '\0';

  Serial.print(F("The user input: ")); Serial.println(buffer);

  if(strcmp(buffer,"e") == 0)
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
