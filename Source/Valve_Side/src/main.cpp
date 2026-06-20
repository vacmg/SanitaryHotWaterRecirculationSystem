// UART LINK: Arduino Mega <-> ESP32 <-> Valve MCU
//  +--------------------+      UART      +------------------+      CAN      +----------------------------+
//  |   Arduino Mega     |                |      ESP32       |               |     Heater MCU             |
//  |   Serial2 TX  ---->+--------------> |  RX 32 CAN RX 23 | <-----------< |     CAN TX  CANH -> Verde  |
//  |   Serial2 RX  <----+--------------< |  TX 33 CAN TX 22 | >-----------> |     CAN RX  CANL -> Blanco |
//  +--------------------+                +------------------+               +----------------------------+

#include <Arduino.h>
#include "Config.h"
#include <EEPROM.h>
#include "Static.h"
#include "Error.h"
#include "Pinout.h"
#include "Actuators.h"
#include "Sensors.h"
#include "Button.h"
#include "Utils.h"
#include "Remote.h"
#include "Profiler.h"
#include "Watchdogs.h"


#if ENABLE_AUTO_RESTART
void checkResetTime()
{
    if((currentMode != ErrorFallBackMode) && (millis() > SYSTEM_RESET_PERIOD))
    {
        raiseError(NO_ERROR, F("INFO: Restarting the system due to SYSTEM_RESET_PERIOD timeout"));
    }
}
#endif


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
