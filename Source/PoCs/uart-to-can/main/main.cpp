#include "Arduino.h"

#define SC_USE_HAMMING_7_4_CORRECTION_CODE 0

#include "EspCANDriver.h"

EspCANDriver*  driver = nullptr;
EspOSInterface osInterface;

[[noreturn]] void onSerialEvent(void* pvParameters)
{
    OSInterfaceLogInfo("onSerialEvent", "Starting task...");
    auto* comms = static_cast<Stream*>(pvParameters);
    CANFrame frame = {
            .id = {.N_AI = 0x1},
            .ide = true,
    };
    uint32_t buffLen = 0;

    while (true) {
        buffLen = 0;
        if (!comms->available()) {
            vTaskDelay(pdMS_TO_TICKS(10)); // Yield to other tasks
            continue;
        }
        while (comms->available() && buffLen < sizeof(frame.data)) // We are not using null-terminated strings, be careful.
        {
            int dataByte = comms->read();
            frame.data[buffLen++] = static_cast<uint8_t>(dataByte);
            OSInterfaceLogDebug("onSerialEvent", "Read byte: %d (0x%02X) (%c) at position %u", dataByte, dataByte, dataByte, buffLen - 1);
        }
        frame.dlc = static_cast<uint8_t>(buffLen);
        driver->sendFrame(frame, portMAX_DELAY);
        OSInterfaceLogInfo("onSerialEvent", "Sent data \"%s\"", toString(frame));
    }
}

[[noreturn]] void onCANEvent(void* pvParameters)
{
    OSInterfaceLogInfo("onCANEvent", "Starting task...");
    auto* comms = static_cast<Stream*>(pvParameters);
    while (true)
    {
        OSInterfaceLogDebug("onCANEvent", "Waiting for event from ISR...");
        CANEvent event = driver->getEvent(portMAX_DELAY);
        OSInterfaceLogDebug("onCANEvent", "Received event: %s", toString(event));
        if (std::holds_alternative<CANRXDoneEvent>(event))
        {
            CANRXDoneEvent& rxEvent       = std::get<CANRXDoneEvent>(event);
            comms->write(rxEvent.frame.data, std::min(rxEvent.frame.dlc, static_cast<uint16_t>(sizeof(rxEvent.frame))));
            OSInterfaceLogInfo("onCANEvent", "Forwarding received data over Serial: %s", toString(rxEvent));
        }
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

    xTaskCreate(onCANEvent, "onCANEvent", 4096, stream, 5, nullptr);
    xTaskCreate(onSerialEvent, "writerTask", 4096, stream, 5, nullptr);

    OSInterfaceLogInfo("main", "Ready.");
    vTaskSuspend(nullptr);

    // WARNING: if program reaches end of function app_main() the MCU will restart.
}
