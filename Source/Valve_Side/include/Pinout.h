//
// Pinout definitions for Sanitary Hot Water Recirculation System
// Created by Victor on 17/06/2024.
//

#ifndef PINOUT_H
#define PINOUT_H

#include <Arduino.h>

// Pin Definitions
constexpr uint8_t RECEIVER_ENABLE_PIN = 5;  // HIGH = Driver / LOW = Receptor
constexpr uint8_t DRIVE_ENABLE_PIN = 4;  // HIGH = Driver / LOW = Receptor
constexpr uint8_t VALVE_RELAY_PIN = 2;
constexpr uint8_t RED_LED_PIN = 11;
constexpr uint8_t GREEN_LED_PIN = 9;
constexpr uint8_t BLUE_LED_PIN = 10;
constexpr uint8_t BUTTON_PIN = 8;
constexpr uint8_t HEARTBEAT_PIN = 13;
constexpr uint8_t PRESSURE_SENSOR = A0;
constexpr uint8_t TEMP_SENSOR = 12;

#endif

