//
// Created by varzoz on 20/6/26.
//

#include "Config.h"
#include "Profiler.h"
#include "EEPROM.h"
#include "Arduino.h"

#if PROFILER_ENABLED
unsigned long profilerMillis = 0;
ProfilerData profilerData = defaultProfilerData;

void profilerStartMeasure()
{
    profilerMillis = millis();
}

int profilerEndMeasure()
{
    profilerMillis = millis() - profilerMillis;
    int updated = 0;
    if(profilerMillis < profilerData.minTime)
    {
        profilerData.minTime = profilerMillis;
        updated = -1;
    }
    if(profilerMillis > profilerData.maxTime)
    {
        profilerData.maxTime = profilerMillis;
        updated = 1;
    }
    return updated;
}

void saveProfilerData()
{
    EEPROM.put(PROFILER_DATA_START_ADDRESS, profilerData);
}

void clearProfilerData()
{
    profilerData = defaultProfilerData;
    saveProfilerData();
}

void loadProfilerData()
{
    EEPROM.get(PROFILER_DATA_START_ADDRESS, profilerData);
    if(profilerData.valid != defaultProfilerData.valid)
    {
        clearProfilerData();
    }
}

void printProfilerData()
{
    Serial.println("Profiler Data:");
    Serial.print("Min Time: ");
    Serial.println(profilerData.minTime);
    Serial.print("Max Time: ");
    Serial.println(profilerData.maxTime);
    Serial.print("Min Time Data: ");
    Serial.println(profilerData.minTimeData);
    Serial.print("Max Time Data: ");
    Serial.println(profilerData.maxTimeData);
    Serial.println();
}

#else

void loadProfilerData()
{
    unsigned long invalid = 0;
    EEPROM.put(PROFILER_DATA_START_ADDRESS, invalid);
}

void printProfilerData()
{
    Serial.println("Profiler is disabled.");
}

#endif
