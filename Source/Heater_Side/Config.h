#ifndef CONFIG_H
#define CONFIG_H

#include<avr/wdt.h>

#ifndef WDTO_8S
  #error This sketch is only compatible with an some arduinos with avr architecture due to the watchdog timer used (8s). However, with some modifications to the timings in the program, it is possible to run it in other platforms. For more info check this: https://www.nongnu.org/avr-libc/user-manual/wdt_8h_source.html
#endif

#define VS "V1.1.0"

#define DEBUG 0
#define DEBUGWATCHDOG 0
#define DISABLE_WATCHDOGS 0 // This will also disable the connection check with the other MCU
#define MOCK_SENSORS 0
#define USE_BUTTON 1
#define TEST_COLORS 0
#define EEPROM_DONT_WRITE 0 // Only works for errors
#define ENABLE_AUTO_RESTART 1
#define COLOR_PROGRESS_FEEDBACK 1

#define ERROR_REGISTER_ADDRESS 0
#define EEPROM_USED_SIZE 1020
#define EEPROM_ERROR_MAGIC_STR "V1"

#define ENABLE_REGISTER_ADDRESS ERROR_REGISTER_ADDRESS + EEPROM_USED_SIZE + 1
#define MINIMAL_WORKING_MODE_REGISTER_ADDRESS ENABLE_REGISTER_ADDRESS + 1 + sizeof(SYSTEM_ENABLED)

#define RELAY_ENABLED 1
#define RELAY_DISABLED !RELAY_ENABLED

#define LED_ENABLED 0
#define LED_DISABLED !LED_ENABLED

#define BTN_PRESSED 1
#define BTN_RELEASED !BTN_PRESSED

const uint16_t SERIAL_USB_BAUD_RATE = 9600;
const uint16_t RS485_SERIAL_BAUD_RATE = 9600;

const float MIN_ALLOWED_TEMP = 0; // 0ºC
const float MAX_ALLOWED_TEMP = 60; // 60ºc
const float MIN_ALLOWED_PRESSURE_SENSOR_CURRENT_mA = 2.5; // mA
const float MAX_ALLOWED_PRESSURE_SENSOR_CURRENT_mA = 21; // mA

const uint8_t MAX_NUM_OF_ERRORS = 3;

const int WATCHDOG_RESET_PERIOD = 5000; // 6 s
const long SYSTEM_RESET_PERIOD = 86400000; // 24 h

const int MAX_PROGRESS_VALUE = 200; // [0-255]
const int ANIMATION_FRAME_DELAY = 2; // ms
const int FADE_MIN_TEMP_OFFSET = 2;

const long INIT_CONNECTION_TIMEOUT = 120000; // 2 min
const long INIT_CONNECTION_TIMEOUT_IF_DISABLED = 12000; // 12 sec
const long AUTO_DISABLE_PUMP_TIMEOUT = 150000; // 2.5 min

const int BUTTON_LONG_PRESSED_TIME = 2000; // 2 s
const int BUTTON_SHORT_PRESSED_MIN_TIME = 100; // ms

const int HEATER_TEMP_GATHERING_PERIOD = 10000; // 10 s
const int VALVE_TEMP_GATHERING_PERIOD = 1000; // 1 s
const float PIPE_HEAT_TRANSPORT_EFFICIENCY = 0.85; // 85% of the temperature at the heater should get into the valve
const float COLD_WATER_TEMPERATURE_MULTIPLIER = 0.9; // The temperature at wich the systems closes the valve is desiredTemp*COLD_WATER_TEMPERATURE_MULTIPLIER

const double PRESSURE_SENSOR_MIN_BAR = 0.0;
const double PRESSURE_SENSOR_MAX_BAR = 10.0;
const double WATER_MIN_NORMAL_PRESSURE_BAR = 2.75;
const double PRESSURE_SENSOR_CURRENT_MIN_mA = 3.6;
const double PRESSURE_SENSOR_CURRENT_MAX_mA = 19.5;

const int RECEIVED_MESSAGE_TIMEOUT = 100; // 100 ms
const int PUMP_MESSAGE_PROCESSING_MULTIPLIER = 2;
const int TEMP_MESSAGE_PROCESSING_MULTIPLIER = 15;
const int WDT_RST_MESSAGE_PROCESSING_MULTIPLIER = 2;

// Command structure: "SHWRS_{CMD$}[ARG$]*"
const uint8_t MAX_COMMAND_LENGTH = 17;
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
  wdt_enable(WDTO_8S); /* Enable the watchdog with a timeout of 8 seconds */

  while (1) 
  {
    Serial.print('.');
    delay(500);
  }
}


#endif
