#pragma once

#include <Arduino.h>

#include "RobotControl.h"
#include "WheelEncoders.h"

class SpeedTrim
{
public:
    void begin();

    MotorCommand apply(
        const MotorCommand &command,
        int8_t steering,
        uint8_t buttonMask,
        const EncoderData &encoder);

    float getCorrectionPwm() const;
    float getIntegralForward() const;
    float getIntegralReverse() const;

private:
    int32_t lastLeftTotal = 0;
    int32_t lastRightTotal = 0;

    uint32_t lastControlMs = 0;

    float correctionPwm = 0.0f;

    // Separat inlärd trim framåt/bakåt
    float integralForward = 0.0f;
    float integralReverse = 0.0f;

    static constexpr uint32_t CONTROL_INTERVAL_MS = 500;

    // PI-regulator
    static constexpr float KP = 0.35f;
    static constexpr float KI = 0.10f;

    static constexpr int32_t ERROR_DEADBAND = 1;

    // Max total korrigering uttryckt i gamla PWM 0...255
    static constexpr float MAX_CORRECTION_PWM = 15.0f;

    // Begränsa I-delen separat
    static constexpr float MAX_INTEGRAL_PWM = 12.0f;

    static constexpr int8_t STEERING_DEADBAND = 5;
};