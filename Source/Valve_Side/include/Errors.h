#ifndef ERRORS_H
#define ERRORS_H

#include <Arduino.h>
#include "Types.h"
#include "Config.h"

void invalidateErrorData();
void printErrorData();
uint16_t getRaiseErrorCounter();
void resetRaiseErrorCounter();
void resetRaiseErrorCounterIfTimeoutElapsed();
void toggleFallbackMode(bool enableFallBackMode);
[[noreturn]] void raiseErrorImpl(ErrorCode error, ErrorMessageTemplate messageTemplate, float value, const char* file, uint16_t line);
void handleHeaterError(int retryCount, char* buff = nullptr);

#define raiseError(error) raiseErrorImpl((error), ERROR_MSG_TEMPLATE_NONE, EEPROM_ERROR_VALUE_NOT_SET, __FILE__, __LINE__)
#define raiseErrorWithTemplate(error, messageTemplate) raiseErrorImpl((error), (messageTemplate), EEPROM_ERROR_VALUE_NOT_SET, __FILE__, __LINE__)
#define raiseErrorWithValue(error, messageTemplate, value) raiseErrorImpl((error), (messageTemplate), (value), __FILE__, __LINE__)

#endif // ERRORS_H
