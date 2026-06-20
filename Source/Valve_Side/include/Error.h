//
// Created by varzoz on 20/6/26.
//

#ifndef VALVE_SIDE_ERROR_H
#define VALVE_SIDE_ERROR_H

#include "Config.h"

[[noreturn]] void raiseError(ErrorCode error, const char* message = nullptr);
[[noreturn]] void raiseError(ErrorCode error, const __FlashStringHelper* message);
void invalidateErrorData();
void printErrorData();
void handleHeaterError(int retryCount, char* buff = nullptr);

#endif //VALVE_SIDE_ERROR_H