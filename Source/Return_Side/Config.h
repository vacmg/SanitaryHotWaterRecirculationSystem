#ifndef CONFIG_H
#define CONFIG_H

#define DEBUG 1

#define PENABLED 1
#define PDISABLED !PENABLED

const int receivedMessageTimeout = 100; //ms
const long autoDisablePumpTimeout = 120000; // 2 min

// Command structure: "SHWRS_{CMD$}[ARG$]*"
char HEADER[] = "SHWRS_"; // This string is prepended to the message and used to discard leftover bytes from previous messages
const char pumpCMD[] = "PUMP";
const char tempCMD[] = "TEMP";
const char OKCMD[] = "OK";

#if DEBUG
  #define debug(...) do {Serial.print(__VA_ARGS__); Serial.flush();} while(0)
  #define debugln(...) do {Serial.println(__VA_ARGS__); Serial.flush();} while(0)
#else
  #define debug(...) 
  #define debugln(...) 
#endif

#endif