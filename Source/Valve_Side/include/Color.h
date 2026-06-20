//
// Created by varzoz on 20/6/26.
//

#ifndef VALVE_SIDE_RGBLED_H
#define VALVE_SIDE_RGBLED_H

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

RGBColor getRGBColor(const Color color);
void writeColor(uint8_t r, uint8_t g, uint8_t b);
void writeColor(const RGBColor& color);
void writeColor(const Color color);
RGBColor getShadeFromColor(const RGBColor& color, float shadeProportion);
void fadeAnimationStep(const RGBColor& baseColor, float fadeAmountPerFrame);

#endif //VALVE_SIDE_RGBLED_H