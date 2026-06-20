//
// Created by varzoz on 20/6/26.
//

#include "Config.h"
#include "Static.h"
#include "Error.h"
#include "Utils.h"
#include "Remote.h"
#include "Actuators.h"

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

void connectToHeater(bool ignoreErrors)
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
