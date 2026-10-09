#pragma once

#include <Arduino.h>

struct InputState
{
    uint16_t throttleRaw = 0;
    uint16_t steeringRaw = 0;

    int8_t throttle = 0;
    int8_t steering = 0;

    uint8_t buttonMask = 0;
};

class InputManager
{
public:
    void begin();
    void update();

    const InputState &getState() const;

private:
    static constexpr uint8_t BUTTON_COUNT = 8;

    InputState state;

    int8_t convertAxis(uint16_t raw, uint16_t center);
};