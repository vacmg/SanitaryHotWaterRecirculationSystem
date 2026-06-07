#include "Button.h"
#include "Config.h"
#include "Globals.h"
#include "HeaterControl.h"

ButtonStatus readButton(bool avoidWatchdogReset)
{
    ButtonStatus res = NO_PULSE;

    #if MOCK_SENSORS
    res = btnSt;
    btnSt = NO_PULSE;
    #else
    if(digitalRead(BUTTON_PIN) == BTN_PRESSED)
    {
        unsigned long time = millis();
        delay(10);
        while(digitalRead(BUTTON_PIN) == BTN_PRESSED && millis() - time < BUTTON_LONG_PRESSED_TIME)
        {
            #if !DISABLE_WATCHDOGS
            if(!avoidWatchdogReset)
            {
                resetWatchdogsIfNecessary();
            }
            #endif
        }

        time = millis() - time;

        if(time >= BUTTON_LONG_PRESSED_TIME)
        {
            debug(F("Long press detected: ")); debugln(time);
            res = LONG_PULSE;
        }
        else if (time >= BUTTON_SHORT_PRESSED_MIN_TIME)
        {
            debug(F("Short press detected: ")); debugln(time);
            res = SHORT_PULSE;
        }
    }
    #endif

    return res;
}
