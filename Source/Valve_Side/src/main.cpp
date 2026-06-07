// UART LINK: Arduino Mega <-> ESP32 <-> Valve MCU
//  +--------------------+      UART      +------------------+      CAN      +----------------------------+
//  |   Arduino Mega     |                |      ESP32       |               |     Heater MCU             |
//  |   Serial2 TX  ---->+--------------> |  RX 32 CAN RX 23 | <-----------< |     CAN TX  CANH -> Verde  |
//  |   Serial2 RX  <----+--------------< |  TX 33 CAN TX 22 | >-----------> |     CAN RX  CANL -> Blanco |
//  +--------------------+                +------------------+               +----------------------------+

#include <Arduino.h>
#include "Config.h"
#include<avr/wdt.h> /* Header for watchdog timers in AVR*/
#include <EEPROM.h>
#include <MAX_RS485.h>
#include "SimpleComms.h"

#if !MOCK_SENSORS
#include <OneWire.h>
#include <DallasTemperature.h>
#endif


#define MAIN_HELP_STRING "\nType 'reboot' to restart the system;\n'enable' or 'disable' to disable or enable the fallback mode;\n'clear' to invalidate the Error Register;\n'sensors' to print all the sensors current value;\n'changemode' to change the operation mode to the next one (similar to pressing the button);\n'startpump' or 'stoppump' to manually start or stop the pump;\n'openvalve' or 'closevalve' to manually open or close the valve;\n'errorlist' to print the error list;\n'reset' or 'reseton' to clear the error list and enable or disable the fallback mode"
#define MOCK_SENSORS_HELP_STRING "\nPress 'e' or 'd' to enable or disable trigger;\nPress 'n', 's' or 'l' to set the button to NO_PULSE, SHORT_PULSE or LONG_PULSE;\nSend a number to incorporate it as the valve temp\n"

constexpr uint8_t RECEIVER_ENABLE_PIN = 5;  // HIGH = Driver / LOW = Receptor
constexpr uint8_t DRIVE_ENABLE_PIN = 4;  // HIGH = Driver / LOW = Receptor

constexpr uint8_t VALVE_RELAY_PIN = 2;
// const uint8_t VALVE_FEEDBACK_PIN = 7;

constexpr uint8_t RED_LED_PIN = 11;
constexpr uint8_t GREEN_LED_PIN = 9;
constexpr uint8_t BLUE_LED_PIN = 10;

constexpr uint8_t BUTTON_PIN = 8;

#if ENABLE_HEARTBEAT
constexpr uint8_t HEARTBEAT_PIN = 13;
#endif

#if !MOCK_SENSORS
constexpr uint8_t PRESSURE_SENSOR = A0;
constexpr uint8_t TEMP_SENSOR = 12;

OneWire ourWire(TEMP_SENSOR); // Create Onewire instance for temp sensor
DallasTemperature tempSensor(&ourWire); // Create temp sensor instance
#endif

SimpleComms comms(&Serial2, HEADER); // Create comms instance

typedef enum {Black, Red, Green, Blue, Yellow, Orange, Purple, Cyan, White, Gray} Color;

typedef struct
{
    byte r;
    byte g;
    byte b;
} RGBColor;

#define COLOR_BLACK_AS_RGB {0,0,0}
#define COLOR_RED_AS_RGB {255,0,0}
#define COLOR_GREEN_AS_RGB {0,255,0}
#define COLOR_BLUE_AS_RGB {0,0,255}
#define COLOR_YELLOW_AS_RGB {255,255,0}
#define COLOR_ORANGE_AS_RGB {255,20,0}
#define COLOR_PURPLE_AS_RGB {255,0,255}
#define COLOR_CYAN_AS_RGB {0,255,255}
#define COLOR_WHITE_AS_RGB {255,255,255}
#define COLOR_GRAY_AS_RGB {1,1,1}

#define BOOT_COLOR Green
#define USER_ACK_COLOR Cyan
#define ERROR_FALLBACK_COLOR Yellow
#define WAITING_COLD_COLOR Gray
#define DRIVING_WATER_COLOR Blue
#define SERVING_WATER_COLOR Red
#define ALWAYS_ACTIVE_WATER_COLOR Orange
#define WDT_BOOT_DELAY_COLOR Cyan
#define EMERGENCY_SHUTDOWN_1_COLOR White
#define EMERGENCY_SHUTDOWN_2_COLOR Yellow

typedef enum {NO_PULSE = 0, SHORT_PULSE, LONG_PULSE} ButtonStatus;

typedef enum {OnPressureTrigger = 0, AlwaysActive, NUM_OF_MODES, ErrorFallBackMode} Mode; // Mode of the system
#define changeMode(newMode) do {debug(F("Changing Mode from ")); debug(modeToString(currentMode)); currentMode = newMode; debug(F(" to ")); debugln(modeToString(currentMode)); changeStatus(getBeginStatus(currentMode));} while(0)

typedef enum {ErrorFallBack_Begin, ErrorFallBack, OnPressureTrigger_Begin, OnPressureTrigger_WaitingCold, OnPressureTrigger_TransitionToDrivingWater, OnPressureTrigger_DrivingWater, OnPressureTrigger_ServingWater, AlwaysActive_Begin, AlwaysActive_TransitionToGettingHotWater, AlwaysActive_GettingHotWater, AlwaysActive_Idle} Status;
#define changeStatus(newStatus) do {debug(F("Changing Status from ")); debug(statusToString(currentStatus)); currentStatus = newStatus; writeColor(statusToColor(currentStatus)); debug(F(" to ")); debugln(statusToString(currentStatus));} while(0)

Mode currentMode = ErrorFallBackMode;
Status currentStatus = ErrorFallBack_Begin;

#if !DISABLE_WATCHDOGS
unsigned long watchdogsPMillis = 0;
#endif

#if MOCK_SENSORS
ButtonStatus btnSt = NO_PULSE;
bool triggerVal = false;
#endif

unsigned long heaterTempPMillis = 0;
unsigned long timeBeforeGettingHeaterTempMillis = 0;
unsigned long flashDrivingWaterColorMillis = 0;

bool hotStart = false; // This is used to know if enough time has passed since the last time the pump was started.
int lastHeaterTemp;

int progressMinTemp = 0;
float desiredTemp = 0;
float maxTemp = 0;

unsigned long valveTempRequestTempMillis = 0;
bool valveTempRequested = false;
unsigned long VALVE_TEMP_WAIT_FROM_REQUEST_TO_READ; // Updated to the real value at setup
float valveTemp = 0;
bool tempRequestReady = false;

void stepFSM();
[[noreturn]] void raiseError(ErrorCode error, const char* message = nullptr);
[[noreturn]] void raiseError(ErrorCode error, const __FlashStringHelper* message);

Status getBeginStatus(Mode mode)
{
    switch(mode)
    {
        case OnPressureTrigger:
            return OnPressureTrigger_Begin;
        case AlwaysActive:
            return AlwaysActive_Begin;
        default:
            return ErrorFallBack_Begin;
    }
}

double fmap(double x, double in_min, double in_max, double out_min, double out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

char* strncpy_F(char* dest, const __FlashStringHelper* src, size_t size)
{
    PGM_P p = reinterpret_cast<PGM_P>(src);
    for (size_t i = 0; i<size; i++)
    {
        char c = pgm_read_byte(p++);

        dest[i] = c;
        if (c == '\0')
        {
            break;
        }
    }

    dest[size-1] = '\0';
    return dest;
}

Color statusToColor(Status status)
{
    switch(status)
    {
        case ErrorFallBack_Begin:
        case ErrorFallBack:
            return ERROR_FALLBACK_COLOR;
        case OnPressureTrigger_Begin:
            return BOOT_COLOR;
        case OnPressureTrigger_WaitingCold:
            return WAITING_COLD_COLOR;
        case OnPressureTrigger_TransitionToDrivingWater:
        case OnPressureTrigger_DrivingWater:
            return DRIVING_WATER_COLOR;
        case OnPressureTrigger_ServingWater:
            return SERVING_WATER_COLOR;
        case AlwaysActive_Begin:
            return BOOT_COLOR;
        case AlwaysActive_Idle:
        case AlwaysActive_TransitionToGettingHotWater:
        case AlwaysActive_GettingHotWater:
            return ALWAYS_ACTIVE_WATER_COLOR;
        default:
            return Black; // Return Black color for unknown status
    }
}

const char* modeToString(Mode mode)
{
    switch(mode)
    {
        case OnPressureTrigger:
            return "OnPressureTrigger";
        case AlwaysActive:
            return "AlwaysActive";
        case ErrorFallBackMode:
            return "ErrorFallBackMode";
        default:
            return "Unknown Mode";
    }
}

const char* statusToString(Status status)
{
    switch(status)
    {
        case ErrorFallBack_Begin:
            return "ErrorFallBack_Begin";
        case ErrorFallBack:
            return "ErrorFallBack";
        case OnPressureTrigger_Begin:
            return "OnPressureTrigger_Begin";
        case OnPressureTrigger_WaitingCold:
            return "OnPressureTrigger_WaitingCold";
        case OnPressureTrigger_TransitionToDrivingWater:
            return "OnPressureTrigger_TransitionToDrivingWater";
        case OnPressureTrigger_DrivingWater:
            return "OnPressureTrigger_DrivingWater";
        case OnPressureTrigger_ServingWater:
            return "OnPressureTrigger_ServingWater";
        case AlwaysActive_Begin:
            return "AlwaysActive_Begin";
        case AlwaysActive_GettingHotWater:
            return "AlwaysActive_GettingHotWater";
        case AlwaysActive_TransitionToGettingHotWater:
            return "AlwaysActive_TransitionToGettingHotWater";
        case AlwaysActive_Idle:
            return "AlwaysActive_Idle";
        default:
            return "Unknown Status";
    }
}

const char* formattedTime(long milliseconds, char* buff)
{
    if(milliseconds<0)
    {
        strcpy(buff, "None");
        return buff;
    }

    long ms = milliseconds%1000;
    long s = milliseconds/1000;
    long m = s/60;
    long h = m/60;
    m = m%60;
    s = s%60;

    sprintf(buff,"%lih %lim %lis %lims (%lims)",h,m,s,ms,milliseconds);
    return buff;
}

RGBColor getRGBColor(const Color color)
{
    switch (color)
    {
        case Black:
            return COLOR_BLACK_AS_RGB;
        case Red:
            return COLOR_RED_AS_RGB;
        case Green:
            return COLOR_GREEN_AS_RGB;
        case Blue:
            return COLOR_BLUE_AS_RGB;
        case Yellow:
            return COLOR_YELLOW_AS_RGB;
        case Orange:
            return COLOR_ORANGE_AS_RGB;
        case Purple:
            return COLOR_PURPLE_AS_RGB;
        case Cyan:
            return COLOR_CYAN_AS_RGB;
        case White:
            return COLOR_WHITE_AS_RGB;
        case Gray:
            return COLOR_GRAY_AS_RGB;
    }

    return COLOR_BLACK_AS_RGB;
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

void writeColor(const RGBColor& color)
{
    writeColor(color.r, color.g, color.b);
}

void writeColor(const Color color)
{
    writeColor(getRGBColor(color));
}

RGBColor getShadeFromColor(const RGBColor& color, float shadeProportion)
{
    shadeProportion = constrain(shadeProportion, 0.0, 1.0);
    return {static_cast<byte>(color.r*shadeProportion), static_cast<byte>(color.g*shadeProportion), static_cast<byte>(color.b*shadeProportion)};
}

void fadeAnimationStep(const RGBColor& baseColor, float fadeAmountPerFrame)
{
    static bool increasing = true;
    static float fade = 0.0;

    if(increasing)
    {
        fade += fadeAmountPerFrame;
        if(fade >= 1.0)
        {
            fade = 1.0;
            increasing = false;
        }
    }
    else
    {
        fade -= fadeAmountPerFrame;
        if(fade <= 0.0)
        {
            fade = 0.0;
            increasing = true;
        }
    }
    writeColor(getShadeFromColor(baseColor, fade));
}

#if !DISABLE_WATCHDOGS

void resetWatchdogs()
{
    wdt_reset();
    watchdogsPMillis = millis();
    #if DEBUGWATCHDOG
        debugln(F("Watchdogs reset in progress..."));
    #endif

    char errorBuff[ERROR_MESSAGE_SIZE] = "resetWatchdogs - NoError";
    char commsBuffer[SC_MAX_MESSAGE_SIZE+1] = "";
    ErrorCode err = ENUM_LEN; // Some invalid value to enter the loop, must be overwritten no matter what branch is taken.
    for (int retries = 0; err != NO_ERROR && retries<COMMS_MAX_RETRIES; retries++)
    {
        #if DEBUGWATCHDOG
            debug(F("Retry number ")); debug(retries); debug(F("\tCurrent error string: ")); debugln(errorBuff);
        #endif
        comms.sendCommand(WTDRSTCMD, nullptr, 0);
        delay(WDT_RST_MESSAGE_PROCESSING_WAIT_TIME);

        int res = comms.getNextCommand(commsBuffer, SC_MAX_MESSAGE_SIZE);
        if(res > 0)
        {
            if(strcmp(commsBuffer, OKCMD) == 0)
            {
                #if DEBUGWATCHDOG
                    debugln(F("Watchdogs reset successfully"));
                #endif
                err = NO_ERROR;
            }
            else if (currentMode != ErrorFallBackMode)
            {
                snprintf_P(errorBuff, ERROR_MESSAGE_SIZE, PSTR("Unexpected response from the HEATER MCU at resetWatchdogs: %s"), commsBuffer);
                err = ERROR_COMMS_UNEXPECTED_MESSAGE;
            }
            else
            {
                err = NO_ERROR;
            }
        }
        else if (currentMode != ErrorFallBackMode)
        {
            if(res == 0)
            {
                snprintf_P(errorBuff, ERROR_MESSAGE_SIZE, PSTR("Timeout receiving WTD-RST response"));
                err = ERROR_COMMS_NO_RESPONSE;
            }
            else
            {
                snprintf_P(errorBuff, ERROR_MESSAGE_SIZE, PSTR("Unable to parse message header in resetWatchdogs"));
                err = ERROR_COMMS_UNEXPECTED_MESSAGE;
            }
        }
        else
        {
            err = NO_ERROR;
        }
    }

    if(err != NO_ERROR)
    {
        raiseError(err, errorBuff);
    }
}

void resetWatchdogsIfNecessary()
{
    if(millis() - watchdogsPMillis > WATCHDOG_RESET_PERIOD) // Reset both watchdogs once in a WATCHDOG_RESET_PERIOD
    {
    #if ENABLE_HEARTBEAT
        digitalWrite(HEARTBEAT_PIN, 1);
    #endif
        resetWatchdogs();
    #if ENABLE_HEARTBEAT
        digitalWrite(HEARTBEAT_PIN, 0);
    #endif
    }
}

#endif

ButtonStatus readButton(bool avoidWatchdogReset = false)
{
    ButtonStatus res = NO_PULSE;

    #if MOCK_SENSORS
    res = btnSt;
    btnSt = NO_PULSE;
    #else
    if(digitalRead(BUTTON_PIN) == BTN_PRESSED)
    {
        unsigned long time = millis();
        delay(10);
        while(digitalRead(BUTTON_PIN) == BTN_PRESSED && millis() - time < BUTTON_LONG_PRESSED_TIME)
        {
            #if !DISABLE_WATCHDOGS
            if(!avoidWatchdogReset)
            {
                resetWatchdogsIfNecessary();
            }
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
    #endif

    return res;
}

void invalidateErrorData()
{
    EEPROMError eepromError;
    for(uint8_t i = 0; i < EEPROM_ERROR_MEMORY_ITEMS; i++)
    {
        EEPROM.get(EEPROM_ERROR_START_ADDRESS + i*sizeof(EEPROMError), eepromError);
        eepromError.activeError = false;
        EEPROM.put(EEPROM_ERROR_START_ADDRESS + i*sizeof(EEPROMError), eepromError);
    }
}

void printErrorData()
{
    EEPROMError eepromError;
    bool anyError = false;
    for(uint8_t i = 0; i < EEPROM_ERROR_MEMORY_ITEMS; i++)
    {
        EEPROM.get(EEPROM_ERROR_START_ADDRESS + i*sizeof(EEPROMError), eepromError);
        if(eepromError.activeError)
        {
            anyError = true;
            Serial.print(F("Error: ")); Serial.print(getErrorName(eepromError.errorCode)); Serial.print(F(" - ")); Serial.println(eepromError.message);
        }
    }

    if(!anyError)
    {
        Serial.println(F("No errors found"));
    }

}

void toggleFallbackMode(bool enableFallBackMode)
{
    if(enableFallBackMode)
    {
        if(currentMode == ErrorFallBackMode)
        {
            debugln(F("Warning: FallBack Mode already enabled"));
            return;
        }
        debug(F("Enabling Error FallBack Mode from mode "));debugln(modeToString(currentMode));
        EEPROM.put(EEPROM_FALLBACK_MODE_ENABLED_ADDRESS, true);
        EEPROM.put(EEPROM_MODE_ADDRESS, currentMode);
        changeMode(ErrorFallBackMode);
        stepFSM();
    }
    else
    {
        debugln(F("Disabling Error FallBack Mode"));
        Mode mode;
        EEPROM.get(EEPROM_MODE_ADDRESS, mode);
        if(mode >= NUM_OF_MODES)
        {
            Serial.println(F("ERROR: Invalid Mode stored in EEPROM, setting to OnPressureTrigger"));
            mode = OnPressureTrigger;
        }
        changeMode(mode);
        EEPROM.put(EEPROM_FALLBACK_MODE_ENABLED_ADDRESS, false);
    }
}

[[noreturn]] void raiseError(ErrorCode error, const char* message)
{
    #if !DISABLE_WATCHDOGS
        wdt_reset();
    #endif
    Serial.println(F("\n-----------------------------------------"));
    Serial.println(F("-----------------------------------------"));
    Serial.print(F("ERROR RAISED: ")); Serial.print(getErrorName(error)); if(message != nullptr) {Serial.print(F(": ")); Serial.print(message);} else {Serial.print(F("No aditional information provided"));} Serial.println();
    Serial.println(F("-----------------------------------------"));
    Serial.println(F("-----------------------------------------\n"));

    writeColor(ERROR_FALLBACK_COLOR);

    #if !EEPROM_DONT_WRITE_ERRORS
    if(error != NO_ERROR)
    {
        bool errorSaved = false;
        for(uint8_t i = 0; !errorSaved && i < EEPROM_ERROR_MEMORY_ITEMS; i++)
        {
            EEPROMError eepromError;
            EEPROM.get(EEPROM_ERROR_START_ADDRESS + i*sizeof(EEPROMError), eepromError);

            if(!eepromError.activeError)
            {
                eepromError.activeError = true;
                eepromError.errorCode = error;
                if(message != nullptr)
                {
                    strncpy(eepromError.message, message, ERROR_MESSAGE_SIZE);
                }
                else
                {
                    strncpy_P(eepromError.message, PSTR("No message provided"), ERROR_MESSAGE_SIZE);
                }
                EEPROM.put(EEPROM_ERROR_START_ADDRESS + i*sizeof(EEPROMError), eepromError);
                errorSaved = true;
            }
        }
        if(!errorSaved)
        {
            Serial.println(F("ERROR: Error memory is full"));
        }

        toggleFallbackMode(true);
    }
    #endif

    Serial.println(F("Rebooting..."));
    rebootLoop();
}

[[noreturn]] void raiseError(ErrorCode error, const __FlashStringHelper* message)
{
    char buff[ERROR_MESSAGE_SIZE];

    strncpy_F(buff,message,ERROR_MESSAGE_SIZE);

    raiseError(error, buff);
}

void handleHeaterError(int retryCount, char* buff = nullptr)
{
    if(retryCount>=COMMS_MAX_RETRIES)
    {
        if(comms.getNextArgument(buff, SC_MAX_MESSAGE_SIZE) > 0)
        {
            raiseError(ERROR_HEATER_MCU_ERROR, buff);
        }
        raiseError(ERROR_HEATER_MCU_ERROR);
    }
}

void setPumpTimeout(long timeout)
{
    if(timeout < 0)
    {
        timeout = 0;
    }
    char commsBuffer[SC_MAX_MESSAGE_SIZE+1] = "";
    char args[12];
    snprintf(args, 12, "%ld", timeout);
    const char* argsPtr[] = {args};

    #if DEBUG
        if(timeout == 0)
        {
            debugln(F("Disabling pump timeout"));
        }
        else
        {
            debug(F("Setting pump timeout to ")); debugln(args);
        }
    #endif

    comms.sendCommand(setPumpTimeoutCMD, argsPtr, 1); // As this is a config command, no retries are used, if the message is not received, both MCUs will reboot.

    delay(PUMP_TIMEOUT_MESSAGE_PROCESSING_WAIT_TIME);
    int res = comms.getNextCommand(commsBuffer, SC_MAX_MESSAGE_SIZE);
    if(res > 0)
    {
        if(strcmp(commsBuffer, OKCMD) == 0)
        {
            #if DEBUG
                if(timeout == 0)
                {
                    debugln(F("Pump timeout disabled successfully"));
                }
                else
                {
                    debugln(F("Pump timeout set successfully"));
                }
            #endif
        }
        else if(strcmp(commsBuffer, ERRCMD) == 0)
        {
            handleHeaterError(COMMS_MAX_RETRIES, commsBuffer);
        }
        else
        {
            char errorBuff[ERROR_MESSAGE_SIZE];
            snprintf_P(errorBuff, ERROR_MESSAGE_SIZE, PSTR("Unexpected response from the HEATER MCU at setPumpTimeout: %s"), commsBuffer);
            raiseError(ERROR_COMMS_UNEXPECTED_MESSAGE, errorBuff);
        }
    }
    else if(res == 0)
    {
        raiseError(ERROR_COMMS_NO_RESPONSE, F("Timeout receiving setPumpTimeout response"));
    }
    else
    {
        raiseError(ERROR_COMMS_UNEXPECTED_MESSAGE, F("Unable to parse message header in setPumpTimeout"));
    }
}

void setValve(bool enable)
{
    digitalWrite(VALVE_RELAY_PIN, enable?RELAY_ENABLED:RELAY_DISABLED);
}

void setPump(bool enable, bool ignoreErrors = false)
{
    const char* args[] = {"0"};
    if(enable)
    {
        args[0] = "1";
    }

    debug(enable?F("Starting pump... "):F("Stopping pump... ")); if(ignoreErrors) {debug(F("Ignoring errors"));} debugln();

    char errorBuff[ERROR_MESSAGE_SIZE] = "setPump - NoError";
    char commsBuffer[SC_MAX_MESSAGE_SIZE+1] = "";
    ErrorCode err = ENUM_LEN; // Some invalid value to enter the loop, must be overwritten no matter what branch is taken.
    for (int retries = 0, delayTime = PUMP_MESSAGE_PROCESSING_WAIT_TIME; err != NO_ERROR && retries<COMMS_MAX_RETRIES; retries++, delayTime *= 2)
    {
        debug(F("Retry number ")); debug(retries); debug(F("\tCurrent error string: ")); debugln(errorBuff);
        comms.sendCommand(pumpCMD, args, 1);
        delay(delayTime);
        int res = comms.getNextCommand(commsBuffer, SC_MAX_MESSAGE_SIZE);
        if(res > 0)
        {
            if(strcmp(commsBuffer, OKCMD) == 0)
            {
                debugln(enable?F("Pump started successfully"):F("Pump stopped successfully"));
                err = NO_ERROR;
            }
            else if(strcmp(commsBuffer, ERRCMD) == 0)
            {
                if(!ignoreErrors)
                {
                    handleHeaterError(retries, commsBuffer);
                }
                else if(comms.getNextArgument(commsBuffer, SC_MAX_MESSAGE_SIZE) > 0)
                {
                    debug(F("HEATER MCU returned the error: '")); debug(commsBuffer); debugln(F("', but it was ignored"));
                }
                else
                {
                    debugln(F("HEATER MCU returned an error, but it was ignored"));
                }
                err = NO_ERROR;
            }
            else
            {
                snprintf_P(errorBuff, ERROR_MESSAGE_SIZE, PSTR("Unexpected response from the HEATER MCU at setPump: %s"), commsBuffer);
                err = ERROR_COMMS_UNEXPECTED_MESSAGE;
            }
        }
        else if(!ignoreErrors)
        {
            if(res == 0)
            {
                snprintf_P(errorBuff, ERROR_MESSAGE_SIZE, PSTR("Timeout receiving setPump response"));
                err = ERROR_COMMS_NO_RESPONSE;
            }
            else
            {
                snprintf_P(errorBuff, ERROR_MESSAGE_SIZE, PSTR("Unable to parse message header in setPump"));
                err = ERROR_COMMS_UNEXPECTED_MESSAGE;
            }
        }
        else
        {
            err = NO_ERROR;
        }
    }

    if(err != NO_ERROR)
    {
        raiseError(err, errorBuff);
    }
}

void requestValveTempIfNecessary()
{
#if !MOCK_SENSORS
    if((!valveTempRequested) && (millis() - valveTempRequestTempMillis > VALVE_TEMP_GATHERING_PERIOD))
    {
        valveTempRequestTempMillis = millis();
        #if DEBUGTEMP
            debug(F("Requesting temp at millis() = ")); debugln(valveTempRequestTempMillis);
        #endif
        tempSensor.requestTemperatures();
        valveTempRequested = true;
    }
#endif
}

void getValveTempIfNecessary(bool ignoreErrors = false)
{
#if !MOCK_SENSORS
    if((valveTempRequested) && (millis() - valveTempRequestTempMillis > VALVE_TEMP_WAIT_FROM_REQUEST_TO_READ))
    {
        valveTemp = tempSensor.getTempCByIndex(0); // Get temp
        #if DEBUGTEMP
            debug(F("Temp read: ")); debugln(valveTemp);
        #endif

        char errorBuff[ERROR_MESSAGE_SIZE];
        if(!ignoreErrors && valveTemp<MIN_ALLOWED_TEMP)
        {
            snprintf_P(errorBuff, ERROR_MESSAGE_SIZE, PSTR("VALVE TEMP IS TOO LOW (%d)"),static_cast<int>(valveTemp));
            raiseError(ERROR_TEMP_SENSOR_INVALID_VALUE, errorBuff);
        }
        if(!ignoreErrors && valveTemp>MAX_ALLOWED_TEMP)
        {
            snprintf_P(errorBuff, ERROR_MESSAGE_SIZE, PSTR("VALVE TEMP IS TOO HIGH (%d)"),static_cast<int>(valveTemp));
            raiseError(ERROR_TEMP_SENSOR_INVALID_VALUE, errorBuff);
        }
        valveTempRequested = false;
        tempRequestReady = true;
    }
#endif
}

float getValveTemp(bool ignoreErrors = false)
{
    tempSensor.setWaitForConversion(true);

    tempSensor.requestTemperatures();
    valveTemp = tempSensor.getTempCByIndex(0);

    char errorBuff[ERROR_MESSAGE_SIZE];
    if(!ignoreErrors && valveTemp<MIN_ALLOWED_TEMP)
    {
        snprintf_P(errorBuff, ERROR_MESSAGE_SIZE, PSTR("VALVE TEMP IS TOO LOW (%d)"),static_cast<int>(valveTemp));
        raiseError(ERROR_TEMP_SENSOR_INVALID_VALUE, errorBuff);
    }
    if(!ignoreErrors && valveTemp>MAX_ALLOWED_TEMP)
    {
        snprintf_P(errorBuff, ERROR_MESSAGE_SIZE, PSTR("VALVE TEMP IS TOO HIGH (%d)"),static_cast<int>(valveTemp));
        raiseError(ERROR_TEMP_SENSOR_INVALID_VALUE, errorBuff);
    }

    tempSensor.setWaitForConversion(false);
    return valveTemp;
}

double getValvePressure(bool ignoreErrors = false)
{
    int adcData = analogRead(A0);

    double pressureSensorVoltage = (adcData * 1.1)/1024;
    double pressureSensorCurrent = (pressureSensorVoltage*1000) / 51;

    if(!ignoreErrors && !(pressureSensorCurrent >= MIN_ALLOWED_PRESSURE_SENSOR_CURRENT_mA && pressureSensorCurrent <= MAX_ALLOWED_PRESSURE_SENSOR_CURRENT_mA))
    {
        char errorBuff[ERROR_MESSAGE_SIZE];
        snprintf_P(errorBuff, ERROR_MESSAGE_SIZE, PSTR("PRESSURE CURRENT (%dmA) IS OUTSIDE THE RANGE (%d, %d)mA"),static_cast<int>(pressureSensorCurrent), static_cast<int>(MIN_ALLOWED_PRESSURE_SENSOR_CURRENT_mA), static_cast<int>(MAX_ALLOWED_PRESSURE_SENSOR_CURRENT_mA));
        raiseError(ERROR_PRESSURE_SENSOR_INVALID_VALUE,errorBuff);
    }

    return fmap(pressureSensorCurrent, PRESSURE_SENSOR_CURRENT_MIN_mA, PRESSURE_SENSOR_CURRENT_MAX_mA, PRESSURE_SENSOR_MIN_BAR, PRESSURE_SENSOR_MAX_BAR);
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

int getHeaterTemp(bool ignoreErrors = false)
{
    if(!ignoreErrors)
    {
        debugln(F("Getting heater temp..."));
    }

    char errorBuff[ERROR_MESSAGE_SIZE] = "getHeaterTemp - NoError";
    char commsBuffer[SC_MAX_MESSAGE_SIZE+1] = "";
    ErrorCode err = ENUM_LEN; // Some invalid value to enter the loop, must be overwritten no matter what branch is taken.
    for (int retries = 0; err != NO_ERROR && retries<COMMS_MAX_RETRIES; retries++)
    {
        #if DEBUGTEMP
        debug(F("Retry number ")); debug(retries); debug(F("\tCurrent error string: ")); debugln(errorBuff);
        #endif
        comms.sendCommand(tempCMD, nullptr, 0);
        delay(TEMP_MESSAGE_PROCESSING_WAIT_TIME);
        int res = comms.getNextCommand(commsBuffer, SC_MAX_MESSAGE_SIZE);
        if(res > 0)
        {
            if(strcmp(commsBuffer, tempCMD) == 0)
            {
                if(comms.getNextArgument(commsBuffer, SC_MAX_MESSAGE_SIZE) > 0)
                {
                    if(!ignoreErrors)
                    {
                        #if DEBUGTEMP
                        debug(F("Heater temp received: "));
                        debugln(commsBuffer);
                        #endif
                    }
                    const long temp = strtol(commsBuffer, nullptr, 10);
                    if(!ignoreErrors && static_cast<float>(temp) < MIN_ALLOWED_TEMP)
                    {
                        snprintf_P(errorBuff, ERROR_MESSAGE_SIZE, PSTR("HEATER TEMP IS TOO LOW (%d)"),static_cast<int>(valveTemp));
                        raiseError(ERROR_TEMP_SENSOR_INVALID_VALUE, errorBuff);
                    }
                    if(!ignoreErrors && static_cast<float>(temp) > MAX_ALLOWED_TEMP)
                    {
                        snprintf_P(errorBuff, ERROR_MESSAGE_SIZE, PSTR("HEATER TEMP IS TOO HIGH (%d)"),static_cast<int>(valveTemp));
                        raiseError(ERROR_TEMP_SENSOR_INVALID_VALUE, errorBuff);
                    }
                    return static_cast<int>(temp);
                }
                if(!ignoreErrors)
                {
                    snprintf_P(errorBuff, ERROR_MESSAGE_SIZE, PSTR("No temperature value received from the HEATER MCU"));
                    err = ERROR_COMMS_UNEXPECTED_MESSAGE;
                }
                else
                {
                    err = NO_ERROR;
                }
            }
            else if(strcmp(commsBuffer, ERRCMD) == 0)
            {
                if(!ignoreErrors)
                {
                    handleHeaterError(retries, commsBuffer);
                }
                else if(comms.getNextArgument(commsBuffer, SC_MAX_MESSAGE_SIZE) > 0)
                {
                    debug(F("HEATER MCU returned the error: '")); debug(commsBuffer); debugln(F("', but it was ignored"));
                }
                else
                {
                    debugln(F("HEATER MCU returned an error, but it was ignored"));
                }
                err = NO_ERROR;
            }
            else
            {
                if(!ignoreErrors)
                {
                    snprintf_P(errorBuff, ERROR_MESSAGE_SIZE, PSTR("Unexpected response from the HEATER MCU at getHeaterTemp: %s"), commsBuffer);
                    err = ERROR_COMMS_UNEXPECTED_MESSAGE;
                }
                else
                {
                    err = NO_ERROR;
                }
            }
        }
        else
        {
            if(!ignoreErrors)
            {
                if(res == 0)
                {
                    snprintf_P(errorBuff, ERROR_MESSAGE_SIZE, PSTR("Timeout receiving getTemp response"));
                    err = ERROR_COMMS_NO_RESPONSE;
                }
                else
                {
                    snprintf_P(errorBuff, ERROR_MESSAGE_SIZE, PSTR("Unable to parse message header in getTemp"));
                    err = ERROR_COMMS_UNEXPECTED_MESSAGE;
                }
            }
            else
            {
                err = NO_ERROR;
            }
        }
    }

    if(err != NO_ERROR)
    {
        raiseError(err, errorBuff);
    }

    return 0;
}

bool getHeaterTempIfNecessary(int* temp)
{
    if(millis() - heaterTempPMillis > HEATER_TEMP_GATHERING_PERIOD) // each 10 secs update heater temp
    {
        *temp = getHeaterTemp();

        heaterTempPMillis = millis();
        return true;
    }
    return false;
}

float getDesiredTemp(const float temp, Status status = currentStatus)
{
    if( (status == OnPressureTrigger_DrivingWater || status == AlwaysActive_GettingHotWater) && !hotStart)
    {
        return temp*HOT_WATER_TEMPERATURE_MULTIPLIER;
    }

    return temp*COLD_WATER_TEMPERATURE_MULTIPLIER;
}

void printSystemInfo()
{
    char buff[64];
    Serial.print(F(    "Remaining time until next restart: ")); Serial.println(formattedTime(SYSTEM_RESET_PERIOD - static_cast<long>(millis()), buff));
    Serial.print(F(    "Watchdogs reset period: ")); Serial.println(formattedTime(WATCHDOG_RESET_PERIOD, buff));
    Serial.print(F(    "FallBack Mode ")); Serial.println(currentMode==ErrorFallBackMode?"Enabled":"Disabled");
    Serial.print(F(    "Comms max retries: ")); Serial.println(COMMS_MAX_RETRIES);
    Serial.print(F(    "Error message max length: ")); Serial.println(ERROR_MESSAGE_SIZE);
    Serial.print(F(    "Comms message max length: ")); Serial.println(SC_MAX_MESSAGE_SIZE);
    Serial.print(F(    "Mode: ")); Serial.println(modeToString(currentMode));
    Serial.print(F(    "Status: ")); Serial.println(statusToString(currentStatus));
    Serial.print(F(    "Hot Start: ")); Serial.println(hotStart?F("ACTIVE"):F("INACTIVE"));
}

void printSensorsInfo()
{
    Serial.println(F("\n-----------------------------------------"  ));
    printSystemInfo();
    Serial.println(F(  "Sensor list:\n"));
    #if MOCK_SENSORS
    Serial.println(F("Valve pressure sensor: MOCKED"));
    Serial.print(F(  "Valve temperature sensor: MOCKED to ")); Serial.print(valveTemp);Serial.println(F("ºC"));
    #else
    Serial.print(F(  "Valve pressure sensor: ")); Serial.print(getValvePressure(true));Serial.println(F("BAR"));
    Serial.print(F(  "Valve temperature sensor: ")); Serial.print(getValveTemp(true));Serial.println(F("ºC"));
    #endif
    int heaterTemp = getHeaterTemp(true);
    Serial.print(F(    "Heater temperature sensor: ")); Serial.print(heaterTemp);Serial.println(F("ºC"));
    Serial.print(F(    "Desired temperature: ")); Serial.print(getDesiredTemp(heaterTemp));Serial.println(F("ºC"));

    Serial.println(F(  "-----------------------------------------\n"));
}

#if ENABLE_AUTO_RESTART
void checkResetTime()
{
    if((currentMode != ErrorFallBackMode) && (millis() > SYSTEM_RESET_PERIOD))
    {
        raiseError(NO_ERROR, F("INFO: Restarting the system due to SYSTEM_RESET_PERIOD timeout"));
    }
}
#endif

void setNextMode()
{
    changeMode(static_cast<Mode>((currentMode + 1) % NUM_OF_MODES));
    EEPROM.put(EEPROM_MODE_ADDRESS, currentMode);
}

void serialEvent()
{
    char buffer[65];
    size_t len = Serial.readBytesUntil('\n', buffer, 64);
    buffer[len] = '\0';

    Serial.print(F("User input: ")); Serial.println(buffer);

    if(strstr(buffer,"help") != nullptr)
    {
        Serial.println(F(MAIN_HELP_STRING));
        #if MOCK_SENSORS
        Serial.println(F(MOCK_SENSORS_HELP_STRING));
        #endif
    }
    else if(strstr(buffer,"reboot") != nullptr)
    {
        raiseError(NO_ERROR, F("Rebooting by user command"));
    }
    else if(strstr(buffer,"clear") != nullptr)
    {
        invalidateErrorData();
        Serial.println(F("Error list cleared"));
    }
    else if(strstr(buffer,"errorlist") != nullptr)
    {
        printErrorData();
    }
    else if(strstr(buffer,"enable") != nullptr)
    {
        toggleFallbackMode(false);
    }
    else if(strstr(buffer,"disable") != nullptr)
    {
        toggleFallbackMode(true);
    }
    else if(strstr(buffer,"sensors") != nullptr)
    {
        printSensorsInfo();
    }
    else if(strstr(buffer,"changemode") != nullptr)
    {
        setNextMode();
    }
    else if(strstr(buffer,"startpump") != nullptr)
    {
        setPump(true, true);
    }
    else if(strstr(buffer,"stoppump") != nullptr)
    {
        setPump(false, true);
    }
    else if(strstr(buffer,"openvalve") != nullptr)
    {
        setValve(true);
    }
    else if(strstr(buffer,"closevalve") != nullptr)
    {
        setValve(false);
    }
    else if(strstr(buffer,"reseton") != nullptr)
    {
        invalidateErrorData();
        currentMode = OnPressureTrigger;
        toggleFallbackMode(false);
        raiseError(NO_ERROR, F("Rebooting to complete reset and disable fallback mode"));
    }
    else if(strstr(buffer,"reset") != nullptr)
    {
        invalidateErrorData();
        currentMode = OnPressureTrigger;
        toggleFallbackMode(true);
        raiseError(NO_ERROR, F("Rebooting to complete reset and enable fallback mode"));
    }


#if MOCK_SENSORS
    else if(strstr(buffer,"e") != nullptr)
    {
        Serial.println(F("Enabling trigger"));
        triggerVal = true;
    }
    else if(strstr(buffer,"d") != nullptr)
    {
        Serial.println(F("Disabling trigger"));
        triggerVal = false;
    }
    else if(strstr(buffer,"n") != nullptr)
    {
        Serial.println(F("Button press set to NO_PULSE"));
        btnSt = NO_PULSE;
    }
    else if(strstr(buffer,"s") != nullptr)
    {
        Serial.println(F("Button press set to SHORT_PULSE"));
        btnSt = SHORT_PULSE;
    }
    else if(strstr(buffer,"l") != nullptr)
    {
        Serial.println(F("Button press set to LONG_PULSE"));
        btnSt = LONG_PULSE;
    }
    else
    {
        valveTemp = atoi(buffer);
        Serial.print(F("Setting valve temp to: ")); Serial.println(valveTemp);
    }
#endif
}

void handleCommsEvent()
{
    char commsBuffer[SC_MAX_MESSAGE_SIZE+1] = "";
    int res = comms.getNextCommand(commsBuffer, SC_MAX_MESSAGE_SIZE);
    if(res > 0)
    {
        if(strcmp(commsBuffer, ERRCMD) == 0)
        {
            if(comms.getNextArgument(commsBuffer, SC_MAX_MESSAGE_SIZE) <= 0)
            {
                snprintf_P(commsBuffer, SC_MAX_MESSAGE_SIZE, PSTR("No error message provided"));
            }
            handleHeaterError(COMMS_MAX_RETRIES, commsBuffer);
        }
        else
        {
            debug(F("handleCommsEvent: Unknown command: ")); debugln(commsBuffer);
        }
    }
    else if(res < 0)
    {
        Serial.println(F("handleCommsEvent: Error parsing message header"));
    }
}

void stepFSM()
{
    switch(currentStatus)
    {
        case ErrorFallBack_Begin:
            setPump(false, true);
            setValve(true);
            changeStatus(ErrorFallBack);
            break;

        case ErrorFallBack:
            {
                fadeAnimationStep(getRGBColor(Yellow),FALLBACK_MODE_ANIMATION_BRIGHTNESS_STEP);
                delay(FALLBACK_MODE_ANIMATION_FRAME_DELAY);
            }
            break;


        case OnPressureTrigger_Begin:
            setPump(false);
            setPumpTimeout(AUTO_DISABLE_PUMP_TIMEOUT);
            setValve(false);
            hotStart = false;
            changeStatus(OnPressureTrigger_WaitingCold);
            break;

        case OnPressureTrigger_WaitingCold:
            if(isTriggerActive())
            {
                setPump(true);

                changeStatus(OnPressureTrigger_TransitionToDrivingWater);
                hotStart = false;
                timeBeforeGettingHeaterTempMillis = millis();
                flashDrivingWaterColorMillis = timeBeforeGettingHeaterTempMillis;
                debug(F("Waiting ")); debug(TIME_BEFORE_GETTING_HEATER_TEMP/1000); debugln(F(" seconds to get accurate temperature readings"));
                progressMinTemp = static_cast<int>(getValveTemp()) - FADE_MIN_TEMP_OFFSET;
            }
            break;

        case OnPressureTrigger_TransitionToDrivingWater:
            {
                if(millis() - flashDrivingWaterColorMillis > FLASH_DRIVING_WATER_COLOR_PERIOD)
                {
                    static bool flash = false;
                    writeColor(flash?DRIVING_WATER_COLOR:Black);
                    flash = (!flash);
                    flashDrivingWaterColorMillis = millis();
                }
                if(millis() - timeBeforeGettingHeaterTempMillis > TIME_BEFORE_GETTING_HEATER_TEMP)
                {
                    heaterTempPMillis = 0;

                    changeStatus(OnPressureTrigger_DrivingWater);

                    getHeaterTempIfNecessary(&lastHeaterTemp);
                    if(valveTemp > getDesiredTemp(lastHeaterTemp))
                    {
                        debug(statusToString(currentStatus));debugln(F("\tHot start detected"));
                        hotStart = true;
                    }

                    heaterTempPMillis = 0;
                }
            }
            break;

        case OnPressureTrigger_DrivingWater:
            {
                getHeaterTempIfNecessary(&lastHeaterTemp);
                desiredTemp = getDesiredTemp(lastHeaterTemp);

                if(tempRequestReady)
                {
                    tempRequestReady = false;
                    long progress = map(static_cast<long>(valveTemp), progressMinTemp, static_cast<long>(desiredTemp), MIN_PROGRESS_VALUE, MAX_PROGRESS_VALUE);
                    debug(statusToString(currentStatus));debug(F("\tfadeMinTemp: ")); debug(progressMinTemp); debug(F("\tvalveTemp: ")); debug(valveTemp); debug(F("\tdesiredTemp: ")); debug(desiredTemp); debug(F("\tProgress: ")); debug((progress*100)/MAX_PROGRESS_VALUE); debug(F("% (")); debug(progress); debugln(F(")"));

                    writeColor(progress, 0, 255-progress);

                    if(valveTemp >= desiredTemp)
                    {
                        setPump(false);
                        setValve(true);

                        changeStatus(OnPressureTrigger_ServingWater);

                        desiredTemp = MIN_ALLOWED_TEMP;
                        maxTemp = MIN_ALLOWED_TEMP;
                        progressMinTemp = static_cast<int>(valveTemp);
                    }
                }
            }
            break;

        case OnPressureTrigger_ServingWater:
            {
                if(tempRequestReady)
                {
                    tempRequestReady = false;

                    if(valveTemp > maxTemp)
                    {
                        debug(statusToString(currentStatus));debug(F("\tmaxTemp updated from ")); debug(maxTemp); debug(F(" to ")); debugln(valveTemp);
                        maxTemp = valveTemp;
                        desiredTemp = getDesiredTemp(valveTemp);
                    }

                    debug(statusToString(currentStatus));debug(F("\tValve temp: ")); debug(valveTemp); debug(F("\tDesired temp: ")); debugln(desiredTemp);

                    if(!isTriggerActive() && valveTemp < desiredTemp)
                    {
                        setValve(false);
                        changeStatus(OnPressureTrigger_WaitingCold);
                    }
                }
            }
            break;


        case AlwaysActive_Begin:
            setPumpTimeout(AUTO_DISABLE_PUMP_TIMEOUT);
            setPump(true);
            setValve(false);

            changeStatus(AlwaysActive_TransitionToGettingHotWater);
            timeBeforeGettingHeaterTempMillis = millis();
            debug(F("Waiting ")); debug(TIME_BEFORE_GETTING_HEATER_TEMP/1000); debugln(F(" seconds to get accurate temperature readings"));
            progressMinTemp = static_cast<int>(getValveTemp()) - FADE_MIN_TEMP_OFFSET;
        break;

        case AlwaysActive_TransitionToGettingHotWater:
            if(millis() - timeBeforeGettingHeaterTempMillis > TIME_BEFORE_GETTING_HEATER_TEMP)
            {
                changeStatus(AlwaysActive_GettingHotWater);

                heaterTempPMillis = 0;

                getHeaterTempIfNecessary(&lastHeaterTemp);
                if(valveTemp > getDesiredTemp(lastHeaterTemp))
                {
                    debug(statusToString(currentStatus));debugln(F("\tHot start detected"));
                    hotStart = true;
                }

                heaterTempPMillis = 0;
            }
            break;

        case AlwaysActive_GettingHotWater:
            {
                getHeaterTempIfNecessary(&lastHeaterTemp);
                desiredTemp = getDesiredTemp(lastHeaterTemp);

                fadeAnimationStep(getRGBColor(ALWAYS_ACTIVE_WATER_COLOR), ALWAYS_ACTIVE_MODE_ANIMATION_BRIGHTNESS_STEP);
                delay(ALWAYS_ACTIVE_MODE_ANIMATION_FRAME_DELAY);

                if(tempRequestReady)
                {
                    tempRequestReady = false;
                    long progress = map(static_cast<long>(valveTemp), progressMinTemp, static_cast<long>(desiredTemp), MIN_PROGRESS_VALUE, MAX_PROGRESS_VALUE);
                    debug(statusToString(currentStatus));debug(F("\tinitialTemp: ")); debug(progressMinTemp); debug(F("\tvalveTemp: ")); debug(valveTemp); debug(F("\tdesiredTemp: ")); debug(desiredTemp); debug(F("\tProgress: ")); debug((progress*100)/MAX_PROGRESS_VALUE); debug(F("% (")); debug(progress); debugln(F(")"));

                    if(valveTemp >= desiredTemp)
                    {
                        setPump(false);
                        setValve(true);

                        changeStatus(AlwaysActive_Idle);

                        desiredTemp = MIN_ALLOWED_TEMP;
                        maxTemp = MIN_ALLOWED_TEMP;
                        progressMinTemp = static_cast<int>(valveTemp);
                    }
                }
            }
            break;

        case AlwaysActive_Idle:
            if(tempRequestReady)
            {
                tempRequestReady = false;

                if(valveTemp > maxTemp)
                {
                    debug(statusToString(currentStatus));debug(F("\tmaxTemp updated from ")); debug(maxTemp); debug(F(" to ")); debugln(valveTemp);
                    maxTemp = valveTemp;
                    desiredTemp = getDesiredTemp(valveTemp);
                }

                debug(statusToString(currentStatus));debug(F("\tValve temp: ")); debug(valveTemp); debug(F("\tDesired temp: ")); debugln(desiredTemp);

                if(!isTriggerActive() && valveTemp < desiredTemp)
                {
                    changeStatus(AlwaysActive_TransitionToGettingHotWater);
                    hotStart = false;
                    timeBeforeGettingHeaterTempMillis = millis();
                    debug(F("Waiting ")); debug(TIME_BEFORE_GETTING_HEATER_TEMP/1000); debugln(F(" seconds to get accurate temperature readings"));
                    progressMinTemp = static_cast<int>(getValveTemp()) - FADE_MIN_TEMP_OFFSET;
                }
            }
            break;
    }
}

void connectToHeater(bool ignoreErrors = false)
{
    #if !DISABLE_WATCHDOGS
    char commsBuffer[SC_MAX_MESSAGE_SIZE+1];
    unsigned long connectionPMillis = millis();
    bool connected = false;

    Serial.print(F("Connecting to heater MCU...\tTimeout in ")); Serial.println(formattedTime(INIT_CONNECTION_TIMEOUT, commsBuffer));

    while(!connected && (millis() - connectionPMillis < INIT_CONNECTION_TIMEOUT))
    {
        wdt_reset();
        delay(20);

        #if DEBUGCONNECT
        debugln(F("Sending Watchdog Reset CMD"));
        #endif

        comms.sendCommand(WTDRSTCMD, nullptr, 0);

        delay(WDT_RST_MESSAGE_PROCESSING_WAIT_TIME);

        if(comms.getNextCommand(commsBuffer, SC_MAX_MESSAGE_SIZE) > 0)
        {
            #if DEBUGCONNECT
            debugln(F("Received WTD-RST response"));
            #endif
            if(strcmp(commsBuffer, OKCMD) == 0)
            {
                connected = true;
            }
            else if(!ignoreErrors)
            {
                char errorBuff[ERROR_MESSAGE_SIZE];
                snprintf_P(errorBuff, ERROR_MESSAGE_SIZE, PSTR("Unexpected response from the HEATER MCU at connectToHeater: %s"), commsBuffer);
                raiseError(ERROR_COMMS_UNEXPECTED_MESSAGE, errorBuff);
            }
        }
        #if DEBUGCONNECT
        else
        {
            debugln(F("Timeout receiving WTD-RST response, ignoring it"));
        }
        #endif

        if(Serial.available())
        {
            serialEvent();
        }
    }

    if(!connected)
    {
        if(!ignoreErrors)
        {
            raiseError(ERROR_COMMS_CONNECTION_NOT_ESTABLISHED, F("Timeout connecting to the HEATER MCU"));
        }
        else
        {
            Serial.println(F("Timeout connecting to the HEATER MCU"));
        }
    }
    else
    {
        Serial.print(F("Connection established with heater MCU in ")); Serial.println(formattedTime(static_cast<long>(millis() - connectionPMillis), commsBuffer));
    }
    wdt_reset();
    delay(1000);
    #endif
}

void setup()
{
    wdt_disable(); /* Disable the watchdog and wait for more than 8 seconds*/

    pinMode(VALVE_RELAY_PIN,OUTPUT);
    bool fallbackModeEnabled;
    EEPROM.get(EEPROM_FALLBACK_MODE_ENABLED_ADDRESS, fallbackModeEnabled);
    setValve(fallbackModeEnabled);

    pinMode(RED_LED_PIN, OUTPUT);
    pinMode(GREEN_LED_PIN, OUTPUT);
    pinMode(BLUE_LED_PIN, OUTPUT);

    #if ENABLE_HEARTBEAT
        pinMode(HEARTBEAT_PIN, OUTPUT);
        digitalWrite(HEARTBEAT_PIN, 1);
    #endif

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    writeColor(WDT_BOOT_DELAY_COLOR);

    #if !DISABLE_WATCHDOGS
    delay(10000); /* Done so that the Arduino doesn't keep resetting infinitely if wrong configuration.*/
    wdt_enable(WDTO_8S); /* Enable the watchdog with a timeout of 8 seconds*/
    #endif

    Serial.begin(SERIAL_USB_BAUD_RATE); // Used for debug
    delay(3000);
    writeColor(White);

    unsigned long startMillis = millis();
    while(millis() - startMillis < 3000)
    {
        if(readButton(true) == LONG_PULSE)
        {
            wdt_disable();
            debugln(F("Long pulse detected, entering emergency shutdown mode"));
            writeColor(Black);

            bool valveSt = false;
            setValve(valveSt);

            byte colorSt = 0;
            unsigned long colorMillis = millis();

            while (readButton(true) != NO_PULSE);

            while (true)
            {
                if(millis() - colorMillis > FLASH_EMERGENCY_SHUTDOWN_COLOR_PERIOD)
                {
                    colorMillis = millis();
                    switch (colorSt)
                    {
                        case 0:
                            writeColor(EMERGENCY_SHUTDOWN_1_COLOR);
                            break;
                        case 1:
                            writeColor(Black);
                            break;
                        case 2:
                            writeColor(EMERGENCY_SHUTDOWN_2_COLOR);
                            break;
                        case 3:
                            writeColor(Black);
                            break;
                        default:
                            break;
                    }
                    colorSt = (colorSt + 1) % 4;
                }

                switch (readButton(true))
                {
                    case SHORT_PULSE:
                        valveSt = !valveSt;
                        setValve(valveSt);
                        break;
                    case LONG_PULSE:
                        raiseError(NO_ERROR, F("Rebooting by user command (button long press)"));
                    default:
                        break;
                }
            }
        }
    }

    writeColor(BOOT_COLOR);

    Serial.println(F("\n------------------------------------------"  ));
    Serial.println(F(  "|                SHWRS-VS                |"  ));
    Serial.println(F(  "|                 " VS "                 |"  ));
    Serial.println(F(  "------------------------------------------\n"));
    wdt_reset();

    printSystemInfo();

    printErrorData();

    Serial.println(F("Starting..."));

    #if SC_USE_HAMMING_7_4_CORRECTION_CODE
        Serial.println(F("INFO: HAMMING 7,4 CORRECTION CODE ENABLED FOR RS485 COMMUNICATION OVER SERIAL1"));
        Serial2.begin(RS485_SERIAL_BAUD_RATE, SERIAL_7N1);
        Serial2.setTimeout(RECEIVED_MESSAGE_TIMEOUT);
    #else
        Serial2.begin(RS485_SERIAL_BAUD_RATE);
        Serial2.setTimeout(RECEIVED_MESSAGE_TIMEOUT);
    #endif

    Serial.print(F("\nINFO: COMMUNICATION OVER SERIAL1 ENABLED WITH A SPEED OF ")); Serial.print(RS485_SERIAL_BAUD_RATE); Serial.println(F(" BAUDS"));

    #if !MOCK_SENSORS
        #ifdef __AVR_ATmega2560__
          analogReference(INTERNAL1V1);
        #else
          analogReference(INTERNAL);
        #endif
        tempSensor.begin();
        tempSensor.setWaitForConversion(false);
        VALVE_TEMP_WAIT_FROM_REQUEST_TO_READ = TEMP_SENSOR_ADDITIONAL_CONVERSION_TIME + DallasTemperature::millisToWaitForConversion(tempSensor.getResolution());
    #endif

    if(fallbackModeEnabled)
    {
        changeMode(ErrorFallBackMode);
    }
    else
    {
        EEPROM.get(EEPROM_MODE_ADDRESS, currentMode);
        if(currentMode >= NUM_OF_MODES || currentMode < 0)
        {
            debugln(F("WARNING: Invalid Mode stored in EEPROM, setting to OnPressureTrigger"));
            changeMode(OnPressureTrigger);
            EEPROM.put(EEPROM_MODE_ADDRESS, currentMode);
        }
        toggleFallbackMode(false);
    }

    writeColor(BOOT_COLOR);
    delay(1000);

    if(fallbackModeEnabled)
    {
        Serial.println(F("\nWARNING: SYSTEM IN ERROR FALLBACK MODE\n\n"));
        connectToHeater(true);
    }
    else
    {
        connectToHeater();
    }

    Serial.println(F("System successfully started!!!"));

    delay(500);
    printSensorsInfo();
    wdt_reset();

    #if MOCK_SENSORS
        Serial.println(F("WARNING: SENSOR MOCKING ENABLED"));
    #endif

    Serial.println(F(MAIN_HELP_STRING));
    #if MOCK_SENSORS
        Serial.println(F(MOCK_SENSORS_HELP_STRING));
    #endif
    Serial.println();

    loadProfilerData();
    printProfilerData();

    Serial.println(F("Error list:"));
    printErrorData();

    Serial.println();

    delay(1000);

    resetWatchdogs();

    #if ENABLE_HEARTBEAT
        digitalWrite(HEARTBEAT_PIN, 0);
    #endif

    writeColor(statusToColor(currentStatus));

    if(Serial.available())
    {
        serialEvent();
    }
}

void loop()
{
    #if PROFILER_ENABLED
        profilerStartMeasure();
    #endif

    resetWatchdogsIfNecessary();
    requestValveTempIfNecessary();
    getValveTempIfNecessary(currentMode == ErrorFallBackMode);
    switch (readButton())
    {
        case NO_PULSE:
            break;
        case SHORT_PULSE:
            if(currentMode != ErrorFallBackMode)
            {
                writeColor(USER_ACK_COLOR);
                delay(2000);
                setNextMode();
            }
            break;
        case LONG_PULSE:
            writeColor(USER_ACK_COLOR);
            delay(2000);
            toggleFallbackMode(currentMode != ErrorFallBackMode);
            break;
    }
    stepFSM();
    handleCommsEvent();
    #if ENABLE_AUTO_RESTART
        checkResetTime();
    #endif

    #if PROFILER_ENABLED
        int profilerRes = profilerEndMeasure();

        switch (profilerRes)
        {
            case 1:
                sprintf(profilerData.maxTimeData, "Status: %s", statusToString(currentStatus));
                #if DEBUG
                    debugln(F("Max time reached in this iteration"));
                    printProfilerData();
                #endif
                saveProfilerData();
                break;
            case -1:
                sprintf(profilerData.minTimeData, "Status: %s", statusToString(currentStatus));
                #if DEBUG
                        debugln(F("Min time reached in this iteration"));
                        printProfilerData();
                #endif
                saveProfilerData();
                break;
            default:
                break;
        }
    #endif
}
