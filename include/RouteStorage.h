#pragma once

#include <Arduino.h>

class RouteStorage
{
public:
    bool begin();

    bool saveRoute(const String &data);

    String loadRoute();
};