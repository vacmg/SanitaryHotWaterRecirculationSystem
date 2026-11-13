//
// Created by varzoz on 20/6/26.
//

#ifndef VALVE_SIDE_SENSORS_H
#define VALVE_SIDE_SENSORS_H

#include "Static.h"

void requestValveTempIfNecessary();
void getValveTempIfNecessary(bool ignoreErrors = false);
float getValveTemp(bool ignoreErrors = false);
double getValvePressure(bool ignoreErrors = false);
bool isTriggerActive();
int getHeaterTemp(bool ignoreErrors = false);
bool getHeaterTempIfNecessary(int* temp);
float getDesiredTemp(const float temp, Status status = currentStatus);

#endif //VALVE_SIDE_SENSORS_H