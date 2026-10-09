#pragma once

#include <Arduino.h>

class MotorDriver
{
public:
    void begin();

    void set(
        int16_t left,
        int16_t right);

    void update();

    void stop();

    int16_t getCurrentLeft() const;
    int16_t getCurrentRight() const;

    int8_t getLeftDirection() const;
    int8_t getRightDirection() const;

private:
    void setMotorHardware(
        uint8_t channel,
        uint8_t ina,
        uint8_t inb,
        int16_t command);

    int16_t rampMotor(
        int16_t current,
        int16_t target);

    int16_t targetLeft = 0;
    int16_t targetRight = 0;

    int16_t currentLeft = 0;
    int16_t currentRight = 0;

    int8_t leftDirection = 0;
    int8_t rightDirection = 0;

    uint32_t lastRampMs = 0;

    static constexpr uint32_t PWM_FREQ = 20000;
    static constexpr uint8_t PWM_BITS = 8;

    static constexpr uint8_t LEFT_CHANNEL = 0;
    static constexpr uint8_t RIGHT_CHANNEL = 1;

    static constexpr uint32_t RAMP_INTERVAL_MS = 10;
    static constexpr int16_t RAMP_STEP = 50;
};