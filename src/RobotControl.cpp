#include "RobotControl.h"

void RobotControl::begin()
{
}

// --------------------------------------------------
// NORMAL KÖRNING
//
// Målet:
// - gas bestämmer fart
// - styr bestämmer svängradie
// - samma styrutslag ska ge ungefär samma svängradie
//   oberoende av hastighet
// - max rakt fram = 80 %
// --------------------------------------------------

MotorCommand RobotControl::normalDrive(
    int8_t throttle,
    int8_t steering)
{
    MotorCommand cmd;

    // -127...127 -> -800...800
    const int16_t base =
        map(throttle, -127, 127, -800, 800);

    // Steering normaliserat -1...+1
    const float steer =
        static_cast<float>(steering) / 127.0f;

    // Full styrning ger +/- 25 % av aktuell basfart.
    // Vid base = 800 blir turn = +/-200.
    const float TURN_GAIN = 0.25f;

    const int16_t turn =
        static_cast<int16_t>(
            abs(base) * steer * TURN_GAIN);

    // Högerstyrning:
    // vänster hjul snabbare
    // höger hjul långsammare
    cmd.left = base + turn;
    cmd.right = base - turn;

    cmd.left = constrain(cmd.left, -1000, 1000);
    cmd.right = constrain(cmd.right, -1000, 1000);

    return cmd;
}

// --------------------------------------------------
// PIVOT / KORKSKRUV
//
// Pivotknappen hålls inne.
// Gasspak = rotation.
// Styrspak = liten fram/bak-förflyttning.
// --------------------------------------------------

MotorCommand RobotControl::pivotDrive(
    int8_t throttle,
    int8_t steering)
{
    MotorCommand cmd;

    // Max pivot ca 50 %
    const int16_t rotation =
        map(throttle, -127, 127, -800, 800);

    // Styrspaken används som offset/korkskruv.
    // Begränsad till ca +/-200.
    const int16_t translation =
        map(steering, -127, 127, -200, 200);

    cmd.left = rotation + translation;
    cmd.right = -rotation + translation;

    cmd.left = constrain(cmd.left, -1000, 1000);
    cmd.right = constrain(cmd.right, -1000, 1000);

    return cmd;
}

// --------------------------------------------------
// CALCULATE
// --------------------------------------------------

MotorCommand RobotControl::calculate(
    int8_t throttle,
    int8_t steering,
    uint8_t buttonMask)
{
    if (buttonMask & BTN_STOP)
    {
        return MotorCommand{};
    }

    if (buttonMask & BTN_PIVOT)
    {
        return pivotDrive(
            throttle,
            steering);
    }

    return normalDrive(
        throttle,
        steering);
}