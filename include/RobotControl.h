#pragma once

#include <Arduino.h>
#include "Protocol.h"

struct MotorCommand
{
    int16_t left = 0;  // -1000 ... +1000
    int16_t right = 0; // -1000 ... +1000
};

class RobotControl
{
public:
    void begin();

    MotorCommand calculate(
        int8_t throttle,
        int8_t steering,
        uint8_t buttonMask);

private:
    MotorCommand normalDrive(
        int8_t throttle,
        int8_t steering);

    MotorCommand pivotDrive(
        int8_t throttle,
        int8_t steering);
};