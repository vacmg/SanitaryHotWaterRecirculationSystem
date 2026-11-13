//
// Created by varzoz on 20/6/26.
//

#include "Static.h"
#include "Pinout.h"

#if !MOCK_SENSORS
#include <OneWire.h>

OneWire ourWire(TEMP_SENSOR); // Create Onewire instance for temp sensor
DallasTemperature tempSensor(&ourWire); // Create temp sensor instance
#endif

SimpleComms comms(&Serial2, HEADER); // Create comms instance

Mode currentMode = ErrorFallBackMode;
Status currentStatus = ErrorFallBack_Begin;

#if !DISABLE_WATCHDOGS
unsigned long watchdogsPMillis = 0;
#endif

#if MOCK_SENSORS
ButtonStatus btnSt = NO_PULSE;
bool triggerVal = false;
#endif

unsigned long heaterTempPMillis = 0;
unsigned long timeBeforeGettingHeaterTempMillis = 0;
unsigned long flashDrivingWaterColorMillis = 0;

bool hotStart = false; // This is used to know if enough time has passed since the last time the pump was started.
int lastHeaterTemp = 0;

float progressMinTemp = 0;
float desiredTemp = 0;
float maxTemp = 0;

unsigned long valveTempRequestTempMillis = 0;
bool valveTempRequested = false;
unsigned long VALVE_TEMP_WAIT_FROM_REQUEST_TO_READ; // Updated to the real value at setup
float valveTemp = 0;
bool tempRequestReady = false;
