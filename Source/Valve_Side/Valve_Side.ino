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

typedef enum SystemStatus{WaitingCold, DrivingWater, ServingWater} Status;
#define changeStatus(newStatus) do {debug(F("Changing Status from ")); debug(statusToString(status)); status = newStatus; debug(F(" to ")); debug(statusToString(status));} while(0)

int heaterTemp = 0;
int desiredTemp = INT16_MAX;
unsigned long heaterTempPMillis = 0;
unsigned long valveTempPMillis = 0;
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

void error()
{
  #warning TODO error handler
  debugln(F("\n\nERROR\n"));
  while (1);
}

void setup() 
{
  pinMode(valveRelayPin,OUTPUT);
  digitalWrite(valveRelayPin,RELAY_DISABLED); // TODO check valve feedback pin

  Serial.begin(9600); // Used for debug purposes
  delay(3000);

  debugln(F("\nSanitaryHotWaterRecirculationSystem - Valve side system successfully started!!!"));

  Serial.println(F("\nPress e or d to enable or disable trigger\nor send a number to incorporate it as valve temp\n"));

  rs485.begin(9600,receivedMessageTimeout); // first argument is serial baud rate & second one is serial input timeout (to enable the use of the find function)

  delay(1000);
}

float getValveTemp()
{
  #warning Implement DS18B20 logic
  return 63.38; // TODO implement DS18B20 logic
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
            desiredTemp = round(heaterTemp*pipeHeatTransportEfficiency);
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
      }

      if(millis() - valveTempPMillis > valveTempGatheringPeriod) // each sec update valve temp & compare 
      {
        #warning TODO valve temp sensor reading
        debug(F("Current valve temp:\t"));
        valveTemp = valveTemp; // TODO
        debug(valveTemp); debug(F("\tDesired temp:\t"));debugln(desiredTemp);







        if(valveTemp >= desiredTemp) // compare temps & change phase
        {
          digitalWrite(valveRelayPin, RELAY_ENABLED);
          changeStatus(ServingWater);
        }

        valveTempPMillis = millis();
      }

      

    break;

    case ServingWater:

      //

    break;
  }
  // TODO reset_both_watchdogs();
}
