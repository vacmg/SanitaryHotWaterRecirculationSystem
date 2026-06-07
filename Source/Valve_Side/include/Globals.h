#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include "Config.h"
#include "Types.h"
#include "SimpleComms.h"

extern SimpleComms comms;

extern Mode currentMode;
extern Status currentStatus;

#if !DISABLE_WATCHDOGS
extern unsigned long watchdogsPMillis;
#endif

#if MOCK_SENSORS
extern ButtonStatus btnSt;
extern bool triggerVal;
#endif

extern unsigned long heaterTempPMillis;
extern unsigned long timeBeforeGettingHeaterTempMillis;
extern unsigned long flashDrivingWaterColorMillis;

extern bool hotStart;
extern int lastHeaterTemp;

extern int progressMinTemp;
extern float desiredTemp;
extern float maxTemp;

extern unsigned long valveTempRequestTempMillis;
extern bool valveTempRequested;
extern unsigned long VALVE_TEMP_WAIT_FROM_REQUEST_TO_READ;
extern float valveTemp;
extern bool tempRequestReady;

#endif // GLOBALS_H
