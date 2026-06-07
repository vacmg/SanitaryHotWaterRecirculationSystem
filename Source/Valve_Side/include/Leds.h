#ifndef LEDS_H
#define LEDS_H

#include <Arduino.h>
#include "Types.h"

RGBColor getRGBColor(const Color color);
void writeColor(uint8_t r, uint8_t g, uint8_t b);
void writeColor(const RGBColor& color);
void writeColor(const Color color);
RGBColor getShadeFromColor(const RGBColor& color, float shadeProportion);
void fadeAnimationStep(const RGBColor& baseColor, float fadeAmountPerFrame);

#endif // LEDS_H
