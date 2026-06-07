#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>
#include "Types.h"

ButtonStatus readButton(bool avoidWatchdogReset = false);

#endif // BUTTON_H
