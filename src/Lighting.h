// Park patterns, parked-vehicle sync, and the light stage, scene lighting, advisor and fixed
// patterns requested for AI vehicles.
#pragma once

namespace lighting
{
    bool Install();
    int TrackedCount();

    // Forgets vehicles that no longer exist. Script thread only.
    void Prune();
}
