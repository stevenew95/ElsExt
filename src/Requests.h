// Per-vehicle settings made by other mods through the exported interface.
#pragma once
#include "ElsExt.h"

#include <cstdint>

namespace requests
{
    struct Request
    {
        int32_t sirenMode = ELSEXT_SIREN_AUTO;
        int32_t lightStage = 0;                 // 1 or 2; 0 = ELS default
        bool takedowns = false;
        bool sceneLights = false;
        int32_t advisor = ELSEXT_ADVISOR_OFF;
        int32_t pattern[3] = { -1, -1, -1 };    // per light group, -1 = not set

        bool operator==(const Request&) const = default;
        bool IsDefault() const { return *this == Request{}; }
    };

    Request Get(int32_t vehicle);
    int Count();

    // Discards the settings of vehicles that no longer exist. Script thread only.
    void Prune();
}
