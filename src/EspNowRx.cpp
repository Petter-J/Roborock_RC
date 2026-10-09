#include "EspNowRx.h"

#include <WiFi.h>
#include <esp_now.h>

// ==================================================
// RECEIVE STATE
// ==================================================

static volatile bool newPacketAvailable = false;

static ControlPacket receivedPacket;

static volatile uint32_t lastPacketMs = 0;

// ==================================================
// CALLBACK
// ==================================================

static void onDataRecv(
    const uint8_t *mac,
    const uint8_t *incomingData,
    int len)
{
    if (len != sizeof(ControlPacket))
    {
        return;
    }

    memcpy(
        &receivedPacket,
        incomingData,
        sizeof(ControlPacket));

    lastPacketMs = millis();

    newPacketAvailable = true;
}

// ==================================================
// BEGIN
// ==================================================

bool EspNowRx::begin()
{
    WiFi.mode(WIFI_STA);

    delay(100);

    Serial.print("RX MAC: ");
    Serial.println(WiFi.macAddress());

    if (esp_now_init() != ESP_OK)
    {
        Serial.println("ERROR: esp_now_init()");
        return false;
    }

    esp_now_register_recv_cb(onDataRecv);

    Serial.println("ESP-NOW RX ready");

    return true;
}

// ==================================================
// NEW PACKET?
// ==================================================

bool EspNowRx::hasNewPacket()
{
    return newPacketAvailable;
}

// ==================================================
// GET PACKET
// ==================================================

ControlPacket EspNowRx::getPacket()
{
    ControlPacket packet;

    noInterrupts();

    packet = receivedPacket;

    newPacketAvailable = false;

    interrupts();

    return packet;
}

// ==================================================
// CONNECTION STATUS
// ==================================================

bool EspNowRx::isConnected(
    uint32_t timeoutMs) const
{
    if (lastPacketMs == 0)
    {
        return false;
    }

    return millis() - lastPacketMs <= timeoutMs;
}

// ==================================================
// LAST PACKET
// ==================================================

uint32_t EspNowRx::getLastPacketMs() const
{
    return lastPacketMs;
}

// ==================================================
// LOCAL MAC
// ==================================================

String EspNowRx::getMacAddress() const
{
    return WiFi.macAddress();
}