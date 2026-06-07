#include "FSM.h"
#include "Config.h"
#include "Globals.h"
#include "Leds.h"
#include "Sensors.h"
#include "HeaterControl.h"
#include "Utils.h"

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
                    long progress = map(static_cast<long>(valveTemp), progressMinTemp, static_cast<long>(desiredTemp), MIN_PROGRESS_VALUE, MAX_PROGRESS_VALUE);
                    debug(statusToString(currentStatus));debug(F("\tfadeMinTemp: ")); debug(progressMinTemp); debug(F("\tvalveTemp: ")); debug(valveTemp); debug(F("\tdesiredTemp: ")); debug(desiredTemp); debug(F("\tProgress: ")); debug((progress*100)/MAX_PROGRESS_VALUE); debug(F("% (")); debug(progress); debugln(F(")"));

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
