#include "Utils.h"
#include "Leds.h"

Status getBeginStatus(Mode mode)
{
    switch(mode)
    {
        case OnPressureTrigger:
            return OnPressureTrigger_Begin;
        case AlwaysActive:
            return AlwaysActive_Begin;
        default:
            return ErrorFallBack_Begin;
    }
}

double fmap(double x, double in_min, double in_max, double out_min, double out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

char* strncpy_F(char* dest, const __FlashStringHelper* src, size_t size)
{
    PGM_P p = reinterpret_cast<PGM_P>(src);
    for (size_t i = 0; i<size; i++)
    {
        char c = pgm_read_byte(p++);

        dest[i] = c;
        if (c == '\0')
        {
            break;
        }
    }

    dest[size-1] = '\0';
    return dest;
}

Color statusToColor(Status status)
{
    switch(status)
    {
        case ErrorFallBack_Begin:
        case ErrorFallBack:
            return ERROR_FALLBACK_COLOR;
        case OnPressureTrigger_Begin:
            return BOOT_COLOR;
        case OnPressureTrigger_WaitingCold:
            return WAITING_COLD_COLOR;
        case OnPressureTrigger_TransitionToDrivingWater:
        case OnPressureTrigger_DrivingWater:
            return DRIVING_WATER_COLOR;
        case OnPressureTrigger_ServingWater:
            return SERVING_WATER_COLOR;
        case AlwaysActive_Begin:
            return BOOT_COLOR;
        case AlwaysActive_Idle:
        case AlwaysActive_TransitionToGettingHotWater:
        case AlwaysActive_GettingHotWater:
            return ALWAYS_ACTIVE_WATER_COLOR;
        default:
            return Black; // Return Black color for unknown status
    }
}

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

const char* statusToString(Status status)
{
    switch(status)
    {
        case ErrorFallBack_Begin:
            return "ErrorFallBack_Begin";
        case ErrorFallBack:
            return "ErrorFallBack";
        case OnPressureTrigger_Begin:
            return "OnPressureTrigger_Begin";
        case OnPressureTrigger_WaitingCold:
            return "OnPressureTrigger_WaitingCold";
        case OnPressureTrigger_TransitionToDrivingWater:
            return "OnPressureTrigger_TransitionToDrivingWater";
        case OnPressureTrigger_DrivingWater:
            return "OnPressureTrigger_DrivingWater";
        case OnPressureTrigger_ServingWater:
            return "OnPressureTrigger_ServingWater";
        case AlwaysActive_Begin:
            return "AlwaysActive_Begin";
        case AlwaysActive_GettingHotWater:
            return "AlwaysActive_GettingHotWater";
        case AlwaysActive_TransitionToGettingHotWater:
            return "AlwaysActive_TransitionToGettingHotWater";
        case AlwaysActive_Idle:
            return "AlwaysActive_Idle";
        default:
            return "Unknown Status";
    }
}

const char* formattedTime(long milliseconds, char* buff)
{
    if(milliseconds<0)
    {
        strcpy(buff, "None");
        return buff;
    }

    long ms = milliseconds%1000;
    long s = milliseconds/1000;
    long m = s/60;
    long h = m/60;
    m = m%60;
    s = s%60;

    sprintf(buff,"%lih %lim %lis %lims (%lims)",h,m,s,ms,milliseconds);
    return buff;
}

float getDesiredTemp(const float temp, Status status)
{
    if( (status == OnPressureTrigger_DrivingWater || status == AlwaysActive_GettingHotWater) && !hotStart)
    {
        return temp*HOT_WATER_TEMPERATURE_MULTIPLIER;
    }

    return temp*COLD_WATER_TEMPERATURE_MULTIPLIER;
}

void changeMode(Mode newMode)
{
    debug(F("Changing Mode from "));
    debug(modeToString(currentMode));
    currentMode = newMode;
    debug(F(" to "));
    debugln(modeToString(currentMode));
    changeStatus(getBeginStatus(currentMode));
}

void changeStatus(Status newStatus)
{
    debug(F("Changing Status from "));
    debug(statusToString(currentStatus));
    currentStatus = newStatus;
    writeColor(statusToColor(currentStatus));
    debug(F(" to "));
    debugln(statusToString(currentStatus));
}
