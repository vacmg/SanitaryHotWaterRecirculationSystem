/*
#include "Arduino.h"

#define SC_USE_HAMMING_7_4_CORRECTION_CODE 0

#include "EspCANDriver.h"

EspOSInterface osInterface;

[[noreturn]] void onSerialEvent(void* pvParameters)
{
    OSInterfaceLogInfo("onSerialEvent", "Starting task...");

    while (true)
    {
        //OSInterfaceLogInfo("onSerialEvent", "Serial2.available() -> %d", Serial2.available());
        while (Serial2.available())
        {
            auto byte = Serial2.read();
            OSInterfaceLogDebug("onSerialEvent", "Read byte: %d (0x%02X) (%c)", byte, byte, byte);
            Serial2.write(byte); // Echo back
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void setup()
{
    OSInterfaceLogInfo("main", "Starting Arduino Core...");
    initArduino();
    Serial.begin(115200);
    delay(200);

    OSInterfaceSetLogLevel("main", OSInterface_LOG_INFO);
    OSInterfaceSetLogLevel(EspCANDriver::TAG, OSInterface_LOG_INFO);
    OSInterfaceSetLogLevel("onCANEvent", OSInterface_LOG_INFO);
    OSInterfaceSetLogLevel("onSerialEvent", OSInterface_LOG_DEBUG);

    constexpr int8_t rx = 25;
    constexpr int8_t tx = 26;
    OSInterfaceLogInfo("main", "Starting UART driver on rx=%d; tx=%d...", rx, tx);
    Serial2.begin(9600, SERIAL_8N1, rx, tx); // rx, tx
    delay(1000);

    // OSInterfaceLogInfo("main", "Starting bridge tasks...");
    // xTaskCreatePinnedToCore(onSerialEvent, "onSerialEvent", 4096, nullptr, 2, nullptr, 1);
    // delay(1000);

    OSInterfaceLogInfo("main", "Ready.");

    Serial2.println("1234");
    Serial2.flush();
    OSInterfaceLogInfo("main", "Hello World!");

    // WARNING: if program reaches end of function app_main() the MCU will restart.
}

void loop()
{
    //OSInterfaceLogInfo("onSerialEvent", "Serial2.available() -> %d", Serial2.available());
    while (Serial2.available())
    {
        auto byte = Serial2.read();
        OSInterfaceLogDebug("onSerialEvent", "Read byte: %d (0x%02X) (%c)", byte, byte, byte);
        Serial2.write(byte); // Echo back
    }
    vTaskDelay(pdMS_TO_TICKS(10));
}
//*/
































#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"

// Definimos los pines y el tamaño del buffer
#define RX2_PIN 25
#define TX2_PIN 26
#define BUF_SIZE 1024

extern "C" void app_main(void) {
    /* =========================================
       EQUIVALENTE A setup()
       ========================================= */

    // 1. Configuración de Serial2 (UART_NUM_2) a 9600 baudios, 8N1
    uart_config_t uart2_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // Instalamos el driver de UART2, aplicamos la configuración y asignamos los pines
    uart_driver_install(UART_NUM_2, BUF_SIZE, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_2, &uart2_config);
    uart_set_pin(UART_NUM_2, TX2_PIN, RX2_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // 2. Configuración de Serial (UART_NUM_0)
    // ESP-IDF ya configura la salida de consola (printf) a 115200 baudios por defecto.
    // Solo necesitamos instalar el driver para poder leer (rx) los datos entrantes de forma no bloqueante.
    uart_driver_install(UART_NUM_0, BUF_SIZE, 0, 0, NULL, 0);

    // delay(1000);
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Serial.print(...) y Serial.flush()
    printf("Ready on rx=%d; tx=%d\n", RX2_PIN, TX2_PIN);
    fflush(stdout);

    uint8_t data;
    size_t length;

    /* =========================================
       EQUIVALENTE A loop()
       ========================================= */
    while (1) {

        // if(Serial.available()) -> Revisamos si hay bytes en el buffer de UART0
        uart_get_buffered_data_len(UART_NUM_0, &length);
        if (length > 0) {
            // Serial.read() -> Leemos 1 byte
            if (uart_read_bytes(UART_NUM_0, &data, 1, 0) == 1) {
                printf("Forwarding character %c\n", (char)data);
                fflush(stdout);

                // Serial2.write(r);
                uart_write_bytes(UART_NUM_2, (const char*)&data, 1);
            }
        }

        // if(Serial2.available()) -> Revisamos si hay bytes en el buffer de UART2
        uart_get_buffered_data_len(UART_NUM_2, &length);
        if (length > 0) {
            // Serial2.read() -> Leemos 1 byte
            if (uart_read_bytes(UART_NUM_2, &data, 1, 0) == 1) {
                // Imprimimos ambas líneas tal como lo hacía tu código
                printf("Received from Serial2 the character \n%c\n", (char)data);
                fflush(stdout);

                // Serial2.write(r);
                uart_write_bytes(UART_NUM_2, (const char*)&data, 1);
            }
        }

        // Añadimos un pequeño delay de 10ms (equivalente a yield)
        // para que FreeRTOS pueda atender otras tareas y no salte el Watchdog timer.
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
