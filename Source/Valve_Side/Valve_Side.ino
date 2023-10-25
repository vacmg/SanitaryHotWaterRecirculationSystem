/**
 * This is the source code of the part of the system which is near to the return valve.
 */

#include<avr/wdt.h> /* Header for watchdog timers in AVR */
#include <EEPROM.h>
#include <MAX_RS485.h>
#include "Config.h"

const uint8_t receiverEnablePin =  5;  // HIGH = Driver / LOW = Receptor
const uint8_t driveEnablePin =  4;  // HIGH = Driver / LOW = Receptor
const uint8_t rxPin = 6; // Serial data in pin
const uint8_t txPin = 3; // Serial data out pin

#warning Change this pin to an actual relay
const uint8_t valveRelayPin = 13;

MAX_RS485 rs485(rxPin, txPin, receiverEnablePin, driveEnablePin); // module constructor

typedef enum SystemStatus{WaitingCold, DrivingWater, ServingWater} Status;
#define changeStatus(newStatus) do {debug(F("Changing Status from ")); debug(statusToString(status)); status = newStatus; debug(F(" to ")); debugln(statusToString(status));} while(0)

const int ERROR_REGISTER_ADDRESS = 10;
const int NUM_OF_ERROR_TYPES = 1;
typedef enum {NO_ERROR = 0} ErrorType;
typedef struct 
{
  bool flags[NUM_OF_ERROR_TYPES];
  
} 
ErrorData;

ErrorData errorData = {};

int heaterTemp = 0;
int desiredTemp = INT16_MAX;
unsigned long heaterTempPMillis = 0;
unsigned long valveTempPMillis = 0;
unsigned long watchdogsPMillis = 0;
Status status = WaitingCold;

bool triggerVal = false; // TODO remove this
int valveTemp = 0; // TODO remove this


const char* statusToString(Status status)
{
  switch (status) 
  {
    case WaitingCold:
      return "WaitingCold";
    case DrivingWater:
      return "DrivingWater";
    case ServingWater:
      return "ServingWater";
  }
}

void error(ErrorType err)
{
  #warning TODO error handler
  debugln(F("\n\nERROR\n"));

  EEPROM.put(ERROR_REGISTER_ADDRESS,errorData);
  while (1);
}

bool getValveTempIfNecessary()
{
  if(millis() - valveTempPMillis > valveTempGatheringPeriod) // each sec update valve temp & compare 
  {
    #warning TODO valve temp sensor reading
    debug(F("Current valve temp:\t"));
    valveTemp = valveTemp; // TODO
    debug(valveTemp); debug(F("\tDesired temp:\t"));debugln(desiredTemp);

    valveTempPMillis = millis();
    return true;
  }
  return false;
}

bool getHeaterTempIfNecessary()
{
  char buffer[17];
  if(millis() - heaterTempPMillis > heaterTempGatheringPeriod) // each 10 secs update heater temp
  {
    sprintf(buffer,"%s%s$",HEADER,tempCMD);
    rs485.print(buffer);
    debugln(F("HEATER TEMP REQUEST CMD SENT"));

    delay(tempMessageProcessingMultiplier*receivedMessageTimeout);

    if(rs485.available() && rs485.find(HEADER))
    {
      size_t dataSize = rs485.readBytesUntil('$', buffer, 16);
      buffer[dataSize] = '\0';

      if(strcmp(buffer, tempCMD) == 0)
      {
        size_t dataSize = rs485.readBytesUntil('$', buffer, 16);
        buffer[dataSize] = '\0';
        
        debug(F("Heater Temp updated from ")); debug(heaterTemp);
        heaterTemp = atoi(buffer);
        debug(F(" to ")); debugln(heaterTemp);

        debug(F("Desired Temp updated from ")); debug(desiredTemp);

        if(status == ServingWater)
        {
          desiredTemp = round(heaterTemp*pipeHeatTransportEfficiency*coldWaterTemperatureMultiplier);
        }
        else
        {
          desiredTemp = round(heaterTemp*pipeHeatTransportEfficiency);
        }
        
        debug(F(" to ")); debugln(desiredTemp);
      }
      else 
      {
        error();
      }
    }
    else 
    {
      error();
    }

    heaterTempPMillis = millis();
    return true;
  }
  return false;
}

#warning Implement real trigger code
bool isTriggerActive()
{
  return triggerVal;
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
  else
  {
    valveTemp = atoi(buffer);
    Serial.print(F("Setting valve temp to: ")); Serial.println(valveTemp);
  }

  Serial.println(F("\nPress e or d to enable or disable trigger\nor send a number to incorporate it as valve temp\n"));
}

void resetWatchdogs()
{
  char buffer[17];
  if(millis() - watchdogsPMillis > watchdogResetPeriod) // Reset both watchdogs once in a watchdogResetPeriod
  {
    wdt_reset();
    watchdogsPMillis = millis();

    sprintf(buffer,"%s%s$",HEADER,WTDRSTCMD);

    debug(F("Sending Watchdog Reset CMD: ")); debugln(buffer);

    rs485.print(buffer);

    delay(wdtRstMessageProcessingMultiplier*receivedMessageTimeout);

    if(rs485.available() && rs485.find(HEADER))
    {
      size_t dataSize = rs485.readBytesUntil('$', buffer, 16);
      buffer[dataSize] = '\0';

      if(strcmp(buffer,OKCMD)==0)
      {
        debugln(F("WTD_RST CMD RESULT: OK"));
      }
      else
      {
        error();
      }
    }
    else
    {
      error();
    }
  }
}

void setup() 
{
  wdt_disable(); /* Disable the watchdog and wait for more than 8 seconds */
  delay(10000); /* Done so that the Arduino doesn't keep resetting infinitely in case of wrong configuration */
  wdt_enable(WDTO_8S); /* Enable the watchdog with a timeout of 8 seconds */

  pinMode(valveRelayPin,OUTPUT);
  digitalWrite(valveRelayPin,RELAY_DISABLED); // TODO check valve feedback pin

  Serial.begin(9600); // Used for debug purposes
  delay(3000);

  EEPROM.get(ERROR_REGISTER_ADDRESS, errorData);

  debugln(F("\nSanitaryHotWaterRecirculationSystem - Valve side system successfully started!!!"));

  Serial.println(F("\nPress e or d to enable or disable trigger\nor send a number to incorporate it as valve temp\n"));

  rs485.begin(9600,receivedMessageTimeout); // first argument is serial baud rate & second one is serial input timeout (to enable the use of the find function)

  delay(1000);
}

void loop() 
{
  char buffer[17];
  switch(status)
  {
    case WaitingCold:

      if(isTriggerActive())
      {
        sprintf(buffer,"%s%s$1$",HEADER,pumpCMD);
        rs485.print(buffer);
        debugln(F("PUMP ON CMD SENT"));
        delay(pumpMessageProcessingMultiplier*receivedMessageTimeout);

        if(rs485.available() && rs485.find(HEADER))
        {
          size_t dataSize = rs485.readBytesUntil('$', buffer, 16);
          buffer[dataSize] = '\0';

          if(strcmp(buffer,OKCMD)==0) // Transition to DrivingWater code
          {
            debugln(F("PUMP CMD RESULT: OK"));

            heaterTempPMillis = 0; // Set millis timers
            valveTempPMillis = 0;
            changeStatus(DrivingWater);
          }
          else 
          {
            error();
          }
        }
        else 
        {
          error();
        }
      }

    break;

    case DrivingWater:

      getHeaterTempIfNecessary();

      if(getValveTempIfNecessary() && valveTemp >= desiredTemp) // compare temps & change phase
      {
        sprintf(buffer,"%s%s$0$",HEADER,pumpCMD);
        rs485.print(buffer);
        debugln(F("PUMP OFF CMD SENT"));
        delay(pumpMessageProcessingMultiplier*receivedMessageTimeout);

        if(rs485.available() && rs485.find(HEADER))
        {
          size_t dataSize = rs485.readBytesUntil('$', buffer, 16);
          buffer[dataSize] = '\0';

          if(strcmp(buffer,OKCMD)==0) // Transition to DrivingWater code
          {
            debugln(F("PUMP CMD RESULT: OK"));

            heaterTempPMillis = 0; // Set millis timers
            valveTempPMillis = 0;
            changeStatus(DrivingWater);
          }
          else 
          {
            error();
          }
        }

        digitalWrite(valveRelayPin, RELAY_ENABLED);
        changeStatus(ServingWater);
      }

    break;

    case ServingWater:

      getHeaterTempIfNecessary();

      if(getValveTempIfNecessary() && !isTriggerActive() && valveTemp < desiredTemp)
      {
        digitalWrite(valveRelayPin, RELAY_DISABLED);
        changeStatus(WaitingCold);
      }

    break;
  }
  resetWatchdogs();
}
