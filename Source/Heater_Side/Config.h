#ifndef CONFIG_H
#define CONFIG_H

#define DEBUG 0
#define DEBUGWATCHDOG 0
#define DISABLE_WATCHDOGS 0
#define MOCK_SENSORS 1

#define ERROR_REGISTER_ADDRESS 0
#define EEPROM_SIZE 1024
#define EEPROM_ERROR_MAGIC_STR "MES"
#define EEPROM_DONT_WRITE 1

#define RELAY_ENABLED 1
#define RELAY_DISABLED !RELAY_ENABLED

const long INIT_CONNECTION_TIMEOUT = 120000; // 2 min
const int WATCHDOG_RESET_PERIOD = 6000; // 7.5 s
const int RECEIVED_MESSAGE_TIMEOUT = 100; // 100 ms
const long AUTO_DISABLE_PUMP_TIMEOUT = 120000; // 2 min
const int HEATER_TEMP_GATHERING_PERIOD = 10000; // 10 s
const int VALVE_TEMP_GATHERING_PERIOD = 1000; // 1 s
const float PIPE_HEAT_TRANSPORT_EFFICIENCY = 0.9; // 90% of the temperature at the heater should get into the valve
const float COLD_WATER_TEMPERATURE_MULTIPLIER = 0.9; // The temperature at wich the systems closes the valve is desiredTemp*COLD_WATER_TEMPERATURE_MULTIPLIER

const double PRESSURE_SENSOR_MIN_BAR = 0.0;
const double PRESSURE_SENSOR_MAX_BAR = 10.0;
const double WATER_MIN_NORMAL_PRESSURE_BAR = 5.0;
const double PRESSURE_SENSOR_CURRENT_MIN_mA = 4.0;
const double PRESSURE_SENSOR_CURRENT_MAX_mA = 20.0;

const int PUMP_MESSAGE_PROCESSING_MULTIPLIER = 2;
const int TEMP_MESSAGE_PROCESSING_MULTIPLIER = 2;
const int WDT_RST_MESSAGE_PROCESSING_MULTIPLIER = 2;

// Command structure: "SHWRS_{CMD$}[ARG$]*"
char HEADER[] = "SHWRS_"; // This string is prepended to the message and used to discard leftover bytes from previous messages
const char pumpCMD[] = "PUMP";
const char tempCMD[] = "TEMP";
const char OKCMD[] = "OK";
const char WTDRSTCMD[] = "WTD_RST";


#if DEBUG
  #define debug(...) do {Serial.print(__VA_ARGS__); Serial.flush();} while(0)
  #define debugln(...) do {Serial.println(__VA_ARGS__); Serial.flush();} while(0)
#else
  #define debug(...) 
  #define debugln(...) 
#endif


void rebootLoop()
{
  #if DISABLE_WATCHDOGS
    wdt_enable(WDTO_8S); /* Enable the watchdog with a timeout of 8 seconds */
  #endif

  while (1) 
  {
    Serial.print('.');
    delay(500);
  }
}


#endif
