#ifndef CONFIG_H
#define CONFIG_H

#define DEBUG 1

#define RELAY_ENABLED 1
#define RELAY_DISABLED !RELAY_ENABLED

const int receivedMessageTimeout = 100; // 100 ms
const long autoDisablePumpTimeout = 120000; // 2 min
const int heaterTempGatheringPeriod = 10000; // 10 s
const int valveTempGatheringPeriod = 1000; // 1 s
const float pipeHeatTransportEfficiency = 0.9; // 90% of the temperature at the heater should get into the valve

const int pumpMessageProcessingMultiplier = 2;
const int tempMessageProcessingMultiplier = 2;

// Command structure: "SHWRS_{CMD$}[ARG$]*"
char HEADER[] = "SHWRS_"; // This string is prepended to the message and used to discard leftover bytes from previous messages
const char pumpCMD[] = "PUMP";
const char tempCMD[] = "TEMP";
const char OKCMD[] = "OK";
const char WTDRSTCMD[] = "WTD_RST"; // TODO implement error logic

#if DEBUG
  #define debug(...) do {Serial.print(__VA_ARGS__); Serial.flush();} while(0)
  #define debugln(...) do {Serial.println(__VA_ARGS__); Serial.flush();} while(0)
#else
  #define debug(...) 
  #define debugln(...) 
#endif

#endif