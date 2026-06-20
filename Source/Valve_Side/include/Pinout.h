//
// Created by varzoz on 20/6/26.
//

#ifndef VALVE_SIDE_PINOUT_H
#define VALVE_SIDE_PINOUT_H

#include "Config.h"

constexpr uint8_t RECEIVER_ENABLE_PIN = 5;  // HIGH = Driver / LOW = Receptor
constexpr uint8_t DRIVE_ENABLE_PIN = 4;  // HIGH = Driver / LOW = Receptor

constexpr uint8_t VALVE_RELAY_PIN = 2;
// const uint8_t VALVE_FEEDBACK_PIN = 7;

constexpr uint8_t RED_LED_PIN = 11;
constexpr uint8_t GREEN_LED_PIN = 9;
constexpr uint8_t BLUE_LED_PIN = 10;

constexpr uint8_t BUTTON_PIN = 8;

#if ENABLE_HEARTBEAT
constexpr uint8_t HEARTBEAT_PIN = 13;
#endif

#if !MOCK_SENSORS
constexpr uint8_t PRESSURE_SENSOR = A0;
constexpr uint8_t TEMP_SENSOR = 12;
#endif

#endif //VALVE_SIDE_PINOUT_H