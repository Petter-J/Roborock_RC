#pragma once

#include <Arduino.h>

class DrawingStorage
{
public:
    bool begin();
    bool save(const String &fileName, const String &dxf);
    bool exists();
    String loadName();
    String loadDXF();
};