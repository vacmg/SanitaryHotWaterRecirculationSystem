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
    Serial.println("Starting Serial1 to Serial bridge");
}

void loop()
{
    while (Serial1.available())
    {
        char buff[16];
        comms->getNextCommand(buff, sizeof(buff));
        Serial.println(buff);
    }
    while (Serial.available())
    {
        char commandStr[2] = " ";
        commandStr[0] = (char) Serial.read();
        comms->sendCommand(commandStr, nullptr, 0);
    }
}
