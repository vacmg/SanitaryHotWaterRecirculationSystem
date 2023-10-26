/**
 * This is the source code of the part of the system which is near to the heater source.
 */

#include<avr/wdt.h> /* Header for watchdog timers in AVR */
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
bool pumpEnabled = false;
unsigned long pumpPMillis = 0;

void setup() 
{
  wdt_disable(); /* Disable the watchdog and wait for more than 8 seconds */
  #if !DISABLE_WATCHDOGS
    delay(10000); /* Done so that the Arduino doesn't keep resetting infinitely in case of wrong configuration */
    wdt_enable(WDTO_8S); /* Enable the watchdog with a timeout of 8 seconds */
  #endif

  pinMode(pumpRelayPin,OUTPUT);
  digitalWrite(pumpRelayPin,RELAY_DISABLED);
  Serial.begin(9600); // Used for debug purposes

  debugln(F("\nSanitaryHotWaterRecirculationSystem - Heater side system successfully started!!!"));

  rs485.begin(9600,receivedMessageTimeout); // first argument is serial baud rate & second one is serial input timeout (to enable the use of the find function)

  delay(1000);
}

float getTemp()
{
  #warning Implement DS18B20 logic
  return 63.38; // TODO implement DS18B20 logic
}

void autoDisablePumpIfTimeout()
{
  if(pumpEnabled && millis() - pumpPMillis > autoDisablePumpTimeout)
  {
    pumpEnabled = false;
    digitalWrite(pumpRelayPin,RELAY_DISABLED);

    debug(F("ERROR: TIMEOUT REACHED FOR PUMP. (Elapsed time = "));
    debug(millis() - pumpPMillis); debug(F("ms > Timeout = "));
    debug(autoDisablePumpTimeout);debugln(F("ms)\nDISCONECTING IT..."));
  }
}

void handleRS485Event()
{
  if(rs485.available() && rs485.find(HEADER))
  {
    size_t dataSize = rs485.readBytesUntil('$', cmdBuffer, 16);
    cmdBuffer[dataSize] = '\0';
    debug(F("Command:\t")); debugln(cmdBuffer);

    if(strcmp(cmdBuffer,WTDRSTCMD) == 0)
    {
      wdt_reset();
      debugln(F("Watchdog Reset CMD PARSED"));

      sprintf(cmdBuffer,"%s%s$",HEADER,OKCMD);

      debug(F("Sending OK CMD: ")); debugln(cmdBuffer);

      rs485.print(cmdBuffer);
    }
    else if(strcmp(cmdBuffer,pumpCMD)==0)
    {
      debugln(F("PUMP CMD PARSED"));

      size_t dataSize = rs485.readBytesUntil('$', cmdBuffer, 16);
      cmdBuffer[dataSize] = '\0';
      debug(F("Enable:\t")); debugln(cmdBuffer);

      if(atoi(cmdBuffer))
      {
        pumpPMillis = millis();
        pumpEnabled = true;
        digitalWrite(pumpRelayPin,RELAY_ENABLED);

        debug(F("Starting pump at millis() = ")); debugln(pumpPMillis);
      }
      else
      {
        digitalWrite(pumpRelayPin,RELAY_DISABLED);

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
    else if(strcmp(cmdBuffer,tempCMD)==0)
    {
      debugln(F("TEMP CMD PARSED"));

      int temp = (int)getTemp();

      sprintf(cmdBuffer,"%s%s$%d$",HEADER,tempCMD,temp);

      debug(F("Sending TEMP CMD ANSWER: ")); debugln(cmdBuffer);

      rs485.print(cmdBuffer);
    }
    if(rs485.available())
    {
      debug(F("WARNING: Discarding extra RS485 input data:\t\""));
      while(rs485.available())
      {
        #if DEBUG
          debug((char)rs485.read());
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
  handleRS485Event();
  autoDisablePumpIfTimeout();

  delay(100);
}
