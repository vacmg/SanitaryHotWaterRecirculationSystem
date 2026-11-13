//
// Created by varzoz on 20/6/26.
//

#ifndef VALVE_SIDE_FLASHHELPER_H
#define VALVE_SIDE_FLASHHELPER_H

#include "Arduino.h"

#define MAIN_HELP_STRING "\nType 'reboot' to restart the system;\n'enable' or 'disable' to disable or enable the fallback mode;\n'clear' to invalidate the Error Register;\n'sensors' to print all the sensors current value;\n'changemode' to change the operation mode to the next one (similar to pressing the button);\n'startpump' or 'stoppump' to manually start or stop the pump;\n'openvalve' or 'closevalve' to manually open or close the valve;\n'errorlist' to print the error list;\n'reset' or 'reseton' to clear the error list and enable or disable the fallback mode"
#define MOCK_SENSORS_HELP_STRING "\nPress 'e' or 'd' to enable or disable trigger;\nPress 'n', 's' or 'l' to set the button to NO_PULSE, SHORT_PULSE or LONG_PULSE;\nSend a number to incorporate it as the valve temp\n"

char* strncpy_F(char* dest, const __FlashStringHelper* src, size_t size);
double fmap(double x, double in_min, double in_max, double out_min, double out_max);
const char* formattedTime(long milliseconds, char* buff);
void printSystemInfo();
void printSensorsInfo();

#endif //VALVE_SIDE_FLASHHELPER_H