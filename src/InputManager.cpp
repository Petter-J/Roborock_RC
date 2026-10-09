#include "InputManager.h"
#include "PinConfig_TX.h"

static const uint8_t buttonPins[8] =
    {
        PIN_BUTTON_1,
        PIN_BUTTON_2,
        PIN_BUTTON_3,
        PIN_BUTTON_4,
        PIN_BUTTON_5,
        PIN_BUTTON_6,
        PIN_BUTTON_7,
        PIN_BUTTON_8};

// ==================================================
// KALIBRERING
// ==================================================

static constexpr uint16_t THROTTLE_CENTER = 1877;
static constexpr uint16_t STEERING_CENTER = 1931;

// Rå-ADC deadband runt mittläget
static constexpr uint16_t ADC_DEADBAND = 25;

// Deadband efter omräkning till -127 ... +127
static constexpr int8_t THROTTLE_DEADBAND = 3;
static constexpr int8_t STEERING_DEADBAND = 5;

// ==================================================
// HJÄLPFUNKTION
// ==================================================

static int8_t applyOutputDeadband(
    int8_t value,
    int8_t deadband)
{
    if (value >= -deadband &&
        value <= deadband)
    {
        return 0;
    }

    return value;
}

// ==================================================
// BEGIN
// ==================================================

void InputManager::begin()
{
    analogReadResolution(12);

    for (uint8_t i = 0; i < BUTTON_COUNT; i++)
    {
        pinMode(buttonPins[i], INPUT_PULLUP);
    }
}

// ==================================================
// AXELKONVERTERING
// ==================================================

int8_t InputManager::convertAxis(
    uint16_t raw,
    uint16_t center)
{
    if (raw >= center - ADC_DEADBAND &&
        raw <= center + ADC_DEADBAND)
    {
        return 0;
    }

    int value = 0;

    if (raw < center)
    {
        value = map(
            raw,
            0,
            center - ADC_DEADBAND,
            -127,
            0);
    }
    else
    {
        value = map(
            raw,
            center + ADC_DEADBAND,
            4095,
            0,
            127);
    }

    value = constrain(value, -127, 127);

    return static_cast<int8_t>(value);
}

// ==================================================
// UPDATE
// ==================================================

void InputManager::update()
{
    state.throttleRaw = analogRead(PIN_THROTTLE);
    state.steeringRaw = analogRead(PIN_STEERING);

    // Standard:
    // framåt = +
    // bakåt  = -
    // höger  = +
    // vänster = -

    state.throttle =
        -convertAxis(
            state.throttleRaw,
            THROTTLE_CENTER);

    state.steering =
        -convertAxis(
            state.steeringRaw,
            STEERING_CENTER);

    // Extra deadband på det färdiga 8-bit-värdet
    state.throttle =
        applyOutputDeadband(
            state.throttle,
            THROTTLE_DEADBAND);

    state.steering =
        applyOutputDeadband(
            state.steering,
            STEERING_DEADBAND);

    // ------------------------------------------------
    // KNAPPAR
    // ------------------------------------------------

    uint8_t mask = 0;

    for (uint8_t i = 0; i < BUTTON_COUNT; i++)
    {
        if (digitalRead(buttonPins[i]) == LOW)
        {
            mask |= (1 << i);
        }
    }

    state.buttonMask = mask;
}

// ==================================================
// GET STATE
// ==================================================

const InputState &InputManager::getState() const
{
    return state;
}