//
// Created by varzoz on 20/6/26.
//

#ifndef VALVE_SIDE_STATUS_H
#define VALVE_SIDE_STATUS_H

#include "Mode.h"
#include "Color.h"

typedef enum {ErrorFallBack_Begin, ErrorFallBack, OnPressureTrigger_Begin, OnPressureTrigger_WaitingCold, OnPressureTrigger_TransitionToDrivingWater, OnPressureTrigger_DrivingWater, OnPressureTrigger_ServingWater, AlwaysActive_Begin, AlwaysActive_TransitionToGettingHotWater, AlwaysActive_GettingHotWater, AlwaysActive_Idle} Status;
#define changeStatus(newStatus) do {debug(F("Changing Status from ")); debug(statusToString(currentStatus)); currentStatus = newStatus; writeColor(statusToColor(currentStatus)); debug(F(" to ")); debugln(statusToString(currentStatus));} while(0)

#define BOOT_COLOR Green
#define USER_ACK_COLOR Cyan
#define ERROR_FALLBACK_COLOR Yellow
#define WAITING_COLD_COLOR Gray
#define DRIVING_WATER_COLOR Blue
#define SERVING_WATER_COLOR Red
#define ALWAYS_ACTIVE_WATER_COLOR Orange
#define WDT_BOOT_DELAY_COLOR Cyan
#define EMERGENCY_SHUTDOWN_1_COLOR White
#define EMERGENCY_SHUTDOWN_2_COLOR Yellow

Status getBeginStatus(Mode mode);
Color statusToColor(Status status);
const char* statusToString(Status status);
void stepFSM();

#endif //VALVE_SIDE_STATUS_H
