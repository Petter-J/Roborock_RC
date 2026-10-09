#include "SpeedTrim.h"
#include "Protocol.h"

// ==================================================
// BEGIN
// ==================================================

void SpeedTrim::begin()
{
    lastLeftTotal = 0;
    lastRightTotal = 0;

    lastControlMs = millis();

    correctionPwm = 0.0f;

    integralForward = 0.0f;
    integralReverse = 0.0f;
}

// ==================================================
// APPLY PI SPEED TRIM
// ==================================================

MotorCommand SpeedTrim::apply(
    const MotorCommand &command,
    int8_t steering,
    uint8_t buttonMask,
    const EncoderData &encoder)
{
    MotorCommand result = command;

    const uint32_t now = millis();

    // ==================================================
    // STOPP
    // ==================================================
    //
    // Nollställ inte inlärd I-trim.
    // Vi behåller motorernas kalibrerade skillnad.
    // ==================================================

    if (command.left == 0 &&
        command.right == 0)
    {
        correctionPwm = 0.0f;

        lastLeftTotal = encoder.leftTotal;
        lastRightTotal = encoder.rightTotal;

        lastControlMs = now;

        return result;
    }

    // ==================================================
    // PIVOT
    // ==================================================

    if (buttonMask & BTN_PIVOT)
    {
        correctionPwm = 0.0f;

        lastLeftTotal = encoder.leftTotal;
        lastRightTotal = encoder.rightTotal;

        lastControlMs = now;

        return result;
    }

    // ==================================================
    // STYRNING
    // ==================================================
    //
    // Vid kurva ska PI-regulatorn INTE försöka
    // göra hjulen lika snabba.
    //
    // Vi behåller däremot den inlärda I-trimmen.
    // ==================================================

    if (abs(steering) > STEERING_DEADBAND)
    {
        correctionPwm = 0.0f;

        lastLeftTotal = encoder.leftTotal;
        lastRightTotal = encoder.rightTotal;

        lastControlMs = now;

        return result;
    }

    // ==================================================
    // MÅSTE KÖRA SAMMA RIKTNING
    // ==================================================

    const bool forward =
        command.left > 0 &&
        command.right > 0;

    const bool reverse =
        command.left < 0 &&
        command.right < 0;

    if (!forward && !reverse)
    {
        correctionPwm = 0.0f;

        lastLeftTotal = encoder.leftTotal;
        lastRightTotal = encoder.rightTotal;

        lastControlMs = now;

        return result;
    }

    // ==================================================
    // PI-REGLERING VAR 500 ms
    // ==================================================

    if (now - lastControlMs >= CONTROL_INTERVAL_MS)
    {
        lastControlMs = now;

        // ------------------------------------------------
        // PULSER SEDAN FÖRRA REGLERINGEN
        // ------------------------------------------------

        const int32_t leftDelta =
            encoder.leftTotal -
            lastLeftTotal;

        const int32_t rightDelta =
            encoder.rightTotal -
            lastRightTotal;

        lastLeftTotal =
            encoder.leftTotal;

        lastRightTotal =
            encoder.rightTotal;

        // ------------------------------------------------
        // JÄMFÖR HASTIGHETENS STORLEK
        //
        // Fungerar både framåt och bakåt.
        // ------------------------------------------------

        const int32_t leftSpeed =
            abs(leftDelta);

        const int32_t rightSpeed =
            abs(rightDelta);

        // Positivt error:
        // vänster hjul har gått fler pulser
        // än höger.
        const int32_t error =
            leftSpeed - rightSpeed;

        // ------------------------------------------------
        // P-DEL
        // ------------------------------------------------

        float proportional = 0.0f;

        if (abs(error) > ERROR_DEADBAND)
        {
            proportional =
                KP * static_cast<float>(error);
        }

        // ------------------------------------------------
        // I-DEL
        // ------------------------------------------------
        //
        // I-delen minns den permanenta skillnaden
        // mellan motorerna.
        // ------------------------------------------------

        if (abs(error) > ERROR_DEADBAND)
        {
            if (forward)
            {
                integralForward +=
                    KI * static_cast<float>(error);

                integralForward =
                    constrain(
                        integralForward,
                        -MAX_INTEGRAL_PWM,
                        MAX_INTEGRAL_PWM);
            }

            if (reverse)
            {
                integralReverse +=
                    KI * static_cast<float>(error);

                integralReverse =
                    constrain(
                        integralReverse,
                        -MAX_INTEGRAL_PWM,
                        MAX_INTEGRAL_PWM);
            }
        }

        // ------------------------------------------------
        // P + I
        // ------------------------------------------------

        if (forward)
        {
            correctionPwm =
                proportional +
                integralForward;
        }
        else
        {
            correctionPwm =
                proportional +
                integralReverse;
        }

        correctionPwm =
            constrain(
                correctionPwm,
                -MAX_CORRECTION_PWM,
                MAX_CORRECTION_PWM);
    }

    // ==================================================
    // PWM 0...255 -> MOTOR COMMAND 0...1000
    // ==================================================

    const int16_t correctionCommand =
        static_cast<int16_t>(
            correctionPwm *
            (1000.0f / 255.0f));

    // ==================================================
    // FRAMÅT
    // ==================================================

    if (forward)
    {
        result.left =
            command.left -
            correctionCommand;

        result.right =
            command.right +
            correctionCommand;
    }

    // ==================================================
    // BAKÅT
    // ==================================================
    //
    // Större negativt värde betyder högre fart
    // bakåt, därför omvänd addition.
    // ==================================================

    else if (reverse)
    {
        result.left =
            command.left +
            correctionCommand;

        result.right =
            command.right -
            correctionCommand;
    }

    result.left =
        constrain(
            result.left,
            -1000,
            1000);

    result.right =
        constrain(
            result.right,
            -1000,
            1000);

    return result;
}

// ==================================================
// DEBUG
// ==================================================

float SpeedTrim::getCorrectionPwm() const
{
    return correctionPwm;
}

float SpeedTrim::getIntegralForward() const
{
    return integralForward;
}

float SpeedTrim::getIntegralReverse() const
{
    return integralReverse;
}