#include "Settings.h"
#include "Log.h"

#include <windows.h>
#include <cstdio>
#include <cstdlib>

namespace
{
    settings::Settings g_settings;

    double ReadNumber(const char* path, const char* section, const char* key, double fallback, double min, double max)
    {
        char text[32];
        GetPrivateProfileStringA(section, key, "", text, sizeof(text), path);
        char* end = nullptr;
        const double value = strtod(text, &end);
        return end != text && value >= min && value <= max ? value : fallback;
    }

    bool ReadSwitch(const char* path, const char* section, const char* key, bool fallback)
    {
        return GetPrivateProfileIntA(section, key, fallback ? 1 : 0, path) != 0;
    }
}

namespace settings
{
    void Load(const char* gameFolder)
    {
        char path[MAX_PATH];
        if (snprintf(path, sizeof(path), "%sElsExt.ini", gameFolder) >= static_cast<int>(sizeof(path))) return;
        if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES)
        {
            elslog::Line("ElsExt.ini not found; using defaults.");
            return;
        }

        Settings& s = g_settings;
        s.parkEnabled = ReadSwitch(path, "Park", "Enabled", s.parkEnabled);
        s.parkPlayerCar = ReadSwitch(path, "Park", "PlayerCar", s.parkPlayerCar);
        s.parkDelayMs = static_cast<int>(ReadNumber(path, "Park", "DelaySeconds", s.parkDelayMs / 1000.0, 0.0, 600.0) * 1000.0);
        s.syncEnabled = ReadSwitch(path, "Sync", "Enabled", s.syncEnabled);
        s.syncPlayerCar = ReadSwitch(path, "Sync", "PlayerCar", s.syncPlayerCar);
        s.syncRadius = static_cast<float>(ReadNumber(path, "Sync", "RadiusMetres", s.syncRadius, 1.0, 1000.0));

        elslog::Line("Settings: park %s (player car %s, %.1f s), sync %s (player car %s, %.0f m).",
                     s.parkEnabled ? "on" : "off", s.parkPlayerCar ? "yes" : "no", s.parkDelayMs / 1000.0,
                     s.syncEnabled ? "on" : "off", s.syncPlayerCar ? "yes" : "no", s.syncRadius);
    }

    const Settings& Get() { return g_settings; }
}
