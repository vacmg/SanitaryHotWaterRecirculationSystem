//
// Created by varzoz on 20/6/26.
//

#ifndef VALVE_SIDE_PROFILER_H
#define VALVE_SIDE_PROFILER_H

#include "Config.h"

#if PROFILER_ENABLED
extern unsigned long profilerMillis;

typedef struct
{
    unsigned long valid;
    unsigned long minTime;
    unsigned long maxTime;
    char minTimeData[PROFILER_DATA_MSG_SIZE];
    char maxTimeData[PROFILER_DATA_MSG_SIZE];
} ProfilerData;

constexpr ProfilerData defaultProfilerData = {0XABDCEF12, INT32_MAX, 0, "", ""};
extern ProfilerData profilerData;

void profilerStartMeasure();
int profilerEndMeasure();
void saveProfilerData();
void clearProfilerData();
void loadProfilerData();
void printProfilerData();

#else

void loadProfilerData();

void printProfilerData();

#endif

#endif //VALVE_SIDE_PROFILER_H