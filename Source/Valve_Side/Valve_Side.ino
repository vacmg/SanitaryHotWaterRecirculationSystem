/**
 * This is the source code of the part of the system which is near to the return valve.
 */

#include "Config.h"
#include<avr/wdt.h> /* Header for watchdog timers in AVR */
#include <EEPROM.h>
#include <MAX_RS485.h>

#if !MOCK_SENSORS
#include <OneWire.h>
#include <DallasTemperature.h>
#endif

const uint8_t RECEIVER_ENABLE_PIN =  10;  // HIGH = Driver / LOW = Receptor
const uint8_t DRIVE_ENABLE_PIN =  9;  // HIGH = Driver / LOW = Receptor
const uint8_t RX_PIN = 11; // Serial data in pin
const uint8_t TX_PIN = 8; // Serial data out pin

const uint8_t VALVE_RELAY_PIN = 2;

bool SYSTEM_ENABLED = false;

#if !MOCK_SENSORS
const uint8_t PRESSURE_SENSOR = A0;

const uint8_t TEMP_SENSOR = 12;


OneWire ourWire(TEMP_SENSOR); // Create Onewire instance for temp sensor
DallasTemperature tempSensor(&ourWire); // Create temp sensor instance
#endif

MAX_RS485 rs485(RX_PIN, TX_PIN, RECEIVER_ENABLE_PIN, DRIVE_ENABLE_PIN); // Create rs485 instance


typedef enum SystemStatus{WaitingCold, DrivingWater, ServingWater} Status;
#define changeStatus(newStatus) do {debug(F("Changing Status from ")); debug(statusToString(status)); status = newStatus; debug(F(" to ")); debugln(statusToString(status));} while(0)

typedef enum {NO_ERROR = 0, ERROR_INTERNAL, ERROR_TEMP_SENSOR_INVALID_VALUE, ERROR_PRESSURE_SENSOR_INVALID_VALUE, ERROR_CONNECTION_NOT_ESTABLISHED, ERROR_RS485_NO_RESPONSE, ERROR_RS485_UNEXPECTED_MESSAGE,              ENUM_LEN} ErrorCode;
#define NUM_OF_ERROR_TYPES ErrorCode::ENUM_LEN
#define ERROR_MESSAGE_SIZE (EEPROM_USED_SIZE-(sizeof(char)*4)-(NUM_OF_ERROR_TYPES * sizeof(bool)))/(NUM_OF_ERROR_TYPES)

typedef struct 
{
  char magic[sizeof(EEPROM_ERROR_MAGIC_STR)];
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

#if MOCK_SENSORS
bool triggerVal = false;
#endif

int valveTemp = 0;

static char errorStrBuff[ERROR_MESSAGE_SIZE];


double fmap(double x, double in_min, double in_max, double out_min, double out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}


const char* getErrorName(ErrorCode error) 
{
  switch (error) 
  {
    case NO_ERROR:
      return "NO_ERROR";
    case ERROR_INTERNAL:
      return "ERROR_INTERNAL";
    case ERROR_TEMP_SENSOR_INVALID_VALUE:
      return "ERROR_TEMP_SENSOR_INVALID_VALUE";
    case ERROR_PRESSURE_SENSOR_INVALID_VALUE:
      return "ERROR_PRESSURE_SENSOR_INVALID_VALUE";
    case ERROR_RS485_NO_RESPONSE:
      return "ERROR_RS485_NO_RESPONSE";
    case ERROR_RS485_UNEXPECTED_MESSAGE:
      return "ERROR_RS485_UNEXPECTED_MESSAGE";
    case ERROR_CONNECTION_NOT_ESTABLISHED:
      return "ERROR_CONNECTION_NOT_ESTABLISHED";
    default:
      sprintf(errorStrBuff,"Unknown Error (%d)",error);
      return errorStrBuff;
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


void startPump()
{
  char buffer[17];
  sprintf(buffer,"%s%s$1$",HEADER,pumpCMD);
  rs485.print(buffer);
  debugln(F("PUMP ON CMD SENT"));
  delay(PUMP_MESSAGE_PROCESSING_MULTIPLIER*RECEIVED_MESSAGE_TIMEOUT);

  if(rs485.available() && rs485.find(HEADER))
  {
    size_t dataSize = rs485.readBytesUntil('$', buffer, 16);
    buffer[dataSize] = '\0';

    if(strcmp(buffer,OKCMD)==0)
    {
      debugln(F("PUMP CMD RESULT: OK"));
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


void stopPump()
{
  char buffer[17];
  sprintf(buffer,"%s%s$0$",HEADER,pumpCMD);
  rs485.print(buffer);
  debugln(F("PUMP OFF CMD SENT"));
  delay(PUMP_MESSAGE_PROCESSING_MULTIPLIER*RECEIVED_MESSAGE_TIMEOUT);

  if(rs485.available() && rs485.find(HEADER))
  {
    size_t dataSize = rs485.readBytesUntil('$', buffer, 16);
    buffer[dataSize] = '\0';

    if(strcmp(buffer,OKCMD)==0)
    {
      debugln(F("PUMP CMD RESULT: OK"));
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


void error(ErrorCode err, const char* message, bool printName)
{
  Serial.println(F("\n-----------------------------------------"));
  Serial.println(F("-----------------------------------------"));
  Serial.print(F("ERROR ")); if(printName){Serial.print(getErrorName(err));} Serial.print(F(": (")); Serial.print(err); Serial.print(")\n");
  Serial.println(message);
  Serial.println(F("-----------------------------------------"));
  Serial.println(F("-----------------------------------------\n"));

  if(err != NO_ERROR)
  {
    errorData.flags[err] = true;
    strcpy(errorData.message[err],message);
    #if !EEPROM_DONT_WRITE
      EEPROM.put(ERROR_REGISTER_ADDRESS,errorData);
    #endif
  }

  delay(5000);
  if(Serial.available())
  {
    serialEvent();
  }

  Serial.print(F("System is rebooting"));
  rebootLoop();
}


void error(ErrorCode err, const char* message)
{
  error(err, message, 1);
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

#if !MOCK_SENSORS
float getValveTemp()
{
  tempSensor.requestTemperatures(); // Request temp
  float temp = tempSensor.getTempCByIndex(0); // Obtain temp
  if(temp<MIN_ALLOWED_TEMP)
  {
    sprintf(errorStrBuff,"TEMP IS TOO LOW (%d)",(int)temp);
    error(ERROR_TEMP_SENSOR_INVALID_VALUE,errorStrBuff);
  }
  else if(temp<MAX_ALLOWED_TEMP)
  {
    sprintf(errorStrBuff,"TEMP IS TOO HIGH (%d)",(int)temp);
    error(ERROR_TEMP_SENSOR_INVALID_VALUE,errorStrBuff);
  }
  return temp;
}

double getValvePressure()
{
  int adcData = analogRead(A0);

  double pressureSensorVoltage = (adcData * 1.1)/1024;
  double pressureSensorCurrent = (pressureSensorVoltage*1000) / 51;

  if(!(pressureSensorCurrent >= MIN_ALLOWED_PRESSURE_SENSOR_CURRENT_mA && pressureSensorCurrent <= MAX_ALLOWED_PRESSURE_SENSOR_CURRENT_mA))
  {
    sprintf(errorStrBuff,"PRESSURE CURRENT (%dmA) IS OUTSIDE THE RANGE (%d, %d)mA",(int)pressureSensorCurrent, (int)MIN_ALLOWED_PRESSURE_SENSOR_CURRENT_mA, (int)MAX_ALLOWED_PRESSURE_SENSOR_CURRENT_mA);
    error(ERROR_PRESSURE_SENSOR_INVALID_VALUE,errorStrBuff);
  }

  return fmap(pressureSensorCurrent, PRESSURE_SENSOR_CURRENT_MIN_mA, PRESSURE_SENSOR_CURRENT_MAX_mA, PRESSURE_SENSOR_MIN_BAR, PRESSURE_SENSOR_MAX_BAR);
}

#endif


bool getValveTempIfNecessary()
{
  if(millis() - valveTempPMillis > VALVE_TEMP_GATHERING_PERIOD) // each sec update valve temp & compare
  {
    debug(F("Current valve temp:\t"));

    #if !MOCK_SENSORS
      valveTemp = getValveTemp();
    #endif
    
    debug(valveTemp); debug(F("\tDesired temp:\t"));debugln(desiredTemp);

    valveTempPMillis = millis();
    return true;
  }
  return false;
}


void getHeaterTemp(bool messages = true)
{
  char buffer[17];
  sprintf(buffer,"%s%s$",HEADER,tempCMD);
  rs485.print(buffer);
  if(messages)
  {
    debugln(F("HEATER TEMP REQUEST CMD SENT"));
  }
  

  delay(TEMP_MESSAGE_PROCESSING_MULTIPLIER*RECEIVED_MESSAGE_TIMEOUT);

  if(rs485.available() && rs485.find(HEADER))
  {
    size_t dataSize = rs485.readBytesUntil('$', buffer, 16);
    buffer[dataSize] = '\0';

    if(strcmp(buffer, tempCMD) == 0)
    {
      size_t dataSize = rs485.readBytesUntil('$', buffer, 16);
      buffer[dataSize] = '\0';
      if(messages)
      {
        debug(F("Heater Temp updated from ")); debug(heaterTemp);
      }
      heaterTemp = atoi(buffer);
      if(messages)
      {
        debug(F(" to ")); debugln(heaterTemp);
        debug(F("Desired Temp updated from ")); debug(desiredTemp);
      }
      
      if(status == ServingWater)
      {
        desiredTemp = round(heaterTemp*PIPE_HEAT_TRANSPORT_EFFICIENCY*COLD_WATER_TEMPERATURE_MULTIPLIER);
      }
      else
      {
        desiredTemp = round(heaterTemp*PIPE_HEAT_TRANSPORT_EFFICIENCY);
      }
      
      if(messages)
      {
        debug(F(" to ")); debugln(desiredTemp);
      }
    }
    else 
    {
      sprintf(errorStrBuff,"Expected 'TEMP';\tReceived '%s'",buffer);
      error(ERROR_RS485_UNEXPECTED_MESSAGE, errorStrBuff);
    }
  }
  else 
  {
    error(ERROR_RS485_NO_RESPONSE,"Timeout receiving TEMP CMD ANSWER");
  }
}


bool getHeaterTempIfNecessary()
{
  if(millis() - heaterTempPMillis > HEATER_TEMP_GATHERING_PERIOD) // each 10 secs update heater temp
  {
    getHeaterTemp();

    heaterTempPMillis = millis();
    return true;
  }
  return false;
}


bool isTriggerActive()
{
  #if !MOCK_SENSORS
    double pressure = getValvePressure();
    return pressure < WATER_MIN_NORMAL_PRESSURE_BAR;
  #else
    return triggerVal;
  #endif
}


void printSensorsInfo()
{
  Serial.println(F("\n-----------------------------------------"  ));
  Serial.println(F(  "Sensor list:\n"));
  #if MOCK_SENSORS
    Serial.println(F("Valve pressure sensor: MOCKED"));
    Serial.print(F(  "Valve temperature sensor: MOCKED to ")); Serial.print(valveTemp);Serial.println(F("ºC"));
  #else
    Serial.print(F(  "Valve pressure sensor: ")); Serial.print(getValvePressure());Serial.println(F("BAR"));
    Serial.print(F(  "Valve temperature sensor: ")); Serial.print(getValveTemp());Serial.println(F("ºC"));
  #endif
  getHeaterTemp(false);
  Serial.print(F(    "Heater temperature sensor: ")); Serial.print(heaterTemp);Serial.println(F("ºC"));
  Serial.print(F(    "Desired temperature: ")); Serial.print(desiredTemp);Serial.println(F("ºC"));
  
  Serial.println(F(  "-----------------------------------------\n"));;
}


void serialEvent()
{
  char buffer[17];
  size_t len = Serial.readBytesUntil('\n',buffer,16);
  buffer[len] = '\0';

  Serial.print(F("User input: ")); Serial.println(buffer);

  if(strcmp(buffer,"help") == 0)
  {
    #if MOCK_SENSORS
      Serial.println(F("\nType 'enable' or 'disable' to enable or disable the system; 'clear' to invalidate the Error Register; 'sensors' to print all the sensors current value; 'startpump' or 'stoppump' to manually start or stop the pump; or 'errorlist' to print the error list\nPress e or d to enable or disable trigger\nor send a number to incorporate it as valve temp\n"));
    #else
      Serial.println(F("\nType 'enable' or 'disable' to enable or disable the system; 'clear' to invalidate the Error Register; 'sensors' to print all the sensors current value; 'startpump' or 'stoppump' to manually start or stop the pump; or 'errorlist' to print the error list\n"));
    #endif
  }
  else if(strcmp(buffer,"clear") == 0)
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
  else if(strcmp(buffer,"enable") == 0)
  {
    EEPROM.put(ENABLE_REGISTER_ADDRESS, true);
    error(NO_ERROR,"Rebooting to enable the system");
  }
  else if(strcmp(buffer,"disable") == 0)
  {
    EEPROM.put(ENABLE_REGISTER_ADDRESS, false);
    error(NO_ERROR,"Rebooting to disable the system");
  }
  else if(strcmp(buffer,"sensors") == 0)
  {
    printSensorsInfo();
  }
  else if(strcmp(buffer,"startpump") == 0)
  {
    startPump();
  }
  else if(strcmp(buffer,"stoppump") == 0)
  {
    stopPump();
  }
  

  #if MOCK_SENSORS
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
  #endif
}

#if !DISABLE_WATCHDOGS
void resetWatchdogs()
{
  char buffer[17];
  if(millis() - watchdogsPMillis > WATCHDOG_RESET_PERIOD) // Reset both watchdogs once in a WATCHDOG_RESET_PERIOD
  {
    wdt_reset();
    watchdogsPMillis = millis();

    sprintf(buffer,"%s%s$",HEADER,WTDRSTCMD);

    #if DEBUGWATCHDOG
    debug(F("Sending Watchdog Reset CMD: ")); debugln(buffer);
    #endif

    rs485.print(buffer);

    delay(WDT_RST_MESSAGE_PROCESSING_MULTIPLIER*RECEIVED_MESSAGE_TIMEOUT);

    if(rs485.available() && rs485.find(HEADER))
    {
      size_t dataSize = rs485.readBytesUntil('$', buffer, 16);
      buffer[dataSize] = '\0';

      if(strcmp(buffer,OKCMD)!=0)
      {
        sprintf(errorStrBuff,"Expected 'OK';\tReceived '%s'",buffer);
        error(ERROR_RS485_UNEXPECTED_MESSAGE, errorStrBuff);
      }
      #if DEBUGWATCHDOG
      else 
      {
        debugln(F("WTD_RST CMD RESULT: OK"));
      }
      #endif
    }
    else if (SYSTEM_ENABLED)
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

  while(!connected && millis() - connectionPMillis < INIT_CONNECTION_TIMEOUT)
  {
    if(Serial.available())
    {
      serialEvent();
    }

    if(millis() - watchdogsPMillis > WATCHDOG_RESET_PERIOD) // Reset both watchdogs once in a WATCHDOG_RESET_PERIOD
    {
      wdt_reset();
      watchdogsPMillis = millis();

      sprintf(buffer,"%s%s$",HEADER,WTDRSTCMD);

      #if DEBUGWATCHDOG
      debug(F("Sending Watchdog Reset CMD: ")); debugln(buffer);
      #endif

      rs485.print(buffer);

      delay(WDT_RST_MESSAGE_PROCESSING_MULTIPLIER*RECEIVED_MESSAGE_TIMEOUT);

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


#if !MOCK_SENSORS
void post()
{
  getValvePressure();
  getValveTemp();
}
#endif


void setup() 
{
  wdt_disable(); /* Disable the watchdog and wait for more than 8 seconds */
  #if !DISABLE_WATCHDOGS
    delay(10000); /* Done so that the Arduino doesn't keep resetting infinitely in case of wrong configuration */
    wdt_enable(WDTO_8S); /* Enable the watchdog with a timeout of 8 seconds */
  #endif

  pinMode(VALVE_RELAY_PIN,OUTPUT);
  digitalWrite(VALVE_RELAY_PIN,RELAY_DISABLED); // TODO check valve feedback pin

  Serial.begin(9600); // Used for debug purposes
  delay(3000);
  Serial.println(F("\n------------------------------------------"  ));
  Serial.println(F(  "|                SHWRS-VS                |"  ));
  Serial.println(F(  "------------------------------------------\n"));
  wdt_reset();

  EEPROM.get(ENABLE_REGISTER_ADDRESS, SYSTEM_ENABLED);
  loadErrorRegister();

  #if MOCK_SENSORS
    Serial.println(F("WARNING: SENSOR MOCKING ENABLED"));
    Serial.println(F("\nType 'enable' or 'disable' to enable or disable the system; 'clear' to invalidate the Error Register; 'sensors' to print all the sensors current value; 'startpump' or 'stoppump' to manually start or stop the pump; or 'errorlist' to print the error list\nPress e or d to enable or disable trigger\nor send a number to incorporate it as valve temp\n"));
  #else
    Serial.println(F("\nType 'enable' or 'disable' to enable or disable the system; 'clear' to invalidate the Error Register; 'sensors' to print all the sensors current value; 'startpump' or 'stoppump' to manually start or stop the pump; or 'errorlist' to print the error list\n"));
  #endif

  Serial.println(F("Starting..."));

  if(!SYSTEM_ENABLED)
  {
    Serial.println(F("\n------------- SYSTEM DISABLED ------------\n"  ));
  }
  else
  {
    Serial.println(F("\n------------- SYSTEM ENABLED -------------\n"  ));

    #if !MOCK_SENSORS
      analogReference(INTERNAL);
      tempSensor.begin();

      post();
    #endif

    rs485.begin(9600,RECEIVED_MESSAGE_TIMEOUT); // first argument is serial baud rate & second one is serial input timeout (to enable the use of the find function)
    delay(1000);
    connectToHeater();

    Serial.println(F("Valve side system successfully connected!!!"));
  }

  wdt_reset();
  Serial.println(F("Start completed"));
    
  delay(1000);
}


void loop() 
{
  if(SYSTEM_ENABLED)
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
          delay(PUMP_MESSAGE_PROCESSING_MULTIPLIER*RECEIVED_MESSAGE_TIMEOUT);

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
          delay(PUMP_MESSAGE_PROCESSING_MULTIPLIER*RECEIVED_MESSAGE_TIMEOUT);

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
              sprintf(errorStrBuff,"Expected 'OK';\tReceived '%s'",buffer);
              error(ERROR_RS485_UNEXPECTED_MESSAGE, errorStrBuff);
            }
          }
          else 
          {
            error(ERROR_RS485_NO_RESPONSE,"Timeout receiving PUMP CMD ANSWER");
          }

          digitalWrite(VALVE_RELAY_PIN, RELAY_ENABLED);
          changeStatus(ServingWater);
        }

      break;

      case ServingWater:

        getHeaterTempIfNecessary();

        if(getValveTempIfNecessary() && !isTriggerActive() && valveTemp < desiredTemp)
        {
          digitalWrite(VALVE_RELAY_PIN, RELAY_DISABLED);
          changeStatus(WaitingCold);
        }

      break;
    }
  }

  #if !DISABLE_WATCHDOGS
  resetWatchdogs();
  #endif
}
