#include "EspNowTx.h"

#include <WiFi.h>
#include <esp_now.h>

// ==================================================
// ROBOTENS MAC-ADRESS
// ==================================================

static const uint8_t ROBOT_MAC[6] =
    {
        0xF0, 0xF5, 0xBD,
        0x73, 0x87, 0x28};

// ==================================================
// SEND CALLBACK
// ==================================================

static void onDataSent(
    const uint8_t *macAddr,
    esp_now_send_status_t status)
{
    // Vi skriver inte ut varje paket.
    // Sändningen sker 50 gånger/s.
}

// ==================================================
// BEGIN
// ==================================================

bool EspNowTx::begin()
{
    WiFi.mode(WIFI_STA);

    delay(100);

    Serial.print("TX MAC: ");
    Serial.println(WiFi.macAddress());

    if (esp_now_init() != ESP_OK)
    {
        Serial.println("ERROR: esp_now_init()");
        return false;
    }

    esp_now_register_send_cb(onDataSent);

    esp_now_peer_info_t peerInfo = {};

    memcpy(
        peerInfo.peer_addr,
        ROBOT_MAC,
        6);

    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    const esp_err_t result =
        esp_now_add_peer(&peerInfo);

    if (result != ESP_OK)
    {
        Serial.print("ERROR: esp_now_add_peer(): ");
        Serial.println(result);

        return false;
    }

    initialized = true;

    Serial.println("ESP-NOW TX ready");
    Serial.print("Robot MAC: ");

    for (uint8_t i = 0; i < 6; i++)
    {
        if (ROBOT_MAC[i] < 0x10)
            Serial.print('0');

        Serial.print(ROBOT_MAC[i], HEX);

        if (i < 5)
            Serial.print(':');
    }

    Serial.println();

    return true;
}

// ==================================================
// SEND
// ==================================================

bool EspNowTx::send(
    const ControlPacket &packet)
{
    if (!initialized)
    {
        return false;
    }

    const esp_err_t result =
        esp_now_send(
            ROBOT_MAC,
            reinterpret_cast<const uint8_t *>(&packet),
            sizeof(packet));

    return result == ESP_OK;
}

// ==================================================
// GET LOCAL MAC
// ==================================================

String EspNowTx::getMacAddress() const
{
    return WiFi.macAddress();
}