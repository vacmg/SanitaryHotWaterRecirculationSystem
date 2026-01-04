#include "Arduino.h"

#define SC_USE_HAMMING_7_4_CORRECTION_CODE 0

#include "SimpleComms.h"
#include "EspCANDriver.h"

EspCANDriver*  driver = nullptr;
EspOSInterface osInterface;

[[noreturn]] void onSerialEvent(void* pvParameters)
{
    OSInterfaceLogInfo("onSerialEvent", "Starting task...");
    SimpleComms* comms = static_cast<SimpleComms*>(pvParameters);
    while (true) {
        while (comms->available())
        {
            char buff[16];
            auto err = comms->getNextCommand(buff, sizeof(buff));
            if (err <= 0)
            {
                OSInterfaceLogWarning("onSerialEvent", "No command available or error (%d)", err);
                continue; // No command available or error
            }
            CANFrame frame = {
                    .id = {.N_AI = 0x1},
                    .dlc = static_cast<uint16_t>(strlen(buff)),
                    .ide = true,
            };
            memcpy(frame.data, buff, std::min(static_cast<size_t>(frame.dlc), sizeof(frame.data)));
            driver->sendFrame(frame, portMAX_DELAY);
            OSInterfaceLogInfo("onSerialEvent", "Sent data \"%s\" (%dB)", buff, err);
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // Yield to other tasks
    }
}

[[noreturn]] void onCANEvent(void* pvParameters)
{
    OSInterfaceLogInfo("onCANEvent", "Starting task...");
    SimpleComms* comms = static_cast<SimpleComms*>(pvParameters);
    while (true)
    {
        OSInterfaceLogDebug("onCANEvent", "Waiting for event from ISR...");
        CANEvent event = driver->getEvent(portMAX_DELAY);
        OSInterfaceLogDebug("onCANEvent", "Received event: %s", toString(event));
        if (std::holds_alternative<CANRXDoneEvent>(event))
        {
            CANRXDoneEvent& rxEvent       = std::get<CANRXDoneEvent>(event);
            char* commandStr = reinterpret_cast<char*>(rxEvent.frame.data);
            commandStr[rxEvent.frame.dlc] = '\0'; // Null-terminate the string
            OSInterfaceLogInfo("onCANEvent", "Forwarding received command over Serial: %s", commandStr);
            comms->sendCommand(commandStr, nullptr, 0);

        }
    }
}

extern "C" void app_main()
{
    OSInterfaceSetLogLevel("main", OSInterface_LOG_INFO);
    OSInterfaceSetLogLevel(EspCANDriver::TAG, OSInterface_LOG_INFO);
    OSInterfaceSetLogLevel("onCANEvent", OSInterface_LOG_INFO);
    OSInterfaceSetLogLevel("onSerialEvent", OSInterface_LOG_INFO);

    OSInterfaceLogInfo("main", "Starting Arduino Core...");
    initArduino();

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
    SimpleComms comms(stream, "SHWRS");

    OSInterfaceLogInfo("main", "Starting CAN driver...");
    constexpr twai_onchip_node_config_t node_config = {
            .io_cfg         = {.tx = GPIO_NUM_4, .rx = GPIO_NUM_5}, // TWAI TX GPIO pin// TWAI RX GPIO pin
            .clk_src        = TWAI_CLK_SRC_DEFAULT,
            .bit_timing     = {.bitrate = 200000}, // 200 kbps bitrate
            .tx_queue_depth = 5,                   // Transmit queue depth set to 5
            .flags          = {.enable_self_test   = true,
                    .enable_loopback    = true,
                    .enable_listen_only = false,
                    .no_receive_rtr     = false},
    };

    esp_err_t err;
    driver = new EspCANDriver(node_config, 4, osInterface, err);
    ESP_ERROR_CHECK(err);

    if (!driver || !driver->enable())
    {
        esp_system_abort("Failed to enable CAN driver");
    }

    OSInterfaceLogInfo("main", "Starting bridge tasks...");

    xTaskCreate(onCANEvent, "onCANEvent", 4096, &comms, 5, nullptr);
    xTaskCreate(onSerialEvent, "writerTask", 4096, &comms, 5, nullptr);

    vTaskSuspend(nullptr);

    // WARNING: if program reaches end of function app_main() the MCU will restart.
}
