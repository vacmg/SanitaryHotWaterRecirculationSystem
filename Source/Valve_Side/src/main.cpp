// UART LINK: Arduino Mega <-> ESP32 <-> Valve MCU
//  +--------------------+      UART      +------------------+      CAN      +----------------------------+
//  |   Arduino Mega     |                |      ESP32       |               |     Heater MCU             |
//  |   Serial2 TX  ---->+--------------> |  RX 32 CAN RX 23 | <-----------< |     CAN TX  CANH -> Verde  |
//  |   Serial2 RX  <----+--------------< |  TX 33 CAN TX 22 | >-----------> |     CAN RX  CANL -> Blanco |
//  +--------------------+                +------------------+               +----------------------------+

#include <Arduino.h>
#include "Config.h"
#include <avr/wdt.h> /* Header for watchdog timers in AVR*/
#include <EEPROM.h>
#include <MAX_RS485.h>
#include "SimpleComms.h"

// Include extracted headers
#include "Types.h"
#include "Globals.h"
#include "Utils.h"
#include "Leds.h"
#include "Button.h"
#include "Sensors.h"
#include "HeaterControl.h"
#include "Errors.h"
#include "FSM.h"

#define MAIN_HELP_STRING "\nType 'reboot' to restart the system;\n'enable' or 'disable' to disable or enable the fallback mode;\n'clear' to invalidate the Error Register;\n'sensors' to print all the sensors current value;\n'changemode' to change the operation mode to the next one (similar to pressing the button);\n'startpump' or 'stoppump' to manually start or stop the pump;\n'openvalve' or 'closevalve' to manually open or close the valve;\n'errorlist' to print the error list;\n'reset' or 'reseton' to clear the error list and enable or disable the fallback mode"
#define MOCK_SENSORS_HELP_STRING "\nPress 'e' or 'd' to enable or disable trigger;\nPress 'n', 's' or 'l' to set the button to NO_PULSE, SHORT_PULSE or LONG_PULSE;\nSend a number to incorporate it as the valve temp\n"

// Global instances
SimpleComms comms(&Serial2, HEADER);

// Global state variables
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

bool hotStart = false;
int lastHeaterTemp = 0;

int progressMinTemp = 0;
float desiredTemp = 0;
float maxTemp = 0;

unsigned long valveTempRequestTempMillis = 0;
bool valveTempRequested = false;
unsigned long VALVE_TEMP_WAIT_FROM_REQUEST_TO_READ = 0;
float valveTemp = 0;
bool tempRequestReady = false;

#if PROFILER_ENABLED
unsigned long profilerMillis = 0;
const ProfilerData defaultProfilerData = {0XABDCEF12, INT32_MAX, 0, "", ""};
ProfilerData profilerData = defaultProfilerData;
#endif

// Function prototypes
void printSystemInfo();
void printSensorsInfo();
void setNextMode();
#if ENABLE_AUTO_RESTART
void checkResetTime();
#endif

void printSystemInfo()
{
    char buff[64];
    Serial.print(F("Remaining time until next restart: ")); Serial.println(formattedTime(SYSTEM_RESET_PERIOD - static_cast<long>(millis()), buff));
    Serial.print(F("Watchdogs reset period: ")); Serial.println(formattedTime(WATCHDOG_RESET_PERIOD, buff));
    Serial.print(F("FallBack Mode ")); Serial.println(currentMode==ErrorFallBackMode?"Enabled":"Disabled");
    Serial.print(F("Comms max retries: ")); Serial.println(COMMS_MAX_RETRIES);
    Serial.print(F("Error message max length: ")); Serial.println(ERROR_MESSAGE_SIZE);
    Serial.print(F("Comms message max length: ")); Serial.println(SC_MAX_MESSAGE_SIZE);
    Serial.print(F("Mode: ")); Serial.println(modeToString(currentMode));
    Serial.print(F("Status: ")); Serial.println(statusToString(currentStatus));
    Serial.print(F("Hot Start: ")); Serial.println(hotStart?F("ACTIVE"):F("INACTIVE"));
}

void printSensorsInfo()
{
    Serial.println(F("\n-----------------------------------------"));
    printSystemInfo();
    Serial.println(F("Sensor list:\n"));
    #if MOCK_SENSORS
    Serial.println(F("Valve pressure sensor: MOCKED"));
    Serial.print(F("Valve temperature sensor: MOCKED to ")); Serial.print(valveTemp);Serial.println(F("ºC"));
    #else
    Serial.print(F("Valve pressure sensor: ")); Serial.print(getValvePressure(true));Serial.println(F("BAR"));
    Serial.print(F("Valve temperature sensor: ")); Serial.print(getValveTemp(true));Serial.println(F("ºC"));
    #endif
    int heaterTemp = getHeaterTemp(true);
    Serial.print(F("Heater temperature sensor: ")); Serial.print(heaterTemp);Serial.println(F("ºC"));
    Serial.print(F("Desired temperature: ")); Serial.print(getDesiredTemp(heaterTemp));Serial.println(F("ºC"));

    Serial.println(F("-----------------------------------------\n"));
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

void setup()
{
    wdt_disable(); /* Disable the watchdog and wait for more than 8 seconds*/

    pinMode(VALVE_RELAY_PIN, OUTPUT);
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

    Serial.println(F("\n------------------------------------------"));
    Serial.println(F("|                SHWRS-VS                |"));
    Serial.println(F("|                 " VS "                 |"));
    Serial.println(F("------------------------------------------\n"));
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

    // Initialize sensors
    sensorsInit();

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
