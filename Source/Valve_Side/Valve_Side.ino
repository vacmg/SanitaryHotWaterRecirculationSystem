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

typedef enum {NO_ERROR = 0, ERROR_CONNECTION_NOT_ESTABLISHED, ERROR_RS485_NO_RESPONSE, ERROR_RS485_UNEXPECTED_MESSAGE,             ENUM_LEN} ErrorCode;
#define NUM_OF_ERROR_TYPES ErrorCode::ENUM_LEN
#define ERROR_MESSAGE_SIZE (EEPROM_SIZE-(sizeof(char)*4)-(NUM_OF_ERROR_TYPES * sizeof(bool)))/(NUM_OF_ERROR_TYPES)

typedef struct 
{
  char magic[4];
  bool flags[NUM_OF_ERROR_TYPES];
  char message[NUM_OF_ERROR_TYPES][ERROR_MESSAGE_SIZE];
} 
ErrorData;


int heaterTemp = 0;
int desiredTemp = INT16_MAX;
unsigned long heaterTempPMillis = 0;
unsigned long valveTempPMillis = 0;
unsigned long watchdogsPMillis = 0;
Status status = WaitingCold;

ErrorData errorData;

bool triggerVal = false; // TODO remove this
int valveTemp = 0; // TODO remove this



const char* getErrorName(ErrorCode error) 
{
  switch (error) 
  {
    case NO_ERROR:
      return "NO_ERROR";
    case ERROR_RS485_NO_RESPONSE:
      return "ERROR_RS485_NO_RESPONSE";
    case ERROR_RS485_UNEXPECTED_MESSAGE:
      return "ERROR_RS485_UNEXPECTED_MESSAGE";
    case ERROR_CONNECTION_NOT_ESTABLISHED:
      return "ERROR_CONNECTION_NOT_ESTABLISHED";
    default:
      return "Unknown Error";
  }
}

void printErrorData(ErrorData& data, bool printAll = false)
{
  if(strcmp(errorData.magic,EEPROM_ERROR_MAGIC_STR) == 0)
  {
    Serial.println(F("\n-----------------------------------------"));
    Serial.println(F("Error list:\n"));
    for(uint8_t i = NO_ERROR + 1; i<NUM_OF_ERROR_TYPES;i++)
    {
      if(printAll || data.flags[i])
      {
        Serial.print(getErrorName((ErrorCode)i)); Serial.print(F(":\t"));
        Serial.println(data.flags[i]?data.message[i]:"OK");
      }
    }
  }
  else
  {
    Serial.println(F("ErrorData is not valid\n"));
  }
  Serial.println(F("-----------------------------------------\n"));;
}

void error(ErrorCode err, const char* message)
{
  Serial.println(F("\n-----------------------------------------"));
  Serial.println(F("-----------------------------------------"));
  Serial.print(getErrorName(err)); Serial.print(F(":\n"));
  Serial.println(message);
  Serial.println(F("-----------------------------------------"));
  Serial.println(F("-----------------------------------------\n"));

  errorData.flags[err] = true;
  strcpy(errorData.message[err],message);
  #if !EEPROM_DONT_WRITE
    EEPROM.put(ERROR_REGISTER_ADDRESS,errorData);
  #endif

  Serial.print(F("System is rebooting"));
  rebootLoop();
}

void loadErrorRegister()
{
  EEPROM.get(ERROR_REGISTER_ADDRESS, errorData);
  if(strcmp(errorData.magic,EEPROM_ERROR_MAGIC_STR) != 0)
  {
    Serial.println(F("-----------------------------------------"));
    Serial.print(F("WARNING:\tEEPROM empty or corrupted\nRestoring it...\t"));
    memset(&errorData,'\0',sizeof(errorData));
    strcpy(errorData.magic,EEPROM_ERROR_MAGIC_STR);

    EEPROM.put(ERROR_REGISTER_ADDRESS, errorData);

    Serial.println(F("DONE"));
    Serial.println(F("-----------------------------------------\n"));
  }
  printErrorData(errorData);
}


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
        char message[64];
        sprintf(message,"Expected 'TEMP';\tReceived '%s'",buffer);
        error(ERROR_RS485_UNEXPECTED_MESSAGE, buffer);
      }
    }
    else 
    {
      error(ERROR_RS485_NO_RESPONSE,"Timeout receiving TEMP CMD ANSWER");
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

  if(strcmp(buffer,"clear") == 0)
  {
    errorData.magic[0]++;
    EEPROM.put(ERROR_REGISTER_ADDRESS, errorData);
    Serial.print(F("Error Register Invalidated\nRebooting"));
    rebootLoop();
  }
  else if(strcmp(buffer,"errorlist") == 0)
  {
    printErrorData(errorData,true);
  }
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
  else
  {
    valveTemp = atoi(buffer);
    Serial.print(F("Setting valve temp to: ")); Serial.println(valveTemp);
  }

  Serial.println(F("\nType 'clear' to invalidate the Error Register or errorlist to print the error list\nPress e or d to enable or disable trigger\nor send a number to incorporate it as valve temp\n"));
}

#if !DISABLE_WATCHDOGS
void resetWatchdogs()
{
  char buffer[17];
  if(millis() - watchdogsPMillis > watchdogResetPeriod) // Reset both watchdogs once in a watchdogResetPeriod
  {
    wdt_reset();
    watchdogsPMillis = millis();

    sprintf(buffer,"%s%s$",HEADER,WTDRSTCMD);

    #if DEBUGWATCHDOG
    debug(F("Sending Watchdog Reset CMD: ")); debugln(buffer);
    #endif

    rs485.print(buffer);

    delay(wdtRstMessageProcessingMultiplier*receivedMessageTimeout);

    if(rs485.available() && rs485.find(HEADER))
    {
      size_t dataSize = rs485.readBytesUntil('$', buffer, 16);
      buffer[dataSize] = '\0';

      if(strcmp(buffer,OKCMD)!=0)
      {
        char message[64];
        sprintf(message,"Expected 'OK';\tReceived '%s'",buffer);
        error(ERROR_RS485_UNEXPECTED_MESSAGE, buffer);
      }
      #if DEBUGWATCHDOG
      else 
      {
        debugln(F("WTD_RST CMD RESULT: OK"));
      }
      #endif
    }
    else 
    {
      error(ERROR_RS485_NO_RESPONSE,"Timeout receiving WTD_RST CMD ANSWER");
    }
  }
}
#endif


void connectToHeater()
{
  #if !DISABLE_WATCHDOGS

  char buffer[17];
  unsigned long connectionPMillis = millis();
  bool connected = false;

  debugln(F("Connecting to heater MCU..."));

  while(!connected && millis() - connectionPMillis < initConnectionTimeout)
  {
    if(millis() - watchdogsPMillis > watchdogResetPeriod) // Reset both watchdogs once in a watchdogResetPeriod
    {
      wdt_reset();
      watchdogsPMillis = millis();

      sprintf(buffer,"%s%s$",HEADER,WTDRSTCMD);

      #if DEBUGWATCHDOG
      debug(F("Sending Watchdog Reset CMD: ")); debugln(buffer);
      #endif

      rs485.print(buffer);

      delay(wdtRstMessageProcessingMultiplier*receivedMessageTimeout);

      if(rs485.available() && rs485.find(HEADER))
      {
        size_t dataSize = rs485.readBytesUntil('$', buffer, 16);
        buffer[dataSize] = '\0';

        if(strcmp(buffer,OKCMD)!=0)
        {
          #if DEBUGWATCHDOG
          debug(F("Expected 'OK';\tReceived '"));debug(buffer);debug(F("'\n"));
          #endif
        }
        else 
        {
          connected = true;
          #if DEBUGWATCHDOG
          debugln(F("WTD_RST CMD RESULT: OK"));
          #endif
        }
      }
      #if DEBUGWATCHDOG
      else 
      {
        debugln(F("Timeout receiving WTD_RST CMD ANSWER"));
      }
      #endif
    }
  }
  if(!connected)
  {
    error(ERROR_CONNECTION_NOT_ESTABLISHED,"ERROR: Cannot connect to heater MCU");
  }
  debugln(F("Connected to heater MCU!!!"));
  delay(1500);

  #endif
}


void setup() 
{
  wdt_disable(); /* Disable the watchdog and wait for more than 8 seconds */
  #if !DISABLE_WATCHDOGS
    delay(10000); /* Done so that the Arduino doesn't keep resetting infinitely in case of wrong configuration */
    wdt_enable(WDTO_8S); /* Enable the watchdog with a timeout of 8 seconds */
  #endif

  pinMode(valveRelayPin,OUTPUT);
  digitalWrite(valveRelayPin,RELAY_DISABLED); // TODO check valve feedback pin

  Serial.begin(9600); // Used for debug purposes
  delay(3000);
  Serial.println(F("\n------------------------------------------"));
  Serial.println(F(  "|                SHWRS-VS                |"));
  Serial.println(F(  "------------------------------------------\n"));

  loadErrorRegister();

  rs485.begin(9600,receivedMessageTimeout); // first argument is serial baud rate & second one is serial input timeout (to enable the use of the find function)

  delay(1000);

  connectToHeater();

  Serial.println(F("Valve side system successfully started!!!"));

  Serial.println(F("\nType 'clear' to invalidate the Error Register or errorlist to print the error list\nPress e or d to enable or disable trigger\nor send a number to incorporate it as valve temp\n"));
  
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
            char message[64];
            sprintf(message,"Expected 'OK';\tReceived '%s'",buffer);
            error(ERROR_RS485_UNEXPECTED_MESSAGE, buffer);
          }
        }
        else 
        {
          error(ERROR_RS485_NO_RESPONSE,"Timeout receiving PUMP CMD ANSWER");
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
            char message[64];
            sprintf(message,"Expected 'OK';\tReceived '%s'",buffer);
            error(ERROR_RS485_UNEXPECTED_MESSAGE, buffer);
          }
        }
        else 
        {
          error(ERROR_RS485_NO_RESPONSE,"Timeout receiving PUMP CMD ANSWER");
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
  #if !DISABLE_WATCHDOGS
  resetWatchdogs();
  #endif
}
