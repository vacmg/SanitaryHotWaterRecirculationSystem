#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>

typedef enum {Black, Red, Green, Blue, Yellow, Orange, Purple, Cyan, White, Gray} Color;

typedef struct
{
    byte r;
    byte g;
    byte b;
} RGBColor;

#define COLOR_BLACK_AS_RGB {0,0,0}
#define COLOR_RED_AS_RGB {255,0,0}
#define COLOR_GREEN_AS_RGB {0,255,0}
#define COLOR_BLUE_AS_RGB {0,0,255}
#define COLOR_YELLOW_AS_RGB {255,255,0}
#define COLOR_ORANGE_AS_RGB {255,20,0}
#define COLOR_PURPLE_AS_RGB {255,0,255}
#define COLOR_CYAN_AS_RGB {0,255,255}
#define COLOR_WHITE_AS_RGB {255,255,255}
#define COLOR_GRAY_AS_RGB {1,1,1}

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

typedef enum {NO_PULSE = 0, SHORT_PULSE, LONG_PULSE} ButtonStatus;

typedef enum {OnPressureTrigger = 0, AlwaysActive, NUM_OF_MODES, ErrorFallBackMode} Mode; // Mode of the system

typedef enum {
    ErrorFallBack_Begin, 
    ErrorFallBack, 
    OnPressureTrigger_Begin, 
    OnPressureTrigger_WaitingCold, 
    OnPressureTrigger_TransitionToDrivingWater, 
    OnPressureTrigger_DrivingWater, 
    OnPressureTrigger_ServingWater, 
    AlwaysActive_Begin, 
    AlwaysActive_TransitionToGettingHotWater, 
    AlwaysActive_GettingHotWater, 
    AlwaysActive_Idle
} Status;

#endif // TYPES_H
