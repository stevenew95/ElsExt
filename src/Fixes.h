// Corrections to defects in ELS V 1.05.
#pragma once

namespace fixes
{
    bool Install();

    // Applied after the hooks are enabled; reverted by Remove.
    void PatchLightingLoop();
    void Remove();
}
