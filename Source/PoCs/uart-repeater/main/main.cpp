#include "Arduino.h"

#define SC_USE_HAMMING_7_4_CORRECTION_CODE 0

#include "EspCANDriver.h"

EspOSInterface osInterface;

[[noreturn]] void onSerialEvent(void* pvParameters)
{
    OSInterfaceLogInfo("onSerialEvent", "Starting task...");
    auto* comms = static_cast<Stream*>(pvParameters);

    while (true) {
        while (comms->available())
        {
            auto byte = comms->read();
            OSInterfaceLogDebug("onSerialEvent", "Read byte: %d (0x%02X) (%c)", byte, byte, byte);
            comms->write(byte); // Echo back
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

extern "C" void app_main()
{
    OSInterfaceLogInfo("main", "Starting Arduino Core...");
    initArduino();

    OSInterfaceSetLogLevel("main", OSInterface_LOG_INFO);
    OSInterfaceSetLogLevel(EspCANDriver::TAG, OSInterface_LOG_INFO);
    OSInterfaceSetLogLevel("onCANEvent", OSInterface_LOG_INFO);
    OSInterfaceSetLogLevel("onSerialEvent", OSInterface_LOG_DEBUG);

    OSInterfaceLogInfo("main", "Starting UART driver...");
    Stream* stream;
#if SC_USE_HAMMING_7_4_CORRECTION_CODE
    Serial1.begin(9600, SERIAL_7N1, 25, 26);
    auto* SerialHamming = new HammingStream<7, 4>(Serial1);
    stream = SerialHamming;
#else
    Serial1.begin(9600, SERIAL_8N1, 25, 26);
    stream = &Serial1;
#endif

    OSInterfaceLogInfo("main", "Starting bridge tasks...");

    xTaskCreate(onSerialEvent, "onSerialEvent", 4096, stream, 5, nullptr);

    OSInterfaceLogInfo("main", "Ready.");
    vTaskSuspend(nullptr);

    // WARNING: if program reaches end of function app_main() the MCU will restart.
}
