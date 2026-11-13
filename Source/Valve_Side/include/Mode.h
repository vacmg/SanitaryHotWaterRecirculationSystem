//
// Created by varzoz on 20/6/26.
//

#ifndef VALVE_SIDE_MODE_H
#define VALVE_SIDE_MODE_H

typedef enum {OnPressureTrigger = 0, AlwaysActive, NUM_OF_MODES, ErrorFallBackMode} Mode; // Mode of the system
#define changeMode(newMode) do {debug(F("Changing Mode from ")); debug(modeToString(currentMode)); currentMode = newMode; debug(F(" to ")); debugln(modeToString(currentMode)); changeStatus(getBeginStatus(currentMode));} while(0)

const char* modeToString(Mode mode);
void toggleFallbackMode(bool enableFallBackMode);
void setNextMode();

#endif //VALVE_SIDE_MODE_H