//
// Created by varzoz on 20/6/26.
//

#include "Config.h"
#include "Mode.h"
#include "Status.h"
#include "Static.h"

const char* modeToString(Mode mode)
{
    switch(mode)
    {
        case OnPressureTrigger:
            return "OnPressureTrigger";
        case AlwaysActive:
            return "AlwaysActive";
        case ErrorFallBackMode:
            return "ErrorFallBackMode";
        default:
            return "Unknown Mode";
    }
}

void toggleFallbackMode(bool enableFallBackMode)
{
    if(enableFallBackMode)
    {
        if(currentMode == ErrorFallBackMode)
        {
            debugln(F("Warning: FallBack Mode already enabled"));
            return;
        }
        debug(F("Enabling Error FallBack Mode from mode "));debugln(modeToString(currentMode));
        EEPROM.put(EEPROM_FALLBACK_MODE_ENABLED_ADDRESS, true);
        EEPROM.put(EEPROM_MODE_ADDRESS, currentMode);
        changeMode(ErrorFallBackMode);
        stepFSM();
    }
    else
    {
        debugln(F("Disabling Error FallBack Mode"));
        Mode mode;
        EEPROM.get(EEPROM_MODE_ADDRESS, mode);
        if(mode >= NUM_OF_MODES)
        {
            Serial.println(F("ERROR: Invalid Mode stored in EEPROM, setting to OnPressureTrigger"));
            mode = OnPressureTrigger;
        }
        changeMode(mode);
        EEPROM.put(EEPROM_FALLBACK_MODE_ENABLED_ADDRESS, false);
    }
}

void setNextMode()
{
    changeMode(static_cast<Mode>((currentMode + 1) % NUM_OF_MODES));
    EEPROM.put(EEPROM_MODE_ADDRESS, currentMode);
}
