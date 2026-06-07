#ifndef ERRORS_H
#define ERRORS_H

#include <Arduino.h>
#include "Types.h"
#include "Config.h"

void invalidateErrorData();
void printErrorData();
void toggleFallbackMode(bool enableFallBackMode);
[[noreturn]] void raiseError(ErrorCode error, const char* message = nullptr);
[[noreturn]] void raiseError(ErrorCode error, const __FlashStringHelper* message);
void handleHeaterError(int retryCount, char* buff = nullptr);

#endif // ERRORS_H
