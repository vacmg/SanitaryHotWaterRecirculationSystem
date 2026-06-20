//
// Created by varzoz on 20/6/26.
//

#include "Config.h"
#include "Actuators.h"
#include "Static.h"
#include "Error.h"
#include "Pinout.h"

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