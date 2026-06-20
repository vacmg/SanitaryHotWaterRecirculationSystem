//
// Created by varzoz on 20/6/26.
//

#include "Config.h"

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
