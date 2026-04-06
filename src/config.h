// Centralized configuration constants for the Brompton BLE project
#pragma once
#include <Arduino.h>

// Frame number used to generate the BLE passkey
// Replace this with your bike's frame number before building
constexpr const char* FRAME_NUMBER = "1234567";

// LED and button GPIOs
constexpr int LED_RED_PIN = 23;
constexpr int LED_GREEN_PIN = 14;
constexpr int BUTTON_PIN = 27; // set to -1 to disable

// PWM is always used for LEDs (LEDC)
