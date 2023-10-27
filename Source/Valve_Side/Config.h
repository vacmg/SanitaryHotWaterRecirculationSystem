#ifndef CONFIG_H
#define CONFIG_H

#define DEBUG 0
#define DEBUGWATCHDOG 0
#define DISABLE_WATCHDOGS 0

#define ERROR_REGISTER_ADDRESS 0
#define EEPROM_SIZE 1024
#define EEPROM_ERROR_MAGIC_STR "MES"
#define EEPROM_DONT_WRITE 1

#define RELAY_ENABLED 1
#define RELAY_DISABLED !RELAY_ENABLED

const long initConnectionTimeout = 120000; // 2 min
const int watchdogResetPeriod = 6000; // 7.5 s
const int receivedMessageTimeout = 100; // 100 ms
const long autoDisablePumpTimeout = 120000; // 2 min
const int heaterTempGatheringPeriod = 10000; // 10 s
const int valveTempGatheringPeriod = 1000; // 1 s
const float pipeHeatTransportEfficiency = 0.9; // 90% of the temperature at the heater should get into the valve
const float coldWaterTemperatureMultiplier = 0.9; // The temperature at wich the systems closes the valve is desiredTemp*coldWaterTemperatureMultiplier

const int pumpMessageProcessingMultiplier = 2;
const int tempMessageProcessingMultiplier = 2;
const int wdtRstMessageProcessingMultiplier = 2;

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