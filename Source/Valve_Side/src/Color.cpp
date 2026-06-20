//
// Created by varzoz on 20/6/26.
//

#include "Config.h"
#include "Color.h"
#include "Pinout.h"


RGBColor getRGBColor(const Color color)
{
    switch (color)
    {
        case Black:
            return COLOR_BLACK_AS_RGB;
        case Red:
            return COLOR_RED_AS_RGB;
        case Green:
            return COLOR_GREEN_AS_RGB;
        case Blue:
            return COLOR_BLUE_AS_RGB;
        case Yellow:
            return COLOR_YELLOW_AS_RGB;
        case Orange:
            return COLOR_ORANGE_AS_RGB;
        case Purple:
            return COLOR_PURPLE_AS_RGB;
        case Cyan:
            return COLOR_CYAN_AS_RGB;
        case White:
            return COLOR_WHITE_AS_RGB;
        case Gray:
            return COLOR_GRAY_AS_RGB;
    }

    return COLOR_BLACK_AS_RGB;
}

void writeColor(uint8_t r, uint8_t g, uint8_t b)
{
#if LED_ENABLED
    analogWrite(RED_LED_PIN, r);
    analogWrite(GREEN_LED_PIN, g);
    analogWrite(BLUE_LED_PIN, b);
#else
    analogWrite(RED_LED_PIN, 255-r);
    analogWrite(GREEN_LED_PIN, 255-g);
    analogWrite(BLUE_LED_PIN, 255-b);
#endif
}

void writeColor(const RGBColor& color)
{
    writeColor(color.r, color.g, color.b);
}

void writeColor(const Color color)
{
    writeColor(getRGBColor(color));
}

RGBColor getShadeFromColor(const RGBColor& color, float shadeProportion)
{
    shadeProportion = constrain(shadeProportion, 0.0, 1.0);
    return {static_cast<byte>(color.r*shadeProportion), static_cast<byte>(color.g*shadeProportion), static_cast<byte>(color.b*shadeProportion)};
}

void fadeAnimationStep(const RGBColor& baseColor, float fadeAmountPerFrame)
{
    static bool increasing = true;
    static float fade = 0.0;

    if(increasing)
    {
        fade += fadeAmountPerFrame;
        if(fade >= 1.0)
        {
            fade = 1.0;
            increasing = false;
        }
    }
    else
    {
        fade -= fadeAmountPerFrame;
        if(fade <= 0.0)
        {
            fade = 0.0;
            increasing = true;
        }
    }
    writeColor(getShadeFromColor(baseColor, fade));
}
