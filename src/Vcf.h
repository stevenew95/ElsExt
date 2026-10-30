// ELS vehicle configuration files (ELS\<VcfContainerFolder>\<model>.xml).
#pragma once
#include <cstdint>

namespace vcf
{
    // Settings from a model's optional <EXT> block.
    struct Model
    {
        uint32_t hash = 0;
        char name[40] = {};
        int32_t park[3] = { -1, -1, -1 };       // pattern per light group, -1 = not set
        int32_t parkDelayMs = -1;               // -1 = settings default
        bool sync = false;
        char syncGroup[64] = {};                // lower case; empty = same model only
        float syncRadius = 0.0f;                // metres; 0 = settings default
        int32_t advisor[3] = { -1, -1, -1 };    // SECL pattern for left, right, both

        bool HasPark() const { return park[0] >= 0 || park[1] >= 0 || park[2] >= 0; }
        bool HasAdvisor() const { return advisor[0] >= 0 || advisor[1] >= 0 || advisor[2] >= 0; }
    };

    // Reads every vehicle file of the folder named in ELS.ini and stores their <EXT> blocks.
    // Settings that ELS reads without checking for them are reported to the log when missing.
    void Load(const char* gameFolder);

    const Model* Find(uint32_t modelHash);

    // Same result as the GET_HASH_KEY native.
    uint32_t HashKey(const char* text);
}
