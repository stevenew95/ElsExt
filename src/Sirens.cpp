#include "Sirens.h"
#include "Diagnostics.h"
#include "Els.h"
#include "Log.h"
#include "Requests.h"

namespace
{
    els::Routine g_updateSirenAndHorn = nullptr;

    // Stops the siren and horn through ELS's own routines so its sound bookkeeping stays valid.
    void Silence(int32_t vehicle)
    {
        ElsVehState* s = els::State(vehicle);
        if (s == nullptr) return;

        if (s->sirenPlaying == 1)
        {
            els::StopSiren(vehicle);
            elslog::Line("Vehicle %d: siren stopped (silent mode).", vehicle);
        }
        if (s->hornActive != 0 || s->hornKindPlaying > 0)
        {
            s->hornActive = 0;
            s->hornKind = 0;
            els::UpdateHornSound(vehicle);
        }
    }

    // Called by ELS for every AI-driven ELS vehicle near the player, every frame.
    uint64_t OnUpdateSirenAndHorn(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4)
    {
        const int32_t vehicle = els::Handle(a1);
        diag::SetLastVehicle(vehicle);
        if (requests::Get(vehicle).sirenMode != ELSEXT_SIREN_SILENT) return g_updateSirenAndHorn(a1, a2, a3, a4);
        Silence(vehicle);
        return 0;
    }
}

namespace sirens
{
    bool Install()
    {
        return els::Hook(els::Function::Ai_UpdateSirenAndHorn, &OnUpdateSirenAndHorn, &g_updateSirenAndHorn);
    }
}
