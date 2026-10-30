#include "Fixes.h"
#include "Els.h"
#include "Log.h"
#include "Natives.h"

namespace
{
    // Offset, within the LightsLoop_GoneVehicleExit site, of the rel8 operand of its jmp.
    constexpr uint32_t kExitJumpOperand = 0x13;
    static_assert(els::kSites[static_cast<int>(els::Site::LightsLoop_GoneVehicleExit)].bytes[kExitJumpOperand - 1] == 0xEB);

    els::Routine g_resetStateAndRemove = nullptr;
    els::Routine g_stepPatternNumber = nullptr;
    bool g_loopPatched = false;
    uint8_t g_originalJump = 0;

    bool ReleaseSound(int32_t& soundId, int32_t& playing)
    {
        if (playing <= 0 || soundId < 0) return false;
        native::StopSound(soundId);
        native::ReleaseSoundId(soundId);
        soundId = -1;
        playing = 0;
        return true;
    }

    // ELS releases a vehicle (list full, out of range, deleted) by clearing its state without
    // stopping its sounds, which leaves a siren playing that nothing can stop.
    uint64_t OnResetStateAndRemove(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4)
    {
        const int32_t vehicle = els::Handle(a1);
        ElsVehState* s = vehicle > 0 ? els::State(vehicle) : nullptr;
        if (s != nullptr && s->initialisedFromVcf == 1)
        {
            const bool siren = ReleaseSound(s->sirenSoundId, s->sirenPlaying);
            const bool auxSiren = ReleaseSound(s->auxSirenSoundId, s->auxSirenPlaying);
            const bool horn = ReleaseSound(s->hornSoundId, s->hornKindPlaying);
            if (siren || auxSiren || horn) elslog::Line("Vehicle %d: sounds stopped as ELS released it.", vehicle);
        }
        return g_resetStateAndRemove(a1, a2, a3, a4);
    }

    // While a forced pattern shows, ELS's pattern keys store the new number in the current
    // stage's preset. At a stage with no preset for the light group the preset index is -1 and
    // the write lands before the vehicle's state, over its key in ELS's vehicle table. The key
    // changes the saved pattern instead.
    uint64_t OnStepPatternNumber(uint64_t vehicle, uint64_t lightGroup, uint64_t direction, uint64_t forced)
    {
        if ((forced & 0xFF) != 0 && els::PresetFieldIndex(els::Handle(vehicle), els::Handle(lightGroup)) < 0)
        {
            static bool logged = false;
            if (!logged) elslog::Line("Pattern key at a stage without a preset: changed the saved pattern instead.");
            logged = true;
            forced &= ~0xFFull;
        }
        return g_stepPatternNumber(vehicle, lightGroup, direction, forced);
    }

    uint32_t ExitJumpOperandRva() { return els::Rva(els::Site::LightsLoop_GoneVehicleExit) + kExitJumpOperand; }
}

namespace fixes
{
    bool Install()
    {
        return els::Hook(els::Function::Veh_ResetStateAndRemove, &OnResetStateAndRemove, &g_resetStateAndRemove)
            && els::Hook(els::Function::Lights_StepPatternNumber, &OnStepPatternNumber, &g_stepPatternNumber);
    }

    // In ELS's lighting loop the branch for a vehicle that no longer exists jumps to the
    // function's exit instead of the next vehicle, so every vehicle after it misses the pass.
    // The jump is redirected to LightsLoop_Continue, the block the other branches end with: the
    // loop keeps the DOES_ENTITY_EXIST hash in RAX and that block reloads it before continuing.
    void PatchLightingLoop()
    {
        const uint32_t operand = ExitJumpOperandRva();
        const auto distance = static_cast<uint8_t>(els::Rva(els::Site::LightsLoop_Continue) - (operand + 1));
        g_loopPatched = els::WriteCodeByte(operand, distance, &g_originalJump);
        if (!g_loopPatched) elslog::Line("Could not apply the lighting-loop fix.");
    }

    void Remove()
    {
        if (g_loopPatched) els::WriteCodeByte(ExitJumpOperandRva(), g_originalJump, nullptr);
        g_loopPatched = false;
    }
}
