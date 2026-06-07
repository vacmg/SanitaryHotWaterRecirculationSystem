// UART LINK: Arduino Mega <-> ESP32 <-> Valve MCU
//  +--------------------+      UART      +------------------+      CAN      +----------------------------+
//  |   Arduino Mega     |                |      ESP32       |               |     Heater MCU             |
//  |   Serial2 TX  ---->+--------------> |  RX 32 CAN RX 23 | <-----------< |     CAN TX  CANH -> Verde  |
//  |   Serial2 RX  <----+--------------< |  TX 33 CAN TX 22 | >-----------> |     CAN RX  CANL -> Blanco |
//  +--------------------+                +------------------+               +----------------------------+

#include <Arduino.h>
#include "Config.h"
#include "Pinout.h"
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

#define MAIN_HELP_STRING "\nType 'reboot' to restart the system;\n'enable' or 'disable' to disable or enable the fallback mode;\n'clear' to invalidate the Error Register;\n'sensors' to print all the sensors current value;\n'systeminfo' to print the system configuration;\n'changemode' to change the operation mode to the next one (similar to pressing the button);\n'startpump' or 'stoppump' to manually start or stop the pump;\n'openvalve' or 'closevalve' to manually open or close the valve;\n'errorlist' to print the error list;\n'reset' or 'reseton' to clear the error list and enable or disable the fallback mode"
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

    // --- Runtime state ---
    Serial.println(F("[ Runtime State ]"));
    Serial.print(F("  Version:                       ")); Serial.println(F(VS));
    Serial.print(F("  Build ID:                      ")); Serial.println(static_cast<unsigned long>(EEPROM_BUILD_ID));
    Serial.print(F("  Mode:                          ")); Serial.println(modeToString(currentMode));
    Serial.print(F("  Status:                        ")); Serial.println(statusToString(currentStatus));
    Serial.print(F("  FallBack Mode:                 ")); Serial.println(currentMode == ErrorFallBackMode ? F("Enabled") : F("Disabled"));
    Serial.print(F("  Hot Start:                     ")); Serial.println(hotStart ? F("ACTIVE") : F("INACTIVE"));
    Serial.print(F("  Uptime:                        ")); Serial.println(formattedTime(millis(), buff));
    Serial.print(F("  Time until next restart:       ")); Serial.println(formattedTime(SYSTEM_RESET_PERIOD - static_cast<long>(millis()), buff));

    // --- Feature flags ---
    Serial.println(F("[ Feature Flags ]"));
    Serial.print(F("  DEBUG:                         ")); Serial.println(DEBUG);
    Serial.print(F("  DEBUGWATCHDOG:                 ")); Serial.println(DEBUGWATCHDOG);
    Serial.print(F("  DEBUGTEMP:                     ")); Serial.println(DEBUGTEMP);
    Serial.print(F("  DEBUGCONNECT:                  ")); Serial.println(DEBUGCONNECT);
    Serial.print(F("  PROFILER_ENABLED:              ")); Serial.println(PROFILER_ENABLED);
    Serial.print(F("  ENABLE_HEARTBEAT:              ")); Serial.println(ENABLE_HEARTBEAT);
    Serial.print(F("  DISABLE_WATCHDOGS:             ")); Serial.println(DISABLE_WATCHDOGS);
    Serial.print(F("  MOCK_SENSORS:                  ")); Serial.println(MOCK_SENSORS);
    Serial.print(F("  SC_USE_HAMMING_7_4:            ")); Serial.println(SC_USE_HAMMING_7_4_CORRECTION_CODE);
    Serial.print(F("  ENABLE_AUTO_RESTART:           ")); Serial.println(ENABLE_AUTO_RESTART);
    Serial.print(F("  EEPROM_DONT_WRITE_ERRORS:      ")); Serial.println(EEPROM_DONT_WRITE_ERRORS);
    Serial.print(F("  EEPROM_CLEAR_ON_BUILD_ID:      ")); Serial.println(EEPROM_CLEAR_ON_BUILD_ID_CHANGE);

    // --- EEPROM layout ---
    Serial.println(F("[ EEPROM Layout ]"));
    Serial.print(F("  EEPROM_MODE_ADDRESS:           ")); Serial.println(EEPROM_MODE_ADDRESS);
    Serial.print(F("  EEPROM_FALLBACK_ADDR:          ")); Serial.println(EEPROM_FALLBACK_MODE_ENABLED_ADDRESS);
    Serial.print(F("  EEPROM_ERROR_COUNTER_ADDR:     ")); Serial.println(EEPROM_ERROR_COUNTER_ADDRESS);
    Serial.print(F("  EEPROM_ERROR_START_ADDR:       ")); Serial.println(EEPROM_ERROR_START_ADDRESS);
    Serial.print(F("  EEPROM_ERROR_HEADER_ADDR:      ")); Serial.println(EEPROM_ERROR_HEADER_ADDRESS);
    Serial.print(F("  EEPROM_ERROR_ENTRIES_ADDR:     ")); Serial.println(EEPROM_ERROR_ENTRIES_START_ADDRESS);
    Serial.print(F("  EEPROM_ERROR_LAYOUT_VERSION:   ")); Serial.println(EEPROM_ERROR_LAYOUT_VERSION);
    Serial.print(F("  PROFILER_DATA_START_ADDR:      ")); Serial.println(PROFILER_DATA_START_ADDRESS);
    Serial.print(F("  Error slot size (bytes):       ")); Serial.println(sizeof(EEPROMError));
    Serial.print(F("  Error slots:                   ")); Serial.println(EEPROM_ERROR_MEMORY_ITEMS);
    Serial.print(F("  Error msg max length:          ")); Serial.println(ERROR_MESSAGE_SIZE);
    Serial.print(F("  Error file field size:         ")); Serial.println(EEPROM_ERROR_FILE_SIZE);

    // --- Fallback / error counter ---
    Serial.println(F("[ Fallback / Error Counter ]"));
    Serial.print(F("  raiseError counter:            ")); Serial.println(getRaiseErrorCounter());
    Serial.print(F("  Fallback threshold (>):        ")); Serial.println(ERROR_COUNT_TO_ENABLE_FALLBACK_MODE);
    Serial.print(F("  Counter reset timeout (ms):    ")); Serial.println(FALLBACK_ERROR_COUNTER_RESET_TIMEOUT_MS);

    // --- Watchdog / restart ---
    Serial.println(F("[ Watchdog / Restart ]"));
    Serial.print(F("  Watchdog reset period (ms):    ")); Serial.println(formattedTime(WATCHDOG_RESET_PERIOD, buff));
    Serial.print(F("  System reset period:           ")); Serial.println(formattedTime(SYSTEM_RESET_PERIOD, buff));

    // --- Communications ---
    Serial.println(F("[ Communications ]"));
    Serial.print(F("  USB baud rate:                 ")); Serial.println(SERIAL_USB_BAUD_RATE);
    Serial.print(F("  UART baud rate:                ")); Serial.println(UART_SERIAL_BAUD_RATE);
    Serial.print(F("  Comms header:                  ")); Serial.println(HEADER);
    Serial.print(F("  Comms max retries:             ")); Serial.println(COMMS_MAX_RETRIES);
    Serial.print(F("  Comms msg max length:          ")); Serial.println(SC_MAX_MESSAGE_SIZE);
    Serial.print(F("  Received msg timeout (ms):     ")); Serial.println(RECEIVED_MESSAGE_TIMEOUT);
    Serial.print(F("  Pump msg proc wait (ms):       ")); Serial.println(PUMP_MESSAGE_PROCESSING_WAIT_TIME);
    Serial.print(F("  PumpTimeout msg proc wait(ms): ")); Serial.println(PUMP_TIMEOUT_MESSAGE_PROCESSING_WAIT_TIME);
    Serial.print(F("  Temp msg proc wait (ms):       ")); Serial.println(TEMP_MESSAGE_PROCESSING_WAIT_TIME);
    Serial.print(F("  WDT-RST msg proc wait (ms):    ")); Serial.println(WDT_RST_MESSAGE_PROCESSING_WAIT_TIME);
    Serial.print(F("  Init connection timeout (ms):  ")); Serial.println(formattedTime(INIT_CONNECTION_TIMEOUT, buff));
    Serial.print(F("  Auto disable pump timeout(ms): ")); Serial.println(formattedTime(AUTO_DISABLE_PUMP_TIMEOUT, buff));

    // --- Temperature ---
    Serial.println(F("[ Temperature ]"));
    Serial.print(F("  Min allowed temp (C):          ")); Serial.println(MIN_ALLOWED_TEMP);
    Serial.print(F("  Max allowed temp (C):          ")); Serial.println(MAX_ALLOWED_TEMP);
    Serial.print(F("  Hot water temp multiplier:     ")); Serial.println(HOT_WATER_TEMPERATURE_MULTIPLIER);
    Serial.print(F("  Cold water temp multiplier:    ")); Serial.println(COLD_WATER_TEMPERATURE_MULTIPLIER);
    Serial.print(F("  Heater temp gather period(ms): ")); Serial.println(HEATER_TEMP_GATHERING_PERIOD);
    Serial.print(F("  Valve temp gather period (ms): ")); Serial.println(VALVE_TEMP_GATHERING_PERIOD);
    Serial.print(F("  Time before heater temp (ms):  ")); Serial.println(TIME_BEFORE_GETTING_HEATER_TEMP);
    Serial.print(F("  Temp sensor conv time (ms):    ")); Serial.println(TEMP_SENSOR_ADDITIONAL_CONVERSION_TIME);

    // --- Pressure ---
    Serial.println(F("[ Pressure ]"));
    Serial.print(F("  Pressure sensor min (bar):     ")); Serial.println(PRESSURE_SENSOR_MIN_BAR);
    Serial.print(F("  Pressure sensor max (bar):     ")); Serial.println(PRESSURE_SENSOR_MAX_BAR);
    Serial.print(F("  Water min normal press (bar):  ")); Serial.println(WATER_MIN_NORMAL_PRESSURE_BAR);
    Serial.print(F("  Pressure current min (mA):     ")); Serial.println(PRESSURE_SENSOR_CURRENT_MIN_mA);
    Serial.print(F("  Pressure current max (mA):     ")); Serial.println(PRESSURE_SENSOR_CURRENT_MAX_mA);
    Serial.print(F("  Min allowed current (mA):      ")); Serial.println(MIN_ALLOWED_PRESSURE_SENSOR_CURRENT_mA);
    Serial.print(F("  Max allowed current (mA):      ")); Serial.println(MAX_ALLOWED_PRESSURE_SENSOR_CURRENT_mA);

    // --- LED / Animation ---
    Serial.println(F("[ LED / Animation ]"));
    Serial.print(F("  Min progress value:            ")); Serial.println(MIN_PROGRESS_VALUE);
    Serial.print(F("  Max progress value:            ")); Serial.println(MAX_PROGRESS_VALUE);
    Serial.print(F("  Fade min temp offset:          ")); Serial.println(FADE_MIN_TEMP_OFFSET);
    Serial.print(F("  Fallback anim frame delay(ms): ")); Serial.println(FALLBACK_MODE_ANIMATION_FRAME_DELAY);
    Serial.print(F("  AlwaysOn anim frame delay(ms): ")); Serial.println(ALWAYS_ACTIVE_MODE_ANIMATION_FRAME_DELAY);
    Serial.print(F("  Fallback anim brightness step: ")); Serial.println(FALLBACK_MODE_ANIMATION_BRIGHTNESS_STEP);
    Serial.print(F("  AlwaysOn anim brightness step: ")); Serial.println(ALWAYS_ACTIVE_MODE_ANIMATION_BRIGHTNESS_STEP);
    Serial.print(F("  Flash driving color period(ms):")); Serial.println(FLASH_DRIVING_WATER_COLOR_PERIOD);
    Serial.print(F("  Flash emergency period (ms):   ")); Serial.println(FLASH_EMERGENCY_SHUTDOWN_COLOR_PERIOD);

    // --- Button ---
    Serial.println(F("[ Button ]"));
    Serial.print(F("  Long press time (ms):          ")); Serial.println(BUTTON_LONG_PRESSED_TIME);
    Serial.print(F("  Short press min time (ms):     ")); Serial.println(BUTTON_SHORT_PRESSED_MIN_TIME);

    // --- Pinout ---
    Serial.println(F("[ Pinout ]"));
    Serial.print(F("  RECEIVER_ENABLE_PIN:           ")); Serial.println(RECEIVER_ENABLE_PIN);
    Serial.print(F("  DRIVE_ENABLE_PIN:              ")); Serial.println(DRIVE_ENABLE_PIN);
    Serial.print(F("  VALVE_RELAY_PIN:               ")); Serial.println(VALVE_RELAY_PIN);
    Serial.print(F("  RED_LED_PIN:                   ")); Serial.println(RED_LED_PIN);
    Serial.print(F("  GREEN_LED_PIN:                 ")); Serial.println(GREEN_LED_PIN);
    Serial.print(F("  BLUE_LED_PIN:                  ")); Serial.println(BLUE_LED_PIN);
    Serial.print(F("  BUTTON_PIN:                    ")); Serial.println(BUTTON_PIN);
    Serial.print(F("  HEARTBEAT_PIN:                 ")); Serial.println(HEARTBEAT_PIN);
    Serial.print(F("  PRESSURE_SENSOR:               ")); Serial.println(PRESSURE_SENSOR);
    Serial.print(F("  TEMP_SENSOR:                   ")); Serial.println(TEMP_SENSOR);
}

void printSensorsInfo()
{
    Serial.println(F("\n-----------------------------------------"));
    Serial.println(F("[ Sensors ]\n"));
    #if MOCK_SENSORS
    Serial.println(F("  Valve pressure sensor: MOCKED"));
    Serial.print(F("  Valve temperature sensor: MOCKED to ")); Serial.print(valveTemp); Serial.println(F("ºC"));
    #else
    Serial.print(F("  Valve pressure sensor: ")); Serial.print(getValvePressure(true)); Serial.println(F(" BAR"));
    Serial.print(F("  Valve temperature sensor: ")); Serial.print(getValveTemp(true)); Serial.println(F("ºC"));
    #endif
    int heaterTemp = getHeaterTemp(true);
    Serial.print(F("  Heater temperature sensor: ")); Serial.print(heaterTemp); Serial.println(F("ºC"));
    Serial.print(F("  Desired temperature: ")); Serial.print(getDesiredTemp(heaterTemp)); Serial.println(F("ºC"));
    Serial.println(F("-----------------------------------------\n"));
}

#if ENABLE_AUTO_RESTART
void checkResetTime()
{
    if((currentMode != ErrorFallBackMode) && (millis() > SYSTEM_RESET_PERIOD))
    {
        raiseErrorWithTemplate(NO_ERROR, ERROR_MSG_TEMPLATE_INFO_SYSTEM_RESET_TIMEOUT);
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
        raiseErrorWithTemplate(NO_ERROR, ERROR_MSG_TEMPLATE_INFO_REBOOT_USER_COMMAND);
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
    else if(strstr(buffer,"systeminfo") != nullptr)
    {
        printSystemInfo();
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
        clearProfilerData();
        currentMode = OnPressureTrigger;
        toggleFallbackMode(false);
        raiseErrorWithTemplate(NO_ERROR, ERROR_MSG_TEMPLATE_INFO_REBOOT_RESET_DISABLE_FALLBACK);
    }
    else if(strstr(buffer,"reset") != nullptr)
    {
        invalidateErrorData();
        clearProfilerData();
        currentMode = OnPressureTrigger;
        toggleFallbackMode(true);
        raiseErrorWithTemplate(NO_ERROR, ERROR_MSG_TEMPLATE_INFO_REBOOT_RESET_ENABLE_FALLBACK);
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
                        raiseErrorWithTemplate(NO_ERROR, ERROR_MSG_TEMPLATE_INFO_REBOOT_BUTTON_LONG_PRESS);
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
        Serial.println(F("INFO: HAMMING 7,4 CORRECTION CODE ENABLED FOR UART COMMUNICATION OVER SERIAL1"));
        Serial2.begin(UART_SERIAL_BAUD_RATE, SERIAL_7N1);
        Serial2.setTimeout(RECEIVED_MESSAGE_TIMEOUT);
    #else
        Serial2.begin(UART_SERIAL_BAUD_RATE);
        Serial2.setTimeout(RECEIVED_MESSAGE_TIMEOUT);
    #endif

    Serial.print(F("\nINFO: COMMUNICATION OVER SERIAL1 ENABLED WITH A SPEED OF ")); Serial.print(UART_SERIAL_BAUD_RATE); Serial.println(F(" BAUDS"));

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
    resetRaiseErrorCounterIfTimeoutElapsed();

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
