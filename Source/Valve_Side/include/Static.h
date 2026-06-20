//
// Created by varzoz on 20/6/26.
//

#ifndef VALVE_SIDE_STATIC_H
#define VALVE_SIDE_STATIC_H

#include "Config.h"
#include "Mode.h"
#include "Status.h"
#include "SimpleComms.h"

#if !MOCK_SENSORS
#include <DallasTemperature.h>
#endif

#if !MOCK_SENSORS
extern DallasTemperature tempSensor; // Create temp sensor instance
#endif

extern SimpleComms comms; // Create comms instance

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

extern bool hotStart; // This is used to know if enough time has passed since the last time the pump was started.
extern int lastHeaterTemp;

extern int progressMinTemp;
extern float desiredTemp;
extern float maxTemp;

extern unsigned long valveTempRequestTempMillis;
extern bool valveTempRequested;
extern unsigned long VALVE_TEMP_WAIT_FROM_REQUEST_TO_READ; // Updated to the real value at setup
extern float valveTemp;
extern bool tempRequestReady;

#endif //VALVE_SIDE_STATIC_H