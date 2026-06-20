//
// Created by varzoz on 20/6/26.
//

#include "Config.h"
#include "Watchdogs.h"
#include "Static.h"
#include "Pinout.h"

#include "../include/Error.h"
#include<avr/wdt.h> /* Header for watchdog timers in AVR*/

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