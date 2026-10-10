#include "RouteStorage.h"
#include <Preferences.h>

static Preferences preferences;

// ==================================================
// BEGIN
// ==================================================

bool RouteStorage::begin()
{
    return preferences.begin(
        "routes",
        false);
}

// ==================================================
// SAVE
// ==================================================

bool RouteStorage::saveRoute(
    const String &data)
{
    const size_t written =
        preferences.putString(
            "route",
            data);

    return written > 0;
}

// ==================================================
// LOAD
// ==================================================

String RouteStorage::loadRoute()
{
    return preferences.getString(
        "route",
        "[]");
}