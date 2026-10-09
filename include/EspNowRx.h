#pragma once

#include <Arduino.h>
#include "Protocol.h"

class EspNowRx
{
public:
    bool begin();

    bool hasNewPacket();

    ControlPacket getPacket();

    bool isConnected(
        uint32_t timeoutMs = 500) const;

    uint32_t getLastPacketMs() const;

    String getMacAddress() const;

private:
};