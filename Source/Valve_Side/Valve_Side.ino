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

const uint8_t RECEIVER_ENABLE_PIN = 5;  // HIGH = Driver / LOW = Receptor
const uint8_t DRIVE_ENABLE_PIN = 4;  // HIGH = Driver / LOW = Receptor
const uint8_t RX_PIN = 6; // Serial data in pin
const uint8_t TX_PIN = 3; // Serial data out pin

const uint8_t VALVE_RELAY_PIN = 2;
// const uint8_t VALVE_FEEDBACK_PIN = 7;

const uint8_t RED_LED_PIN = 11;
const uint8_t GREEN_LED_PIN = 9;
const uint8_t BLUE_LED_PIN = 10;

#if USE_BUTTON

const uint8_t BUTTON_PIN = 8;

#endif

bool SYSTEM_ENABLED = false;
bool MINIMAL_WORKING_STATE_ENABLED = false;

#if !MOCK_SENSORS
const uint8_t PRESSURE_SENSOR = A0;

const uint8_t TEMP_SENSOR = 12;


OneWire ourWire(TEMP_SENSOR); // Create Onewire instance for temp sensor
DallasTemperature tempSensor(&ourWire); // Create temp sensor instance
#endif

MAX_RS485 rs485(RX_PIN, TX_PIN, RECEIVER_ENABLE_PIN, DRIVE_ENABLE_PIN); // Create rs485 instance


typedef enum {MinimalWorkingState, WaitingCold, DrivingWater, ServingWater} Status;
#define changeStatus(newStatus) do {debug(F("Changing Status from ")); debug(statusToString(status)); status = newStatus; writeColor(statusToColor(status)); debug(F(" to ")); debugln(statusToString(status));} while(0)

typedef enum {NO_ERROR = 0, ERROR_INTERNAL, ERROR_TEMP_SENSOR_INVALID_VALUE, ERROR_PRESSURE_SENSOR_INVALID_VALUE, ERROR_CONNECTION_NOT_ESTABLISHED, ERROR_RS485_NO_RESPONSE, ERROR_RS485_UNEXPECTED_MESSAGE,              ENUM_LEN} ErrorCode;
#define NUM_OF_ERROR_TYPES ErrorCode::ENUM_LEN
#define ERROR_MESSAGE_SIZE (EEPROM_USED_SIZE-((sizeof(char)*3)*NUM_OF_ERROR_TYPES)-(NUM_OF_ERROR_TYPES * sizeof(bool)))/(NUM_OF_ERROR_TYPES)
#define errorDataAddress(errorCode) (ERROR_REGISTER_ADDRESS + (sizeof(ErrorData) * errorCode))

typedef enum {Black, Red, Green, Blue, Yellow, Purple, Cyan, White, Gray} Color;
#define BOOT_COLOR Green
#define RESTART_COLOR White
#define USER_ACK_COLOR Cyan
#define MINIMAL_WORKING_STATE_COLOR Yellow
#define WAITING_COLD_COLOR Gray
#define DRIVING_WATER_COLOR Blue
#define SERVING_WATER_COLOR Red
#define SYSTEM_DISABLED_COLOR Black

#if USE_BUTTON
typedef enum {NO_PULSE = 0, SHORT_PULSE, LONG_PULSE} ButtonStatus;
#endif


typedef struct 
{
  bool flag;
  char message[ERROR_MESSAGE_SIZE];
  char magic[sizeof(EEPROM_ERROR_MAGIC_STR)];
} 
ErrorData;


int heaterTemp = 0;
int desiredTemp = INT16_MAX;
int fadeMinTemp = 0;

unsigned long heaterTempPMillis = 0;
unsigned long valveTempPMillis = 0;
unsigned long watchdogsPMillis = 0;
Status status = WaitingCold;

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


void printErrorData(bool printAll = false)
{
  Serial.println(F("\n-----------------------------------------"));
  Serial.println(F("Error list:\n"));
  for(uint8_t i = NO_ERROR + 1; i<NUM_OF_ERROR_TYPES;i++)
  {
    ErrorData data;
    EEPROM.get(errorDataAddress(i), data);
    if(strcmp(data.magic,EEPROM_ERROR_MAGIC_STR) == 0)
    {
      if(printAll || data.flag)
      {
        Serial.print(getErrorName((ErrorCode)i)); Serial.print(F(":\t"));
        Serial.println(data.flag?data.message:"OK");
      }
    }
    else
    {
      Serial.println(F("-----------------------------------------"));
      Serial.print(F("ErrorData of error: "));Serial.print(getErrorName((ErrorCode)i)); Serial.println(F(" is not valid\nRestoring it...\t"));

      memset(&data,'\0',sizeof(ErrorData));
      strcpy(data.magic,EEPROM_ERROR_MAGIC_STR);

      EEPROM.put(errorDataAddress(i), data);

      Serial.println(F("DONE"));
      Serial.println(F("-----------------------------------------\n"));
    }
  }
  Serial.println(F("-----------------------------------------\n"));
}


void invalidateErrorData()
{
  for(uint8_t i = NO_ERROR + 1; i<NUM_OF_ERROR_TYPES;i++)
  {
    ErrorData data;
    EEPROM.get(errorDataAddress(i), data);
    data.magic[0]++;
    EEPROM.put(errorDataAddress(i), data);
  }
}


void writeColor(Color color)
{
  switch(color)
  {
    case Black:
      writeColor(0,0,0);
      break;
    case Red:
      writeColor(255,0,0);
      break;
    case Green:
      writeColor(0,255,0);
      break;
    case Blue:
      writeColor(0,0,255);
      break;
    case Yellow:
      writeColor(255,255,0);
      break;
    case Purple:
      writeColor(255,0,255);
      break;
    case Cyan:
      writeColor(0,255,255);
      break;
    case White:
      writeColor(255,255,255);
      break;
    case Gray:
      writeColor(1,1,1);
      break;
  }
}


void writeColor(uint8_t r, uint8_t g, uint8_t b)
{
  #if LED_ENABLED
    analogWrite(RED_LED_PIN, r);
    analogWrite(GREEN_LED_PIN, g);
    analogWrite(BLUE_LED_PIN, b);
  #else
    analogWrite(RED_LED_PIN, 255-r);
    analogWrite(GREEN_LED_PIN, 255-g);
    analogWrite(BLUE_LED_PIN, 255-b);
  #endif
}


void updateColorToProgress(uint8_t progress)
{
  writeColor(progress, 0, 255-progress);
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
    error(ERROR_RS485_NO_RESPONSE, F("Timeout receiving PUMP CMD ANSWER"));
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
    error(ERROR_RS485_NO_RESPONSE, F("Timeout receiving PUMP CMD ANSWER"));
  }
}


void error(ErrorCode err, const char* message, bool printName)
{
  wdt_reset();
  Serial.println(F("\n-----------------------------------------"));
  Serial.println(F("-----------------------------------------"));
  Serial.print(F("ERROR ")); if(printName){Serial.print(getErrorName(err));} Serial.print(F(": (")); Serial.print(err); Serial.print(")\n");
  Serial.println(message);
  Serial.println(F("-----------------------------------------"));
  Serial.println(F("-----------------------------------------\n"));

  if(err != NO_ERROR)
  {
    ErrorData errorData;
    EEPROM.get(errorDataAddress(err), errorData);
    
    errorData.flag = true;
    strcpy(errorData.message, message);

    #if !EEPROM_DONT_WRITE
      EEPROM.put(errorDataAddress(err), errorData);
    #endif
  }

  delay(3000);
  if(Serial.available())
  {
    serialEvent();
  }

#if USE_BUTTON
  if((SYSTEM_ENABLED && !MINIMAL_WORKING_STATE_ENABLED) || err == NO_ERROR)
#else
  if(SYSTEM_ENABLED || err == NO_ERROR)
#endif
  {
  #if USE_BUTTON
    if(err != NO_ERROR)
    {
      debug(F("Changing MINIMAL_WORKING_STATE_ENABLED from ")); debug(MINIMAL_WORKING_STATE_ENABLED);debug(F(" to ")); debugln(true);
      EEPROM.put(MINIMAL_WORKING_MODE_REGISTER_ADDRESS, true);
      writeColor(MINIMAL_WORKING_STATE_COLOR);
    }
  #endif
    Serial.print(F("System is rebooting"));
    writeColor(RESTART_COLOR);
    rebootLoop();
  }
}


void error(ErrorCode err, const char* message)
{
  error(err, message, 1);
}


void error(ErrorCode err, const __FlashStringHelper* message)
{
  char buff[ERROR_MESSAGE_SIZE];

  PGM_P p = reinterpret_cast<PGM_P>(message);
  unsigned int i = 0;
  while (1) 
  {
    unsigned char c = pgm_read_byte(p++);

    buff[i++] = c;
    if (c == 0)
    {
      break;
    }
  }

  error(err, buff, 1);
}

const char* statusToString(Status status)
{
  switch (status) 
  {
    case MinimalWorkingState:
      return "MinimalWorkingState";
    case WaitingCold:
      return "WaitingCold";
    case DrivingWater:
      return "DrivingWater";
    case ServingWater:
      return "ServingWater";
  }
}


Color statusToColor(Status status)
{
  switch (status) 
  {
    case MinimalWorkingState:
      return MINIMAL_WORKING_STATE_COLOR;
    case WaitingCold:
      return WAITING_COLD_COLOR;
    case DrivingWater:
      return DRIVING_WATER_COLOR;
    case ServingWater:
      return SERVING_WATER_COLOR;
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
  else if(temp>MAX_ALLOWED_TEMP)
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


void getHeaterTemp(bool ignoreErrors = false)
{
  char buffer[17];
  sprintf(buffer,"%s%s$",HEADER,tempCMD);
  rs485.print(buffer);
  if(!ignoreErrors)
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
      if(!ignoreErrors)
      {
        debug(F("Heater Temp updated from ")); debug(heaterTemp);
      }
      heaterTemp = atoi(buffer);
      if(!ignoreErrors)
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
      
      if(!ignoreErrors)
      {
        debug(F(" to ")); debugln(desiredTemp);
      }
    }
    else 
    {
      if(!ignoreErrors)
      {
        sprintf(errorStrBuff,"Expected 'TEMP';\tReceived '%s'",buffer);
        error(ERROR_RS485_UNEXPECTED_MESSAGE, errorStrBuff);
      }
    }
  }
  else 
  {
    if(!ignoreErrors)
    {
      error(ERROR_RS485_NO_RESPONSE, F("Timeout receiving TEMP CMD ANSWER"));
    }
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


const char* formattedTime(long time, char* buff)
{
  if(time<0)
  {
    strcpy(buff, "None");
    return buff;
  }

  long ms = time%1000;
  long s = time/1000;
  long m = s/60;
  long h = m/60;
  m = m%60;
  s = s%60;

  sprintf(buff,"%lih %lim %lis %lims (%lims)",h,m,s,ms,time);
  return buff;
}


void printSensorsInfo()
{
  char buff[64];
  Serial.println(F("\n-----------------------------------------"  ));
  Serial.println(F(  "Sensor list:\n"));
  Serial.print(F(    "Remaining time until next restart: ")); Serial.println(formattedTime(SYSTEM_RESET_PERIOD - millis(), buff));
  Serial.print(F(    "System Enabled: ")); Serial.println(SYSTEM_ENABLED?"True":"False");
  Serial.print(F(    "Minimal Working State Enabled: ")); Serial.println(MINIMAL_WORKING_STATE_ENABLED?"True":"False");
  #if MOCK_SENSORS
    Serial.println(F("Valve pressure sensor: MOCKED"));
    Serial.print(F(  "Valve temperature sensor: MOCKED to ")); Serial.print(valveTemp);Serial.println(F("ºC"));
  #else
    Serial.print(F(  "Valve pressure sensor: ")); Serial.print(getValvePressure());Serial.println(F("BAR"));
    wdt_reset();
    Serial.print(F(  "Valve temperature sensor: ")); Serial.print(getValveTemp());Serial.println(F("ºC"));
  #endif
  wdt_reset();
  getHeaterTemp(true);
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
    Serial.println(F("\nType 'enable' or 'disable' to enable or disable the system;\n'setminimal' or 'clearminimal' to set or clear the minimal mode (valve bypass);\n'clear' to invalidate the Error Register;\n'sensors' to print all the sensors current value;\n'startpump' or 'stoppump' to manually start or stop the pump;\n'openvalve' or 'closevalve' to manually open or close the valve;\n'errorlist' to print the error list;\n"));
    #if MOCK_SENSORS
      Serial.println(F("\nPress e or d to enable or disable trigger\nsend a number to incorporate it as valve temp\n"));
    #endif
  }
  else if(strcmp(buffer,"clear") == 0)
  {
    invalidateErrorData();
    error(NO_ERROR, F("Error Register Invalidated"));
  }
  else if(strcmp(buffer,"errorlist") == 0)
  {
    printErrorData(true);
  }
  else if(strcmp(buffer,"enable") == 0)
  {
    EEPROM.put(ENABLE_REGISTER_ADDRESS, true);
    error(NO_ERROR, F("Rebooting to enable the system"));
  }
  else if(strcmp(buffer,"disable") == 0)
  {
    EEPROM.put(ENABLE_REGISTER_ADDRESS, false);
    error(NO_ERROR, F("Rebooting to disable the system"));
  }
  else if(strcmp(buffer,"setminimal") == 0)
  {
    EEPROM.put(MINIMAL_WORKING_MODE_REGISTER_ADDRESS, !MINIMAL_WORKING_STATE_ENABLED);
    error(NO_ERROR, F("Rebooting to enable the minimal mode"));
  }
  else if(strcmp(buffer,"clearminimal") == 0)
  {
    EEPROM.put(MINIMAL_WORKING_MODE_REGISTER_ADDRESS, !MINIMAL_WORKING_STATE_ENABLED);
    error(NO_ERROR, F("Rebooting to disable the minimal mode"));
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
  else if(strcmp(buffer,"openvalve") == 0)
  {
    digitalWrite(VALVE_RELAY_PIN, RELAY_ENABLED);
  }
  else if(strcmp(buffer,"closevalve") == 0)
  {
    digitalWrite(VALVE_RELAY_PIN, RELAY_DISABLED);
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
      error(ERROR_RS485_NO_RESPONSE, F("Timeout receiving WTD_RST CMD ANSWER"));
    }
  }
}
#endif


void connectToHeater(bool ignoreErrors = false)
{
  #if !DISABLE_WATCHDOGS

  char buffer[17];
  unsigned long connectionPMillis = millis();
  bool connected = false;

  long timeout = (SYSTEM_ENABLED?INIT_CONNECTION_TIMEOUT:INIT_CONNECTION_TIMEOUT_IF_DISABLED);

  char buff[32];
  debug(F("Connecting to heater MCU... Max time: ")); debugln(formattedTime(timeout,buff));

  while(!connected && (millis() - connectionPMillis) < timeout)
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
    if(ignoreErrors)
    {
      debugln(F("Cannot connect to heater MCU"));
    }
    else
    {
      error(ERROR_CONNECTION_NOT_ESTABLISHED, F("ERROR: Cannot connect to heater MCU"));
    }
  }
  else
  {
    debug(F("Connected to heater MCU in ")); debugln(formattedTime(millis() - watchdogsPMillis,buff));
  }

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


void checkResetTime()
{
  #if ENABLE_AUTO_RESTART
    if((status == WaitingCold || status == MinimalWorkingState) && (millis() > SYSTEM_RESET_PERIOD))
    {
      error(NO_ERROR, F("INFO: Restarting the system due to SYSTEM_RESET_PERIOD timeout"));
    }
  #endif
}


#if USE_BUTTON
ButtonStatus readButton()
{
  ButtonStatus res = NO_PULSE;
  if(!digitalRead(BUTTON_PIN))
  {
    unsigned long time = millis();
    delay(10);
    while(!digitalRead(BUTTON_PIN) && millis() - time < BUTTON_LONG_PRESSED_TIME)
    {
      #if !DISABLE_WATCHDOGS
        resetWatchdogs();
      #endif
    }

    time = millis() - time;

    if(time >= BUTTON_LONG_PRESSED_TIME)
    {
      debug(F("Long press detected: ")); debugln(time);
      res = LONG_PULSE;
    }
    else if (time >= BUTTON_SHORT_PRESSED_MIN_TIME)
    {
      debug(F("Short press detected: ")); debugln(time);
      res = SHORT_PULSE;
    }
  }
  return res;
}
#endif


void setup() 
{
  wdt_disable(); /* Disable the watchdog and wait for more than 8 seconds */

  pinMode(VALVE_RELAY_PIN,OUTPUT);
  EEPROM.get(ENABLE_REGISTER_ADDRESS, SYSTEM_ENABLED);

#if USE_BUTTON
  EEPROM.get(MINIMAL_WORKING_MODE_REGISTER_ADDRESS, MINIMAL_WORKING_STATE_ENABLED);
  if(SYSTEM_ENABLED && MINIMAL_WORKING_STATE_ENABLED)
  {
    digitalWrite(VALVE_RELAY_PIN, RELAY_ENABLED);
  }
  else
  {
    digitalWrite(VALVE_RELAY_PIN, RELAY_DISABLED);
  }
#else
  digitalWrite(VALVE_RELAY_PIN, RELAY_DISABLED);
#endif

  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);

#if TEST_COLORS
  writeColor(Red);
  delay(500);
  writeColor(Green);
  delay(500);
  writeColor(Blue);
  delay(500);
  writeColor(Black);
  delay(1000);
#endif

  writeColor(RESTART_COLOR);

  #if !DISABLE_WATCHDOGS
    delay(10000); /* Done so that the Arduino doesn't keep resetting infinitely in case of wrong configuration */
    wdt_enable(WDTO_8S); /* Enable the watchdog with a timeout of 8 seconds */
  #endif

  writeColor(BOOT_COLOR);

  Serial.begin(9600); // Used for debug purposes
  delay(3000);
  Serial.println(F("\n------------------------------------------"  ));
  Serial.println(F(  "|                SHWRS-VS                |"  ));
  Serial.println(F(  "|                 " VS "                 |"  ));
  Serial.println(F(  "------------------------------------------\n"));
  wdt_reset();

  printErrorData();

  Serial.println(F("Starting..."));

  if(MINIMAL_WORKING_STATE_ENABLED)
  {
    Serial.println(F("\nWARNING: SYSTEM UNDER MINIMAL CONDITIONS\n\n"));
  }

  rs485.begin(9600,RECEIVED_MESSAGE_TIMEOUT); // first argument is serial baud rate & second one is serial input timeout (to enable the use of the find function)
  delay(1000);

  #if !MOCK_SENSORS
    analogReference(INTERNAL);
    tempSensor.begin();

    post();
  #endif
    
  if(!SYSTEM_ENABLED)
  {
    Serial.println(F("\n------------- SYSTEM DISABLED ------------\n"  ));

    connectToHeater(true);
  }
  else
  {
    Serial.println(F("\n------------- SYSTEM ENABLED -------------\n"  ));
    connectToHeater();
  }

  wdt_reset();
  Serial.println(F("Start completed"));

  delay(500);
  printSensorsInfo();
  wdt_reset();

  #if MOCK_SENSORS
    Serial.println(F("WARNING: SENSOR MOCKING ENABLED"));
  #endif
  Serial.println(F("\nType 'enable' or 'disable' to enable or disable the system;\n'setminimal' or 'clearminimal' to set or clear the minimal mode (valve bypass);\n'clear' to invalidate the Error Register;\n'sensors' to print all the sensors current value;\n'startpump' or 'stoppump' to manually start or stop the pump;\n'openvalve' or 'closevalve' to manually open or close the valve;\n'errorlist' to print the error list;\n"));
  #if MOCK_SENSORS
    Serial.println(F("\nPress e or d to enable or disable trigger\nsend a number to incorporate it as valve temp\n"));
  #endif
    
  delay(1000);

#if USE_BUTTON
  if(MINIMAL_WORKING_STATE_ENABLED)
  {
    changeStatus(MinimalWorkingState);
  }
  else
#endif
  {
    changeStatus(WaitingCold);
  }

  if(!SYSTEM_ENABLED)
  {
    writeColor(SYSTEM_DISABLED_COLOR);
  }
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
              valveTemp = getValveTemp();
              fadeMinTemp = valveTemp;
              for(uint8_t i = 0; i<6;i++)
              {
                delay(1000);
                #if !DISABLE_WATCHDOGS
                  resetWatchdogs();
                #endif
              }
              getHeaterTemp();
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
            error(ERROR_RS485_NO_RESPONSE, F("Timeout receiving PUMP CMD ANSWER"));
          }
        }

      break;

      case DrivingWater:

        getHeaterTempIfNecessary();

        if(getValveTempIfNecessary())
        {
          long progress = map(valveTemp, fadeMinTemp, desiredTemp, 0, MAX_PROGRESS_VALUE);
          debug(F("valveTemp: ")); debug(valveTemp); debug(F("\tfadeMinTemp: ")); debug(fadeMinTemp); debug(F("\tdesiredTemp: ")); debug(desiredTemp); debug(F("\tProgress: ")); debug((progress*100)/MAX_PROGRESS_VALUE); debug(F(" (")); debug(progress); debugln(F(")"));
          #if COLOR_PROGRESS_FEEDBACK
            updateColorToProgress(progress);
          #endif
          if(valveTemp >= desiredTemp) // compare temps & change phase
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
              error(ERROR_RS485_NO_RESPONSE, F("Timeout receiving PUMP CMD ANSWER"));
            }

            digitalWrite(VALVE_RELAY_PIN, RELAY_ENABLED);
            changeStatus(ServingWater);
            getHeaterTempIfNecessary();
          }
        }
        

      break;

      case ServingWater:

        if(getValveTempIfNecessary() && !isTriggerActive() && valveTemp < desiredTemp)
        {
          digitalWrite(VALVE_RELAY_PIN, RELAY_DISABLED);
          changeStatus(WaitingCold);
        }

      break;

      #if USE_BUTTON
      case MinimalWorkingState:
      {
        static int step = 255;
        static bool up = false;

        if(up)
        {
          if(step >= 254)
          {
            up = false;
          }
          step++;
        }
        else
        {
          if(step <= 1)
          {
            up = true;
            delay(500);
          }
          step--;
        }

        writeColor(step, step, 0);
        delay(ANIMATION_FRAME_DELAY);
      }
      break;
      #endif
    }
  }

  #if USE_BUTTON
  switch(readButton())
  {
    case SHORT_PULSE:
      debug(F("Changing MINIMAL_WORKING_STATE_ENABLED from ")); debug(MINIMAL_WORKING_STATE_ENABLED);debug(F(" to ")); debugln(!MINIMAL_WORKING_STATE_ENABLED);
      EEPROM.put(MINIMAL_WORKING_MODE_REGISTER_ADDRESS, !MINIMAL_WORKING_STATE_ENABLED);
      writeColor(USER_ACK_COLOR);
      error(NO_ERROR, F("Rebooting to apply new Minimal Working State"));
      
      break;
    case LONG_PULSE:
      debug(F("Changing SYSTEM_ENABLED from ")); debug(SYSTEM_ENABLED);debug(F(" to ")); debugln(!SYSTEM_ENABLED);
      EEPROM.put(ENABLE_REGISTER_ADDRESS, !SYSTEM_ENABLED);
      writeColor(USER_ACK_COLOR);
      error(NO_ERROR, F("Rebooting to apply new System Enable State"));
      break;
  }
  #endif

  #if !DISABLE_WATCHDOGS
  resetWatchdogs();
  checkResetTime();
  #endif
}
