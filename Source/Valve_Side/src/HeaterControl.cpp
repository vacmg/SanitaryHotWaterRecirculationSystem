#include "HeaterControl.h"
#include "Config.h"
#include "Globals.h"
#include "Errors.h"
#include "Utils.h"
#include "Button.h"

void setValve(bool enable)
{
    digitalWrite(VALVE_RELAY_PIN, enable?RELAY_ENABLED:RELAY_DISABLED);
}

void setPump(bool enable, bool ignoreErrors)
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

int getHeaterTemp(bool ignoreErrors)
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
            // serialEvent() will be defined in main.cpp, but it handles user input.
            // Since serialEvent() is defined elsewhere, we can just declare it or use serialEvent() directly.
            // Wait, does Arduino framework run serialEvent automatically, or can we call it?
            // Yes, calling it directly requires a forward declaration.
            extern void serialEvent();
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
