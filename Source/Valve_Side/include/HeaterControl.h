#ifndef HEATER_CONTROL_H
#define HEATER_CONTROL_H

#include <Arduino.h>
#include "Types.h"

void setValve(bool enable);
void setPump(bool enable, bool ignoreErrors = false);
void setPumpTimeout(long timeout);
int getHeaterTemp(bool ignoreErrors = false);
bool getHeaterTempIfNecessary(int* temp);
void connectToHeater(bool ignoreErrors = false);
void handleCommsEvent();

#if !DISABLE_WATCHDOGS
void resetWatchdogs();
void resetWatchdogsIfNecessary();
#endif

#endif // HEATER_CONTROL_H
