#include "WheelEncoders.h"
#include "PinConfig_RX.h"

static volatile int32_t leftPulseCounter = 0;
static volatile int32_t rightPulseCounter = 0;

static volatile int8_t leftDirection = 0;
static volatile int8_t rightDirection = 0;

// ==================================================
// INTERRUPTS
// ==================================================

void IRAM_ATTR onLeftHall()
{
    if (leftDirection > 0)
    {
        leftPulseCounter++;
    }
    else if (leftDirection < 0)
    {
        leftPulseCounter--;
    }
}

void IRAM_ATTR onRightHall()
{
    if (rightDirection > 0)
    {
        rightPulseCounter++;
    }
    else if (rightDirection < 0)
    {
        rightPulseCounter--;
    }
}

// ==================================================
// BEGIN
// ==================================================

void WheelEncoders::begin()
{
    pinMode(LEFT_HALL, INPUT_PULLUP);
    pinMode(RIGHT_HALL, INPUT_PULLUP);

    attachInterrupt(
        digitalPinToInterrupt(LEFT_HALL),
        onLeftHall,
        RISING);

    attachInterrupt(
        digitalPinToInterrupt(RIGHT_HALL),
        onRightHall,
        RISING);

    reset();

    Serial.println("Wheel encoders ready");

    Serial.print("MM per pulse: ");
    Serial.println(MM_PER_PULSE, 4);
}

// ==================================================
// DIRECTIONS
// ==================================================

void WheelEncoders::setDirections(
    int8_t leftDir,
    int8_t rightDir)
{
    if (leftDir > 0)
    {
        leftDirection = 1;
    }
    else if (leftDir < 0)
    {
        leftDirection = -1;
    }

    if (rightDir > 0)
    {
        rightDirection = 1;
    }
    else if (rightDir < 0)
    {
        rightDirection = -1;
    }
}

// ==================================================
// UPDATE
// ==================================================

void WheelEncoders::update()
{
    const uint32_t now = millis();

    const uint32_t elapsed =
        now - lastUpdateMs;

    if (elapsed < UPDATE_INTERVAL_MS)
    {
        return;
    }

    lastUpdateMs = now;

    int32_t left;
    int32_t right;

    noInterrupts();

    left = leftPulseCounter;
    right = rightPulseCounter;

    interrupts();

    data.leftTotal = left;
    data.rightTotal = right;

    data.leftDelta =
        left - lastLeftTotal;

    data.rightDelta =
        right - lastRightTotal;

    lastLeftTotal = left;
    lastRightTotal = right;

    // Puls/sekund
    data.leftPps =
        data.leftDelta *
        (1000.0f / elapsed);

    data.rightPps =
        data.rightDelta *
        (1000.0f / elapsed);

    // Total sträcka
    data.leftDistanceMm =
        data.leftTotal * MM_PER_PULSE;

    data.rightDistanceMm =
        data.rightTotal * MM_PER_PULSE;

    // Hastighet mm/s
    data.leftSpeedMmS =
        data.leftPps * MM_PER_PULSE;

    data.rightSpeedMmS =
        data.rightPps * MM_PER_PULSE;
}

// ==================================================
// GET DATA
// ==================================================

const EncoderData &WheelEncoders::getData() const
{
    return data;
}

// ==================================================
// RESET
// ==================================================

void WheelEncoders::reset()
{
    noInterrupts();

    leftPulseCounter = 0;
    rightPulseCounter = 0;

    interrupts();

    data = EncoderData{};

    lastLeftTotal = 0;
    lastRightTotal = 0;

    lastUpdateMs = millis();
}