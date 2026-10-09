#include "MotorDriver.h"
#include "PinConfig_RX.h"

// ==================================================
// BEGIN
// ==================================================

void MotorDriver::begin()
{
    pinMode(LEFT_INA, OUTPUT);
    pinMode(LEFT_INB, OUTPUT);

    pinMode(RIGHT_INA, OUTPUT);
    pinMode(RIGHT_INB, OUTPUT);

    ledcSetup(
        LEFT_CHANNEL,
        PWM_FREQ,
        PWM_BITS);

    ledcSetup(
        RIGHT_CHANNEL,
        PWM_FREQ,
        PWM_BITS);

    ledcAttachPin(
        LEFT_PWM,
        LEFT_CHANNEL);

    ledcAttachPin(
        RIGHT_PWM,
        RIGHT_CHANNEL);

    stop();
}

// ==================================================
// SET TARGET
// ==================================================

void MotorDriver::set(
    int16_t left,
    int16_t right)
{
    targetLeft =
        constrain(left, -1000, 1000);

    targetRight =
        constrain(right, -1000, 1000);
}

// ==================================================
// RAMP ONE MOTOR
// ==================================================

int16_t MotorDriver::rampMotor(
    int16_t current,
    int16_t target)
{
    // ------------------------------------------------
    // RIKTNINGSBYTE
    // Måste alltid först ner till 0
    // ------------------------------------------------

    if (
        (current > 0 && target < 0) ||
        (current < 0 && target > 0))
    {
        target = 0;
    }

    // ------------------------------------------------
    // RAMPA MOT TARGET
    // ------------------------------------------------

    if (current < target)
    {
        current += RAMP_STEP;

        if (current > target)
        {
            current = target;
        }
    }
    else if (current > target)
    {
        current -= RAMP_STEP;

        if (current < target)
        {
            current = target;
        }
    }

    return current;
}

// ==================================================
// UPDATE
// ==================================================

void MotorDriver::update()
{
    const uint32_t now = millis();

    if (now - lastRampMs < RAMP_INTERVAL_MS)
    {
        return;
    }

    lastRampMs = now;

    // ------------------------------------------------
    // LEFT
    // ------------------------------------------------

    const int16_t requestedLeft =
        targetLeft;

    if (
        (currentLeft > 0 && requestedLeft < 0) ||
        (currentLeft < 0 && requestedLeft > 0))
    {
        currentLeft =
            rampMotor(currentLeft, 0);
    }
    else
    {
        currentLeft =
            rampMotor(
                currentLeft,
                requestedLeft);
    }

    // ------------------------------------------------
    // RIGHT
    // ------------------------------------------------

    const int16_t requestedRight =
        targetRight;

    if (
        (currentRight > 0 && requestedRight < 0) ||
        (currentRight < 0 && requestedRight > 0))
    {
        currentRight =
            rampMotor(currentRight, 0);
    }
    else
    {
        currentRight =
            rampMotor(
                currentRight,
                requestedRight);
    }

    // ------------------------------------------------
    // FAKTISK RIKTNING TILL ENCODER
    // ------------------------------------------------
    //
    // Vi använder det rampade motorvärdet,
    // alltså samma riktning som faktiskt skickas
    // till motorkortet.
    //
    // Vid 0 behåller vi senaste riktningen.
    // ------------------------------------------------

    if (currentLeft > 0)
    {
        leftDirection = 1;
    }
    else if (currentLeft < 0)
    {
        leftDirection = -1;
    }

    if (currentRight > 0)
    {
        rightDirection = 1;
    }
    else if (currentRight < 0)
    {
        rightDirection = -1;
    }

    // ------------------------------------------------
    // HARDWARE
    // ------------------------------------------------

    setMotorHardware(
        LEFT_CHANNEL,
        LEFT_INA,
        LEFT_INB,
        currentLeft);

    setMotorHardware(
        RIGHT_CHANNEL,
        RIGHT_INA,
        RIGHT_INB,
        currentRight);
}

// ==================================================
// HARDWARE OUTPUT
// ==================================================

void MotorDriver::setMotorHardware(
    uint8_t channel,
    uint8_t ina,
    uint8_t inb,
    int16_t command)
{
    command =
        constrain(command, -1000, 1000);

    if (command == 0)
    {
        ledcWrite(channel, 0);

        digitalWrite(ina, LOW);
        digitalWrite(inb, LOW);

        return;
    }

    const uint8_t pwm =
        map(
            abs(command),
            0,
            1000,
            0,
            255);

    if (command > 0)
    {
        digitalWrite(ina, LOW);
        digitalWrite(inb, HIGH);
    }
    else
    {
        digitalWrite(ina, HIGH);
        digitalWrite(inb, LOW);
    }

    ledcWrite(channel, pwm);
}

// ==================================================
// STOP
// ==================================================

void MotorDriver::stop()
{
    targetLeft = 0;
    targetRight = 0;

    currentLeft = 0;
    currentRight = 0;

    ledcWrite(LEFT_CHANNEL, 0);
    ledcWrite(RIGHT_CHANNEL, 0);

    digitalWrite(LEFT_INA, LOW);
    digitalWrite(LEFT_INB, LOW);

    digitalWrite(RIGHT_INA, LOW);
    digitalWrite(RIGHT_INB, LOW);

    // OBS:
    // leftDirection/rightDirection nollställs INTE.
    // Vi behåller senaste körriktning om hjulet
    // rullar lite efter stopp.
}

// ==================================================
// ACTUAL MOTOR COMMAND
// ==================================================

int16_t MotorDriver::getCurrentLeft() const
{
    return currentLeft;
}

int16_t MotorDriver::getCurrentRight() const
{
    return currentRight;
}

// ==================================================
// ACTUAL MOTOR DIRECTION
// ==================================================

int8_t MotorDriver::getLeftDirection() const
{
    return leftDirection;
}

int8_t MotorDriver::getRightDirection() const
{
    return rightDirection;
}