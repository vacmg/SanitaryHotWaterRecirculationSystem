//
// Created by varzoz on 20/6/26.
//

#include "Status.h"
#include "Static.h"
#include "Sensors.h"
#include "Actuators.h"

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
        case OnPressureTrigger_ValveOpenPumpRunning:
            return SERVING_WATER_COLOR;
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
        case OnPressureTrigger_ValveOpenPumpRunning:
            return "OnPressureTrigger_ValveOpenPumpRunning";
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

void stepFSM()
{
    switch(currentStatus)
    {
        case ErrorFallBack_Begin:
            setPump(false, true);
            setValve(true);
            changeStatus(ErrorFallBack);
            break;

        case ErrorFallBack:
            {
                fadeAnimationStep(getRGBColor(Yellow),FALLBACK_MODE_ANIMATION_BRIGHTNESS_STEP);
                delay(FALLBACK_MODE_ANIMATION_FRAME_DELAY);
            }
            break;


        case OnPressureTrigger_Begin:
            setPump(false);
            setPumpTimeout(AUTO_DISABLE_PUMP_TIMEOUT);
            setValve(false);
            hotStart = false;
            changeStatus(OnPressureTrigger_WaitingCold);
            break;

        case OnPressureTrigger_WaitingCold:
            if(isTriggerActive())
            {
                setPump(true);

                changeStatus(OnPressureTrigger_TransitionToDrivingWater);
                hotStart = false;
                timeBeforeGettingHeaterTempMillis = millis();
                flashDrivingWaterColorMillis = timeBeforeGettingHeaterTempMillis;
                debug(F("Waiting ")); debug(TIME_BEFORE_GETTING_HEATER_TEMP/1000); debugln(F(" seconds to get accurate temperature readings"));
                progressMinTemp = static_cast<int>(getValveTemp()) - FADE_MIN_TEMP_OFFSET;
            }
            break;

        case OnPressureTrigger_TransitionToDrivingWater:
            {
                if(millis() - flashDrivingWaterColorMillis > FLASH_DRIVING_WATER_COLOR_PERIOD)
                {
                    static bool flash = false;
                    writeColor(flash?DRIVING_WATER_COLOR:Black);
                    flash = (!flash);
                    flashDrivingWaterColorMillis = millis();
                }
                if(millis() - timeBeforeGettingHeaterTempMillis > TIME_BEFORE_GETTING_HEATER_TEMP)
                {
                    heaterTempPMillis = 0;

                    changeStatus(OnPressureTrigger_DrivingWater);

                    getHeaterTempIfNecessary(&lastHeaterTemp);
                    if(valveTemp > getDesiredTemp(lastHeaterTemp))
                    {
                        debug(statusToString(currentStatus));debugln(F("\tHot start detected"));
                        hotStart = true;
                    }

                    heaterTempPMillis = 0;
                }
            }
            break;

        case OnPressureTrigger_DrivingWater:
            {
                getHeaterTempIfNecessary(&lastHeaterTemp);
                desiredTemp = getDesiredTemp(lastHeaterTemp);

                if(tempRequestReady)
                {
                    tempRequestReady = false;
                    if(static_cast<int>(valveTemp) < progressMinTemp)
                    {
                        progressMinTemp = static_cast<int>(valveTemp);
                    }
                    long confidenceTemp = static_cast<long>(lastHeaterTemp * VALVE_OPEN_CONFIDENCE_MULTIPLIER);
                    long progress = map(static_cast<long>(valveTemp), progressMinTemp, confidenceTemp, MIN_PROGRESS_VALUE, MAX_PROGRESS_VALUE);
                    debug(statusToString(currentStatus));debug(F("\tfadeMinTemp: ")); debug(progressMinTemp); debug(F("\tvalveTemp: ")); debug(valveTemp); debug(F("\tconfidenceTemp: ")); debug(confidenceTemp); debug(F("\tdesiredTemp: ")); debug(desiredTemp); debug(F("\tProgress: ")); debug((progress*100)/MAX_PROGRESS_VALUE); debug(F("% (")); debug(progress); debugln(F(")"));

                    writeColor(progress, 0, 255-progress);

                    if(valveTemp >= desiredTemp)
                    {
                        setPump(false);
                        setValve(true);

                        changeStatus(OnPressureTrigger_ServingWater);

                        desiredTemp = MIN_ALLOWED_TEMP;
                        maxTemp = MIN_ALLOWED_TEMP;
                        progressMinTemp = static_cast<int>(valveTemp);
                    }
                    else if(valveTemp >= lastHeaterTemp * VALVE_OPEN_CONFIDENCE_MULTIPLIER)
                    {
                        setValve(true);
                        flashDrivingWaterColorMillis = millis();

                        changeStatus(OnPressureTrigger_ValveOpenPumpRunning);
                    }
                }
            }
            break;

        case OnPressureTrigger_ValveOpenPumpRunning:
            {
                getHeaterTempIfNecessary(&lastHeaterTemp);
                desiredTemp = getDesiredTemp(lastHeaterTemp);

                if(millis() - flashDrivingWaterColorMillis > FLASH_DRIVING_WATER_COLOR_PERIOD)
                {
                    static bool flash = false;
                    writeColor(flash ? SERVING_WATER_COLOR : Black);
                    flash = (!flash);
                    flashDrivingWaterColorMillis = millis();
                }

                if(tempRequestReady)
                {
                    tempRequestReady = false;
                    if(static_cast<int>(valveTemp) < progressMinTemp)
                    {
                        progressMinTemp = static_cast<int>(valveTemp);
                    }
                    debug(statusToString(currentStatus)); debug(F("\tvalveTemp: ")); debug(valveTemp); debug(F("\tdesiredTemp: ")); debugln(desiredTemp);

                    if(valveTemp >= desiredTemp)
                    {
                        setPump(false);

                        changeStatus(OnPressureTrigger_ServingWater);

                        desiredTemp = MIN_ALLOWED_TEMP;
                        maxTemp = MIN_ALLOWED_TEMP;
                        progressMinTemp = static_cast<int>(valveTemp);
                    }
                }
            }
            break;

        case OnPressureTrigger_ServingWater:
            {
                if(tempRequestReady)
                {
                    tempRequestReady = false;

                    if(valveTemp > maxTemp)
                    {
                        debug(statusToString(currentStatus));debug(F("\tmaxTemp updated from ")); debug(maxTemp); debug(F(" to ")); debugln(valveTemp);
                        maxTemp = valveTemp;
                        desiredTemp = getDesiredTemp(valveTemp);
                    }

                    debug(statusToString(currentStatus));debug(F("\tValve temp: ")); debug(valveTemp); debug(F("\tDesired temp: ")); debugln(desiredTemp);

                    if(!isTriggerActive() && valveTemp < desiredTemp)
                    {
                        setValve(false);
                        changeStatus(OnPressureTrigger_WaitingCold);
                    }
                }
            }
            break;


        case AlwaysActive_Begin:
            setPumpTimeout(AUTO_DISABLE_PUMP_TIMEOUT);
            setPump(true);
            setValve(false);

            changeStatus(AlwaysActive_TransitionToGettingHotWater);
            timeBeforeGettingHeaterTempMillis = millis();
            debug(F("Waiting ")); debug(TIME_BEFORE_GETTING_HEATER_TEMP/1000); debugln(F(" seconds to get accurate temperature readings"));
            progressMinTemp = static_cast<int>(getValveTemp()) - FADE_MIN_TEMP_OFFSET;
        break;

        case AlwaysActive_TransitionToGettingHotWater:
            if(millis() - timeBeforeGettingHeaterTempMillis > TIME_BEFORE_GETTING_HEATER_TEMP)
            {
                changeStatus(AlwaysActive_GettingHotWater);

                heaterTempPMillis = 0;

                getHeaterTempIfNecessary(&lastHeaterTemp);
                if(valveTemp > getDesiredTemp(lastHeaterTemp))
                {
                    debug(statusToString(currentStatus));debugln(F("\tHot start detected"));
                    hotStart = true;
                }

                heaterTempPMillis = 0;
            }
            break;

        case AlwaysActive_GettingHotWater:
            {
                getHeaterTempIfNecessary(&lastHeaterTemp);
                desiredTemp = getDesiredTemp(lastHeaterTemp);

                fadeAnimationStep(getRGBColor(ALWAYS_ACTIVE_WATER_COLOR), ALWAYS_ACTIVE_MODE_ANIMATION_BRIGHTNESS_STEP);
                delay(ALWAYS_ACTIVE_MODE_ANIMATION_FRAME_DELAY);

                if(tempRequestReady)
                {
                    tempRequestReady = false;
                    long progress = map(static_cast<long>(valveTemp), progressMinTemp, static_cast<long>(desiredTemp), MIN_PROGRESS_VALUE, MAX_PROGRESS_VALUE);
                    debug(statusToString(currentStatus));debug(F("\tinitialTemp: ")); debug(progressMinTemp); debug(F("\tvalveTemp: ")); debug(valveTemp); debug(F("\tdesiredTemp: ")); debug(desiredTemp); debug(F("\tProgress: ")); debug((progress*100)/MAX_PROGRESS_VALUE); debug(F("% (")); debug(progress); debugln(F(")"));

                    if(valveTemp >= desiredTemp)
                    {
                        setPump(false);
                        setValve(true);

                        changeStatus(AlwaysActive_Idle);

                        desiredTemp = MIN_ALLOWED_TEMP;
                        maxTemp = MIN_ALLOWED_TEMP;
                        progressMinTemp = static_cast<int>(valveTemp);
                    }
                }
            }
            break;

        case AlwaysActive_Idle:
            if(tempRequestReady)
            {
                tempRequestReady = false;

                if(valveTemp > maxTemp)
                {
                    debug(statusToString(currentStatus));debug(F("\tmaxTemp updated from ")); debug(maxTemp); debug(F(" to ")); debugln(valveTemp);
                    maxTemp = valveTemp;
                    desiredTemp = getDesiredTemp(valveTemp);
                }

                debug(statusToString(currentStatus));debug(F("\tValve temp: ")); debug(valveTemp); debug(F("\tDesired temp: ")); debugln(desiredTemp);

                if(!isTriggerActive() && valveTemp < desiredTemp)
                {
                    changeStatus(AlwaysActive_TransitionToGettingHotWater);
                    hotStart = false;
                    timeBeforeGettingHeaterTempMillis = millis();
                    debug(F("Waiting ")); debug(TIME_BEFORE_GETTING_HEATER_TEMP/1000); debugln(F(" seconds to get accurate temperature readings"));
                    progressMinTemp = static_cast<int>(getValveTemp()) - FADE_MIN_TEMP_OFFSET;
                }
            }
            break;
    }
}
