#pragma once

#include <Arduino.h>

// ==================================================
// DFR0601 MOTOR DRIVER
// ==================================================

// Vänster motor
constexpr uint8_t LEFT_PWM = 1;
constexpr uint8_t LEFT_INA = 2;
constexpr uint8_t LEFT_INB = 4;

// Höger motor
constexpr uint8_t RIGHT_PWM = 5;
constexpr uint8_t RIGHT_INA = 6;
constexpr uint8_t RIGHT_INB = 7;

// ==================================================
// HALL H1
// ==================================================

constexpr uint8_t LEFT_HALL = 15;
constexpr uint8_t RIGHT_HALL = 16;