#include "Sensors.h"
#include "Config.h"
#include "Globals.h"
#include "Errors.h"
#include "Utils.h"

#if !MOCK_SENSORS
#include <OneWire.h>
#include <DallasTemperature.h>

static OneWire ourWire(TEMP_SENSOR);
static DallasTemperature tempSensor(&ourWire);
#endif

void sensorsInit()
{
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
#if !MOCK_SENSORS
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
#else
    return valveTemp;
#endif
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
