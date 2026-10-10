#include <Arduino.h>

#include "InputManager.h"
#include "EspNowTx.h"
#include "Protocol.h"
#include "WebServerManager.h"

InputManager inputs;
EspNowTx radio;
WebServerManager web;

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

    // inputs.begin();

    // Serial.println("Inputs initialized");

    // if (!radio.begin())
    // {
    //     Serial.println("ESP-NOW FAILED");
    // }

    if (!web.begin())
    {
        Serial.println("WEB FAILED");
    }

    Serial.println();
}

// ==================================================
// LOOP
// ==================================================

void loop()
{
    web.update();
}