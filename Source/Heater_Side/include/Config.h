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
#include <EEPROM.h>

#define VS "V2.0.0"

#define DEBUG 1
#define DEBUGWATCHDOG 0
#define DEBUGTEMP 0
#define DEBUGCONNECT 0
#define PROFILER_ENABLED 1
#define ENABLE_HEARTBEAT 1
#define DISABLE_WATCHDOGS 0 // This will also disable the connection check with the other MCU.
#define MOCK_SENSORS 0
#define SC_USE_HAMMING_7_4_CORRECTION_CODE 1
#define ENABLE_AUTO_RESTART 1
#define EEPROM_DONT_WRITE_ERRORS 0 // Only works for errors
#define EEPROM_ERROR_MEMORY_ITEMS 5
#define EEPROM_MODE_ADDRESS 0
#define EEPROM_FALLBACK_MODE_ENABLED_ADDRESS 8
#define EEPROM_ERROR_START_ADDRESS 16
#define ERROR_MESSAGE_SIZE 128
#define PROFILER_DATA_START_ADDRESS 700
#define PROFILER_DATA_MSG_SIZE 64

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

#define BTN_PRESSED 0
#define BTN_RELEASED !BTN_PRESSED

#define SC_MAX_MESSAGE_SIZE 127 // Real message size is SC_MAX_MESSAGE_SIZE+1, but the last byte is reserved for the null terminator.


constexpr uint32_t SERIAL_USB_BAUD_RATE = 115200;
constexpr uint32_t RS485_SERIAL_BAUD_RATE = 9600;

constexpr float MIN_ALLOWED_TEMP = 6; // 6ºC
constexpr float MAX_ALLOWED_TEMP = 70; // 70ºc
constexpr float MIN_ALLOWED_PRESSURE_SENSOR_CURRENT_mA = 2.5; // mA
constexpr float MAX_ALLOWED_PRESSURE_SENSOR_CURRENT_mA = 21; // mA

constexpr int WATCHDOG_RESET_PERIOD = 4000; // 4 s
constexpr long SYSTEM_RESET_PERIOD = 86400000; // 24 h
constexpr int TIME_BEFORE_GETTING_HEATER_TEMP = 22000; // 22 s

constexpr int MIN_PROGRESS_VALUE = 0; // [0-255]
constexpr int MAX_PROGRESS_VALUE = 200; // [0-255]
constexpr int FADE_MIN_TEMP_OFFSET = 2;

constexpr int FALLBACK_MODE_ANIMATION_FRAME_DELAY = 20; // ms
constexpr int ALWAYS_ACTIVE_MODE_ANIMATION_FRAME_DELAY = 20; // ms
constexpr float FALLBACK_MODE_ANIMATION_BRIGHTNESS_STEP = 0.01; // [0-1]
constexpr float ALWAYS_ACTIVE_MODE_ANIMATION_BRIGHTNESS_STEP = 0.01; // [0-1]

constexpr long INIT_CONNECTION_TIMEOUT = 120000; // 2 min
constexpr long AUTO_DISABLE_PUMP_TIMEOUT = 150000; // 2.5 min

constexpr int BUTTON_LONG_PRESSED_TIME = 2000; // 2 s
constexpr int BUTTON_SHORT_PRESSED_MIN_TIME = 100; // ms

constexpr int TEMP_SENSOR_ADDITIONAL_CONVERSION_TIME = 100; // ms

constexpr int FLASH_DRIVING_WATER_COLOR_PERIOD = 1000; // 1 s

constexpr int HEATER_TEMP_GATHERING_PERIOD = 10000; // 10 s
constexpr int VALVE_TEMP_GATHERING_PERIOD = 3000; // 3 s
constexpr float HOT_WATER_TEMPERATURE_MULTIPLIER = 0.72; // more than 72% of the water temperature at the heater must be on the valve to open it.
constexpr float COLD_WATER_TEMPERATURE_MULTIPLIER = 0.85; // less than 85% of the maximum water temperature at the valve must be on the valve to close it.

constexpr double PRESSURE_SENSOR_MIN_BAR = 0.0;
constexpr double PRESSURE_SENSOR_MAX_BAR = 10.0;
constexpr double WATER_MIN_NORMAL_PRESSURE_BAR = 2.75;
constexpr double PRESSURE_SENSOR_CURRENT_MIN_mA = 3.6;
constexpr double PRESSURE_SENSOR_CURRENT_MAX_mA = 19.5;

constexpr int COMMS_MAX_RETRIES = 5;
constexpr int RECEIVED_MESSAGE_TIMEOUT = 400; // 400 ms // This is the time the system waits for a message to be completely received.
constexpr int PUMP_MESSAGE_PROCESSING_WAIT_TIME = 400;
constexpr int TEMP_MESSAGE_PROCESSING_WAIT_TIME = 400;
constexpr int WDT_RST_MESSAGE_PROCESSING_WAIT_TIME = 400;
constexpr int PUMP_TIMEOUT_MESSAGE_PROCESSING_WAIT_TIME = 400;

// Command structure: "{HEADER}{CMD$}[ARG$]*"
char HEADER[] = "SHWRS_"; // This string is prepended to the message and used to discard leftover bytes from previous messages.
constexpr char pumpCMD[] = "PUMP";
constexpr char tempCMD[] = "TMP";
constexpr char OKCMD[] = "OK";
constexpr char WTDRSTCMD[] = "WTD-RST";
constexpr char ERRCMD[] = "ERROR";
constexpr char setPumpTimeoutCMD[] = "SPT";


#if DEBUG
#define debug(...) do {Serial.print(__VA_ARGS__); Serial.flush();} while(0)
#define debugln(...) do {Serial.println(__VA_ARGS__); Serial.flush();} while(0)
#else
#define debug(...)
#define debugln(...)
#endif


[[noreturn]] inline void rebootLoop()
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

#if PROFILER_ENABLED
unsigned long profilerMillis = 0;

typedef struct
{
    unsigned long valid;
    unsigned long minTime;
    unsigned long maxTime;
    char minTimeData[PROFILER_DATA_MSG_SIZE];
    char maxTimeData[PROFILER_DATA_MSG_SIZE];
} ProfilerData;

constexpr ProfilerData defaultProfilerData = {0XABDCEF12, INT32_MAX, 0, "", ""};
ProfilerData profilerData = defaultProfilerData;

inline void profilerStartMeasure()
{
    profilerMillis = millis();
}

int profilerEndMeasure()
{
    profilerMillis = millis() - profilerMillis;
    int updated = 0;
    if(profilerMillis < profilerData.minTime)
    {
        profilerData.minTime = profilerMillis;
        updated = -1;
    }
    if(profilerMillis > profilerData.maxTime)
    {
        profilerData.maxTime = profilerMillis;
        updated = 1;
    }
    return updated;
}

inline void saveProfilerData()
{
    EEPROM.put(PROFILER_DATA_START_ADDRESS, profilerData);
}

inline void clearProfilerData()
{
    profilerData = defaultProfilerData;
    saveProfilerData();
}

inline void loadProfilerData()
{
    EEPROM.get(PROFILER_DATA_START_ADDRESS, profilerData);
    if(profilerData.valid != defaultProfilerData.valid)
    {
        clearProfilerData();
    }
}

void printProfilerData()
{
    Serial.println("Profiler Data:");
    Serial.print("Min Time: ");
    Serial.println(profilerData.minTime);
    Serial.print("Max Time: ");
    Serial.println(profilerData.maxTime);
    Serial.print("Min Time Data: ");
    Serial.println(profilerData.minTimeData);
    Serial.print("Max Time Data: ");
    Serial.println(profilerData.maxTimeData);
    Serial.println();
}

#else

inline void loadProfilerData()
{
    unsigned long invalid = 0;
    EEPROM.put(PROFILER_DATA_START_ADDRESS, invalid);
}

inline void printProfilerData()
{
    Serial.println("Profiler is disabled.");
}

#endif

#endif
