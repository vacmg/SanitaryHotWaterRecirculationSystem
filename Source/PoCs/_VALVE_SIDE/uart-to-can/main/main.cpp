#include "driver/uart.h"
#include <algorithm>

#define SC_USE_HAMMING_7_4_CORRECTION_CODE 0

#include "EspCANDriver.h"

#define UART_PORT_NUM UART_NUM_1
#define UART_BAUD_RATE 9600
#define UART_RX_PIN GPIO_NUM_25
#define UART_TX_PIN GPIO_NUM_26
#define BUF_SIZE 1024

EspCANDriver*  driver = nullptr;
EspOSInterface osInterface;

static void handleCANNoEvent(const std::monostate& /*event*/)
{
    // Nothing to forward, keep debug log for visibility
    OSInterfaceLogDebug("onCANEvent", "No CAN event (monostate) received");
}

static void handleCANRXDoneEvent(const CANRXDoneEvent& rxEvent)
{
    // Forward received bytes to the serial stream (cap to data array size)
    auto bytesToWrite = std::min<uint16_t>(rxEvent.frame.dlc, sizeof(rxEvent.frame.data));
    uart_write_bytes(UART_PORT_NUM, rxEvent.frame.data, bytesToWrite);
    OSInterfaceLogInfo("onCANEvent", "Forwarding received data over Serial: %s", toString(rxEvent));
}

static void handleCANTXDoneEvent(const CANTXDoneEvent& txEvent)
{
    // For TX done we only log the result for now
    OSInterfaceLogInfo("onCANEvent", "TX completed: %s", toString(txEvent));
}

static void handleCANStateChangedEvent(const CANStateChangedEvent& stateEvent)
{
    OSInterfaceLogInfo("onCANEvent", "CAN state changed: %s", toString(stateEvent));
}

// Helper for std::visit to combine overloads
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

[[noreturn]] void onSerialEvent(void* pvParameters)
{
    OSInterfaceLogInfo("onSerialEvent", "Starting task...");
    CANFrame frame = {
            .id = {.N_AI = 0x1},
            .ide = true,
    };
    uint32_t buffLen = 0;

    while (true) {
        buffLen = 0;
        size_t available = 0;
        uart_get_buffered_data_len(UART_PORT_NUM, &available);
        if (available == 0) {
            vTaskDelay(pdMS_TO_TICKS(10)); // Yield to other tasks
            continue;
        }

        int length = uart_read_bytes(UART_PORT_NUM, frame.data, sizeof(frame.data), pdMS_TO_TICKS(10));
        if (length > 0) {
            buffLen = length;
            for (int i = 0; i < buffLen; i++) {
                OSInterfaceLogDebug("onSerialEvent", "Read byte: %d (0x%02X) (%c) at position %d", frame.data[i], frame.data[i], frame.data[i], i);
            }
            frame.dlc = static_cast<uint8_t>(buffLen);
            driver->sendFrame(frame, portMAX_DELAY);
            OSInterfaceLogInfo("onSerialEvent", "Sent data \"%s\"", toString(frame));
        }
    }
}

[[noreturn]] void onCANEvent(void* pvParameters)
{
    OSInterfaceLogInfo("onCANEvent", "Starting task...");
    while (true)
    {
        OSInterfaceLogDebug("onCANEvent", "Waiting for event from ISR...");
        CANEvent event = driver->getEvent(portMAX_DELAY);
        OSInterfaceLogDebug("onCANEvent", "Received event: %s", toString(event));

        // Dispatch to the correct handler using std::visit and a set of overloads that forward
        std::visit(overloaded{
                       [&](const std::monostate& e) { handleCANNoEvent(e); },
                       [&](const CANRXDoneEvent& e) { handleCANRXDoneEvent(e); },
                       [&](const CANTXDoneEvent& e) { handleCANTXDoneEvent(e); },
                       [&](const CANStateChangedEvent& e) { handleCANStateChangedEvent(e); }
                   },
                   event);
    }
}

extern "C" void app_main()
{
    OSInterfaceSetLogLevel("main", OSInterface_LOG_INFO);
    OSInterfaceSetLogLevel(EspCANDriver::TAG, OSInterface_LOG_DEBUG);
    OSInterfaceSetLogLevel("onCANEvent", OSInterface_LOG_INFO);
    OSInterfaceSetLogLevel("onSerialEvent", OSInterface_LOG_DEBUG);

    OSInterfaceLogInfo("main", "Starting UART driver...");

    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    OSInterfaceLogInfo("main", "Starting CAN driver...");
    constexpr twai_onchip_node_config_t node_config = {
            .io_cfg         = {.tx = GPIO_NUM_22, .rx = GPIO_NUM_23}, // TWAI TX GPIO pin// TWAI RX GPIO pin
            .clk_src        = TWAI_CLK_SRC_DEFAULT,
            .bit_timing     = {.bitrate = 200000}, // 200 kbps bitrate
            .tx_queue_depth = 5,                   // Transmit queue depth set to 5
            .flags          = {.enable_self_test   = false,
                    .enable_loopback    = false,
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

    xTaskCreate(onCANEvent, "onCANEvent", 4096, nullptr, 5, nullptr);
    xTaskCreate(onSerialEvent, "onSerialEvent", 4096, nullptr, 5, nullptr);

    OSInterfaceLogInfo("main", "Ready.");
    vTaskSuspend(nullptr);

    // WARNING: if program reaches end of function app_main() the MCU will restart.
}
