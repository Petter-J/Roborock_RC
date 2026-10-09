#include <Arduino.h>
#include "WheelEncoders.h"
#include "MotorDriver.h"
#include "EspNowRx.h"
#include "Protocol.h"
#include "RobotControl.h"
#include "SpeedTrim.h"

EspNowRx radio;
RobotControl robot;
MotorDriver motors;
SpeedTrim speedTrim;
WheelEncoders encoders;

bool timeoutPrinted = false;

// ==================================================
// SETUP
// ==================================================

void setup()
{
    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println("==============================");
    Serial.println(" Roborock_RC");
    Serial.println(" RX ROBOT");
    Serial.println("==============================");

    robot.begin();
    motors.begin();
    encoders.begin();
    speedTrim.begin();

    if (!radio.begin())
    {
        Serial.println("ESP-NOW FAILED");
    }

    Serial.println("Waiting for controller...");
}

// ==================================================
// LOOP
// ==================================================

void loop()
{
    static ControlPacket lastPacket{};
    static MotorCommand lastMotorCommand{};

    // ------------------------------------------------
    // NYTT STYRKOMMANDO FRÅN TX
    // ------------------------------------------------

    if (radio.hasNewPacket())
    {
        const ControlPacket packet =
            radio.getPacket();

        lastPacket = packet;

        const MotorCommand requestedCommand =
            robot.calculate(
                packet.throttle,
                packet.steering,
                packet.buttonMask);

        const EncoderData &encoder =
            encoders.getData();

        const MotorCommand motorCommand =
            speedTrim.apply(
                requestedCommand,
                packet.steering,
                packet.buttonMask,
                encoder);

        lastMotorCommand = motorCommand;

        motors.set(
            motorCommand.left,
            motorCommand.right);
    }

    // ------------------------------------------------
    // ESP-NOW FAILSAFE
    // ------------------------------------------------

    if (!radio.isConnected(500))
    {
        motors.stop();

        if (!timeoutPrinted)
        {
            Serial.println();
            Serial.println("*** ESP-NOW TIMEOUT ***");
            Serial.println("*** MOTORS STOPPED ***");
            Serial.println();

            timeoutPrinted = true;
        }
    }
    else
    {
        timeoutPrinted = false;
    }

    // ------------------------------------------------
    // MOTORRAMP
    // ------------------------------------------------

    motors.update();

    // ------------------------------------------------
    // ENCODERRIKTNING
    // ------------------------------------------------

    encoders.setDirections(
        motors.getLeftDirection(),
        motors.getRightDirection());

    // ------------------------------------------------
    // HALL / ENCODER
    // ------------------------------------------------

    encoders.update();

    // ------------------------------------------------
    // SAMlad DEBUG VAR 500 ms
    // ------------------------------------------------

    static uint32_t lastPrintMs = 0;

    if (millis() - lastPrintMs >= 500)
    {
        lastPrintMs = millis();

        const EncoderData &encoder =
            encoders.getData();

        Serial.print("GAS:");
        Serial.print(lastPacket.throttle);

        Serial.print(" STYR:");
        Serial.print(lastPacket.steering);

        Serial.print(" CMD L:");
        Serial.print(lastMotorCommand.left);

        Serial.print(" R:");
        Serial.print(lastMotorCommand.right);

        Serial.print(" | HALL L:");
        Serial.print(encoder.leftPps, 1);

        Serial.print(" R:");
        Serial.print(encoder.rightPps, 1);

        Serial.print(" | mm/s L:");
        Serial.print(encoder.leftSpeedMmS, 1);

        Serial.print(" R:");
        Serial.print(encoder.rightSpeedMmS, 1);

        Serial.print(" | TRIM:");
        Serial.print(
            speedTrim.getCorrectionPwm(),
            1);

        Serial.print(" I:");

        if (lastPacket.throttle >= 0)
        {
            Serial.print(
                speedTrim.getIntegralForward(),
                1);
        }
        else
        {
            Serial.print(
                speedTrim.getIntegralReverse(),
                1);
        }

        Serial.print(" | DIST L:");
        Serial.print(
            encoder.leftDistanceMm,
            1);

        Serial.print(" R:");
        Serial.println(
            encoder.rightDistanceMm,
            1);
    }
}