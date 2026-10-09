#include <Arduino.h>

#include "InputManager.h"
#include "EspNowTx.h"
#include "Protocol.h"

InputManager inputs;
EspNowTx radio;

// ==================================================
// TIMING
// ==================================================

constexpr uint32_t SEND_INTERVAL_MS = 20; // 50 Hz
constexpr uint32_t PRINT_INTERVAL_MS = 250;

uint32_t lastSendMs = 0;
uint32_t lastPrintMs = 0;

uint32_t sequenceNumber = 0;

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
    Serial.println(" TX CONTROL");
    Serial.println("==============================");

    inputs.begin();

    Serial.println("Inputs initialized");

    if (!radio.begin())
    {
        Serial.println("ESP-NOW FAILED");
    }

    Serial.println();
}

// ==================================================
// LOOP
// ==================================================

void loop()
{
    inputs.update();

    const InputState &state =
        inputs.getState();

    const uint32_t now = millis();

    // ------------------------------------------------
    // SEND CONTROL PACKET
    // ------------------------------------------------

    if (now - lastSendMs >= SEND_INTERVAL_MS)
    {
        lastSendMs = now;

        ControlPacket packet;

        packet.sequence =
            sequenceNumber++;

        packet.throttle =
            state.throttle;

        packet.steering =
            state.steering;

        packet.buttonMask =
            state.buttonMask;

        if (!radio.send(packet))
        {
            Serial.println(
                "ESP-NOW send error");
        }
    }

    // ------------------------------------------------
    // SERIAL DEBUG
    // ------------------------------------------------

    if (now - lastPrintMs >= PRINT_INTERVAL_MS)
    {
        lastPrintMs = now;

        Serial.print("TX GAS:");
        Serial.print(state.throttle);

        Serial.print(" STYR:");
        Serial.print(state.steering);

        Serial.print(" BTN:0x");

        if (state.buttonMask < 0x10)
        {
            Serial.print('0');
        }

        Serial.print(
            state.buttonMask,
            HEX);

        Serial.print(" SEQ:");
        Serial.println(sequenceNumber);
    }
}