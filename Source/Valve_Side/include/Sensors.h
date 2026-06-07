#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include "Types.h"

void sensorsInit();
void requestValveTempIfNecessary();
void getValveTempIfNecessary(bool ignoreErrors = false);
float getValveTemp(bool ignoreErrors = false);
double getValvePressure(bool ignoreErrors = false);
bool isTriggerActive();

#endif // SENSORS_H
