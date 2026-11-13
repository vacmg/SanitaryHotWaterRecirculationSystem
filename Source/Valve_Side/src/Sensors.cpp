//
// Created by varzoz on 20/6/26.
//

#include "Config.h"
#include "Sensors.h"
#include "Error.h"
#include "Utils.h"
#include "Pinout.h"

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

void getValveTempIfNecessary(bool ignoreErrors)
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

float getValveTemp(bool ignoreErrors)
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

double getValvePressure(bool ignoreErrors)
{
    int adcData = analogRead(PRESSURE_SENSOR);

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

float getDesiredTemp(const float temp, Status status)
{
    if( (status == OnPressureTrigger_DrivingWater || status == AlwaysActive_GettingHotWater) && !hotStart)
    {
        return temp*HOT_WATER_TEMPERATURE_MULTIPLIER;
    }

    return temp*COLD_WATER_TEMPERATURE_MULTIPLIER;
}
