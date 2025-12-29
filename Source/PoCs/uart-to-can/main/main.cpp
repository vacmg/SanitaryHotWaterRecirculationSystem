#include "Arduino.h"

#define SC_USE_HAMMING_7_4_CORRECTION_CODE 1

#include "SimpleComms.h"

[[noreturn]] void startBridge(SimpleComms *comms)
{
    while (true) {
        while (comms->available())
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
        vTaskDelay(pdMS_TO_TICKS(10)); // Yield to other tasks
    }
}

extern "C" [[noreturn]] void app_main()
{
    initArduino();

    // Arduino-like setup()
    Serial.begin(115200);
    while (!Serial) { ; // wait for serial port to connect
    }

    Stream *stream;

#if SC_USE_HAMMING_7_4_CORRECTION_CODE
    Serial1.begin(9600, SERIAL_7N1, 25, 26);
    auto *SerialHamming = new HammingStream<7, 4>(Serial1);
    stream = SerialHamming;
#else
    Serial1.begin(9600, SERIAL_8N1, 25, 26);
    stream = &Serial1;
#endif

    SimpleComms comms(stream, "SHWRS");

    delay(1000);
    Serial.println("Starting Serial1 to Serial bridge");

    startBridge(&comms);

    // WARNING: if program reaches end of function app_main() the MCU will restart.
}
