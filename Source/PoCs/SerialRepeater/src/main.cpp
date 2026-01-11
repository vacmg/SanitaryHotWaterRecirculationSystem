#include <Arduino.h>

#define SC_USE_HAMMING_7_4_CORRECTION_CODE 0

#include "SimpleComms.h"

SimpleComms *comms;

void setup()
{
    Serial.begin(115200);

#if SC_USE_HAMMING_7_4_CORRECTION_CODE
    Serial1.begin(9600, SERIAL_7N1);
#else
    Serial1.begin(9600, SERIAL_8N1);
#endif
    comms = new SimpleComms(&Serial1, "SHWRS");
    delay(1000);
    Serial.println("Starting Serial1 repeater");
}

void loop()
{
    while (Serial1.available())
    {
        const auto byte = Serial1.read();
        Serial.write(byte);
        Serial1.write(byte);
    }
    while (Serial.available())
    {
        Serial1.write(Serial.read());
    }
}
