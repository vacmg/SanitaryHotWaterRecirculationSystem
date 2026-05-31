#include <Arduino.h>

#define SC_USE_HAMMING_7_4_CORRECTION_CODE 0

void setup()
{
    Serial.begin(115200);

#if SC_USE_HAMMING_7_4_CORRECTION_CODE
    Serial2.begin(9600, SERIAL_7N1);
#else
    Serial2.begin(9600, SERIAL_8N1);
#endif
    delay(1000);
    Serial.println("Starting Serial2 repeater");
}

void loop()
{
    while (Serial2.available())
    {
        const auto byte = Serial2.read();
        Serial.write(byte);
        Serial2.write(byte);
    }
    while (Serial.available())
    {
        const auto byte = Serial.read();
        Serial2.write(byte);
        Serial.print("Writing ");
        Serial.println(static_cast<char>(byte));
    }
}
