#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>
#include "Types.h"

// Forward declaration or include Globals.h for default argument
#include "Globals.h"

double fmap(double x, double in_min, double in_max, double out_min, double out_max);
char* strncpy_F(char* dest, const __FlashStringHelper* src, size_t size);
const char* formattedTime(long milliseconds, char* buff);
const char* modeToString(Mode mode);
const char* statusToString(Status status);
Color statusToColor(Status status);
Status getBeginStatus(Mode mode);
float getDesiredTemp(const float temp, Status status = currentStatus);

void changeMode(Mode newMode);
void changeStatus(Status newStatus);

#endif // UTILS_H
