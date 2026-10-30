// Access to ELS.asi V 1.05: verification, hooks and the internal functions the extension calls.
#pragma once
#include "ElsLayout.h"
#include "ElsVehState.h"

#include <cstdint>

namespace els
{
    // Hooked ELS functions are replaced with this signature. All four argument registers are
    // forwarded so the original receives exactly what ELS passed, whatever its real arity.
    using Routine = uint64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t);

    inline int32_t Handle(uint64_t argument) { return static_cast<int32_t>(static_cast<uint32_t>(argument)); }

    // Finds ELS.asi and confirms it is V 1.05, unmodified at every location the extension uses.
    bool Attach();
    void Detach();

    const char* Name(Function function);
    uint32_t Rva(Site site);
    // RVA of an address inside ELS's code section, or 0.
    uint32_t CodeRva(uint64_t address);

    bool Hook(Function target, void* replacement, Routine* original);
    bool EnableHooks();
    bool WriteCodeByte(uint32_t rva, uint8_t value, uint8_t* previous);

    ElsVehState* State(int32_t vehicle);
    void StopSiren(int32_t vehicle);
    void UpdateHornSound(int32_t vehicle);
    int32_t PresetFieldIndex(int32_t vehicle, int32_t lightGroup);
    void SetExtra(int32_t vehicle, int32_t extra, bool on);
    void DrawTakedowns(int32_t vehicle);
    void DrawSceneLights(int32_t vehicle);
}
