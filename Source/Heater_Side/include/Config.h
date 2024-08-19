//
// Created by Victor on 17/06/2024.
//

#ifndef CONFIG_H
#define CONFIG_H

#include<avr/wdt.h>

#ifndef WDTO_8S
#error This sketch is only compatible with an some arduinos with avr architecture due to the watchdog timer used (8s). However, with some modifications to the timings in the program, it is possible to run it in other platforms. For more info check this: https://www.nongnu.org/avr-libc/user-manual/wdt_8h_source.html
#endif

#include <Arduino.h>

#define VS "V2.0.0"

#define DEBUG 1
#define DEBUGWATCHDOG 0
#define DEBUGTEMP 0
#define DISABLE_WATCHDOGS 0 // This will also disable the connection check with the other MCU
#define MOCK_SENSORS 0
#define SC_USE_HAMMING_7_4_CORRECTION_CODE 1
#define ENABLE_AUTO_RESTART 1
#define EEPROM_DONT_WRITE_ERRORS 0 // Only works for errors
#define EEPROM_ERROR_MEMORY_ITEMS 5
#define EEPROM_MODE_ADDRESS 0
#define EEPROM_FALLBACK_MODE_ENABLED_ADDRESS 8
#define EEPROM_ERROR_START_ADDRESS 16
#define ERROR_MESSAGE_SIZE 128

typedef enum {NO_ERROR = 0, ERROR_TEMP_SENSOR_INVALID_VALUE, ERROR_PRESSURE_SENSOR_INVALID_VALUE, ERROR_COMMS_CONNECTION_NOT_ESTABLISHED, ERROR_COMMS_NO_RESPONSE, ERROR_COMMS_UNEXPECTED_MESSAGE, ERROR_HEATER_MCU_ERROR,             ENUM_LEN} ErrorCode;
typedef struct
{
    bool activeError;
    ErrorCode errorCode;
    char message[ERROR_MESSAGE_SIZE];
} EEPROMError;


#define RELAY_ENABLED 1
#define RELAY_DISABLED !RELAY_ENABLED

#define LED_ENABLED 0
#define LED_DISABLED !LED_ENABLED

#define BTN_PRESSED 1
#define BTN_RELEASED !BTN_PRESSED

#define SC_MAX_MESSAGE_SIZE 127 // Real message size is SC_MAX_MESSAGE_SIZE+1, but the last byte is reserved for the null terminator


const uint32_t SERIAL_USB_BAUD_RATE = 115200;
const uint32_t RS485_SERIAL_BAUD_RATE = 9600;

const float MIN_ALLOWED_TEMP = 10; // 0ºC
const float MAX_ALLOWED_TEMP = 67; // 67ºc
const float MIN_ALLOWED_PRESSURE_SENSOR_CURRENT_mA = 2.5; // mA
const float MAX_ALLOWED_PRESSURE_SENSOR_CURRENT_mA = 21; // mA

const int WATCHDOG_RESET_PERIOD = 5000; // 5 s
const long SYSTEM_RESET_PERIOD = 86400000; // 24 h

const int MIN_PROGRESS_VALUE = 0; // [0-255]
const int MAX_PROGRESS_VALUE = 200; // [0-255]
const int ANIMATION_FRAME_DELAY = 2; // ms
const int FADE_MIN_TEMP_OFFSET = 2;

const long INIT_CONNECTION_TIMEOUT = 120000; // 2 min
const long INIT_CONNECTION_TIMEOUT_FALLBACK = 12000; // 12 sec
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

const int RECEIVED_MESSAGE_TIMEOUT = 400; // 100 ms
const int PUMP_MESSAGE_PROCESSING_WAIT_TIME = 400;
const int TEMP_MESSAGE_PROCESSING_WAIT_TIME = 2500;
const int WDT_RST_MESSAGE_PROCESSING_WAIT_TIME = 300;
const int PUMP_TIMEOUT_MESSAGE_PROCESSING_WAIT_TIME = 300;

// Command structure: "{HEADER}{CMD$}[ARG$]*"
char HEADER[] = "SHWRS_"; // This string is prepended to the message and used to discard leftover bytes from previous messages
const char pumpCMD[] = "PUMP";
const char tempCMD[] = "TMP";
const char OKCMD[] = "OK";
const char WTDRSTCMD[] = "WTD-RST";
const char ERRCMD[] = "ERROR";
const char setPumpTimeoutCMD[] = "SPT";


#if DEBUG
#define debug(...) do {Serial.print(__VA_ARGS__); Serial.flush();} while(0)
#define debugln(...) do {Serial.println(__VA_ARGS__); Serial.flush();} while(0)
#else
#define debug(...)
#define debugln(...)
#endif


[[noreturn]] void rebootLoop()
{
    wdt_enable(WDTO_8S); /* Enable the watchdog with a timeout of 8 seconds */

    while (true)
    {
        Serial.print('.');
        delay(500);
    }
}

const char* getErrorName(ErrorCode error)
{
    switch(error)
    {
        case NO_ERROR:
            return "NO_ERROR";
        case ERROR_TEMP_SENSOR_INVALID_VALUE:
            return "ERROR_TEMP_SENSOR_INVALID_VALUE";
        case ERROR_PRESSURE_SENSOR_INVALID_VALUE:
            return "ERROR_PRESSURE_SENSOR_INVALID_VALUE";
        case ERROR_COMMS_CONNECTION_NOT_ESTABLISHED:
            return "ERROR_COMMS_CONNECTION_NOT_ESTABLISHED";
        case ERROR_COMMS_NO_RESPONSE:
            return "ERROR_COMMS_NO_RESPONSE";
        case ERROR_COMMS_UNEXPECTED_MESSAGE:
            return "ERROR_COMMS_UNEXPECTED_MESSAGE";
        case ERROR_HEATER_MCU_ERROR:
            return "ERROR_HEATER_MCU_ERROR";
        default:
            return "Unknown Error";
    }
}

#endif
