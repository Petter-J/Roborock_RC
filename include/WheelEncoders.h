#pragma once

#include <Arduino.h>

struct EncoderData
{
    int32_t leftTotal = 0;
    int32_t rightTotal = 0;

    int32_t leftDelta = 0;
    int32_t rightDelta = 0;

    float leftPps = 0.0f;
    float rightPps = 0.0f;

    float leftDistanceMm = 0.0f;
    float rightDistanceMm = 0.0f;

    float leftSpeedMmS = 0.0f;
    float rightSpeedMmS = 0.0f;
};

class WheelEncoders
{
public:
    void begin();

    void update();

    void setDirections(
        int8_t leftDirection,
        int8_t rightDirection);

    const EncoderData &getData() const;

    void reset();

private:
    EncoderData data;

    int32_t lastLeftTotal = 0;
    int32_t lastRightTotal = 0;

    uint32_t lastUpdateMs = 0;

    static constexpr uint32_t UPDATE_INTERVAL_MS = 250;

    static constexpr float PULSES_PER_REV = 232.7f;
    static constexpr float WHEEL_CIRCUMFERENCE_MM = 220.0f;

    static constexpr float MM_PER_PULSE =
        WHEEL_CIRCUMFERENCE_MM / PULSES_PER_REV;
};