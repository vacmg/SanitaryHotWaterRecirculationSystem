#include "Arduino.h"
#include <algorithm>

#define SC_USE_HAMMING_7_4_CORRECTION_CODE 0

#include "EspCANDriver.h"

EspCANDriver*  driver = nullptr;
EspOSInterface osInterface;

static void handleCANNoEvent(Stream* comms, const std::monostate& /*event*/)
{
    // Nothing to forward, keep debug log for visibility
    OSInterfaceLogDebug("onCANEvent", "No CAN event (monostate) received");
}

static void handleCANRXDoneEvent(Stream* comms, const CANRXDoneEvent& rxEvent)
{
    // Forward received bytes to the serial stream (cap to data array size)
    auto bytesToWrite = std::min<uint16_t>(rxEvent.frame.dlc, static_cast<uint16_t>(sizeof(rxEvent.frame.data)));
    comms->write(rxEvent.frame.data, bytesToWrite);
    OSInterfaceLogInfo("onCANEvent", "Forwarding received data over Serial: %s", toString(rxEvent));
}

static void handleCANTXDoneEvent(Stream* /*comms*/, const CANTXDoneEvent& txEvent)
{
    // For TX done we only log the result for now
    OSInterfaceLogInfo("onCANEvent", "TX completed: %s", toString(txEvent));
}

static void handleCANStateChangedEvent(Stream* /*comms*/, const CANStateChangedEvent& stateEvent)
{
    OSInterfaceLogInfo("onCANEvent", "CAN state changed: %s", toString(stateEvent));
}

// Helper for std::visit to combine overloads
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

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

        // Dispatch to the correct handler using std::visit and a set of overloads that forward
        std::visit(overloaded{
                       [&](const std::monostate& e) { handleCANNoEvent(comms, e); },
                       [&](const CANRXDoneEvent& e) { handleCANRXDoneEvent(comms, e); },
                       [&](const CANTXDoneEvent& e) { handleCANTXDoneEvent(comms, e); },
                       [&](const CANStateChangedEvent& e) { handleCANStateChangedEvent(comms, e); }
                   },
                   event);
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
    xTaskCreate(onSerialEvent, "onSerialEvent", 4096, stream, 5, nullptr);

    OSInterfaceLogInfo("main", "Ready.");
    vTaskSuspend(nullptr);

    // WARNING: if program reaches end of function app_main() the MCU will restart.
}
