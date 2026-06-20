//
// Created by varzoz on 20/6/26.
//

#ifndef VALVE_SIDE_BUTTON_H
#define VALVE_SIDE_BUTTON_H

typedef enum {NO_PULSE = 0, SHORT_PULSE, LONG_PULSE} ButtonStatus;

ButtonStatus readButton(bool avoidWatchdogReset = false);

#endif //VALVE_SIDE_BUTTON_H