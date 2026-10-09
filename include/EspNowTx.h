#pragma once

#include <Arduino.h>
#include "Protocol.h"

class EspNowTx
{
public:
    bool begin();

    bool send(const ControlPacket &packet);

    String getMacAddress() const;

private:
    bool initialized = false;
};