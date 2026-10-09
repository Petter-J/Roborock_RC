#pragma once

#include <Arduino.h>

// ==================================================
// KNAPPBITAR
// ==================================================

constexpr uint8_t BTN_START = 0x01;
constexpr uint8_t BTN_RESERVED1 = 0x02;
constexpr uint8_t BTN_PREV_CUE = 0x04;
constexpr uint8_t BTN_NEXT_CUE = 0x08;
constexpr uint8_t BTN_STOP = 0x10;
constexpr uint8_t BTN_RESERVED2 = 0x20;
constexpr uint8_t BTN_PIVOT = 0x40;
constexpr uint8_t BTN_MODE = 0x80;

// ==================================================
// CONTROL PACKET
// TX -> RX
// ==================================================

struct __attribute__((packed)) ControlPacket
{
    uint32_t sequence = 0;

    // -127 ... 0 ... +127
    // framåt = +
    // bakåt  = -
    int8_t throttle = 0;

    // -127 ... 0 ... +127
    // vänster = -
    // höger   = +
    int8_t steering = 0;

    uint8_t buttonMask = 0;
};