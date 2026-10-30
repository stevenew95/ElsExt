#pragma once

namespace settings
{
    struct Settings
    {
        bool parkEnabled = true;
        bool parkPlayerCar = true;
        int parkDelayMs = 2000;
        bool syncEnabled = true;
        bool syncPlayerCar = true;
        float syncRadius = 60.0f;
    };

    // Reads ElsExt.ini from the game folder. Missing or invalid values keep their defaults.
    void Load(const char* gameFolder);
    const Settings& Get();
}
