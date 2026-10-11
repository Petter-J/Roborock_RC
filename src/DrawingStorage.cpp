#include "DrawingStorage.h"
#include <LittleFS.h>

static const char *DXF_FILE = "/drawing.dxf";
static const char *NAME_FILE = "/drawing_name.txt";

bool DrawingStorage::begin()
{
    return LittleFS.begin(true);
}

bool DrawingStorage::save(const String &fileName, const String &dxf)
{
    File dxfFile = LittleFS.open(DXF_FILE, "w");
    if (!dxfFile)
        return false;

    size_t written = dxfFile.print(dxf);
    dxfFile.close();

    if (written != dxf.length())
        return false;

    File nameFile = LittleFS.open(NAME_FILE, "w");
    if (!nameFile)
        return false;

    nameFile.print(fileName);
    nameFile.close();

    return true;
}

bool DrawingStorage::exists()
{
    return LittleFS.exists(DXF_FILE);
}

String DrawingStorage::loadName()
{
    if (!LittleFS.exists(NAME_FILE))
        return "drawing.dxf";

    File file = LittleFS.open(NAME_FILE, "r");
    if (!file)
        return "drawing.dxf";

    String name = file.readString();
    file.close();
    return name;
}

String DrawingStorage::loadDXF()
{
    if (!LittleFS.exists(DXF_FILE))
        return "";

    File file = LittleFS.open(DXF_FILE, "r");
    if (!file)
        return "";

    String data = file.readString();
    file.close();
    return data;
}