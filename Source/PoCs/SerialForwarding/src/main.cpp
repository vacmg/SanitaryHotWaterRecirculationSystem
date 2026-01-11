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
    Serial.println("Press any key to send complex payload command");
    while (!Serial.available());
    delay(500);
    while (Serial.available()) Serial.read(); // Clear input buffer

    const char* args[] = {"arg1", "arg2", "arg3"};
    auto res = comms->sendCommand("command", reinterpret_cast<const char**>(args), 3);
    Serial.print("Sent command with result: ");
    Serial.println(res);
}

void loop()
{
    while (Serial1.available())
    {
        char buff[300];
        if(comms->getNextArgument(buff, sizeof(buff)) <= 0)
        {
            Serial.println("Error reading command from Serial1");
        }
        Serial.println(buff);
    }
    while (Serial.available())
    {
        char commandStr[2] = " ";
        commandStr[0] = (char) Serial.read();
        Serial.print("Sending command: ");
        Serial.println(commandStr);
        comms->sendCommand(commandStr, nullptr, 0);
    }
}
