#include "Els.h"
#include "Log.h"

#include <windows.h>
#include <cstring>

#include "MinHook.h"

namespace
{
    uint8_t* g_base = nullptr;

    const els::Signature& Entry(els::Function function) { return els::kFunctions[static_cast<int>(function)]; }

    uint64_t Call(els::Function function, uint64_t a1, uint64_t a2 = 0, uint64_t a3 = 0, uint64_t a4 = 0)
    {
        return reinterpret_cast<els::Routine>(g_base + Entry(function).rva)(a1, a2, a3, a4);
    }

    uint64_t Arg(int32_t handle) { return static_cast<uint32_t>(handle); }

    bool IsVersion105(const uint8_t* base)
    {
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
        if (nt->FileHeader.TimeDateStamp == els::kTimestamp && nt->OptionalHeader.SizeOfImage == els::kImageSize) return true;
        elslog::Line("ELS.asi is not V 1.05 (timestamp %08lX, image size 0x%lX).",
                     nt->FileHeader.TimeDateStamp, nt->OptionalHeader.SizeOfImage);
        return false;
    }

    template <size_t N>
    bool AllMatch(const uint8_t* base, const els::Signature (&table)[N])
    {
        for (const els::Signature& s : table)
        {
            if (memcmp(base + s.rva, s.bytes, els::kSignatureLength) == 0) continue;
            elslog::Line("ELS.asi differs from V 1.05 at %s; another mod may have modified it.", s.name);
            return false;
        }
        return true;
    }
}

namespace els
{
    bool Attach()
    {
        auto* base = reinterpret_cast<uint8_t*>(GetModuleHandleA("ELS.asi"));
        if (base == nullptr)
        {
            elslog::Line("ELS.asi is not loaded.");
            return false;
        }
        if (!IsVersion105(base) || !AllMatch(base, kFunctions) || !AllMatch(base, kSites)) return false;

        const MH_STATUS status = MH_Initialize();
        if (status != MH_OK)
        {
            elslog::Line("MinHook failed to initialise: %s", MH_StatusToString(status));
            return false;
        }
        g_base = base;
        return true;
    }

    void Detach()
    {
        if (g_base == nullptr) return;
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        g_base = nullptr;
    }

    const char* Name(Function function) { return Entry(function).name; }

    uint32_t Rva(Site site) { return kSites[static_cast<int>(site)].rva; }

    uint32_t CodeRva(uint64_t address)
    {
        const auto base = reinterpret_cast<uint64_t>(g_base);
        if (g_base == nullptr || address < base + kCodeStart || address >= base + kCodeEnd) return 0;
        return static_cast<uint32_t>(address - base);
    }

    bool Hook(Function target, void* replacement, Routine* original)
    {
        const MH_STATUS status = MH_CreateHook(g_base + Entry(target).rva, replacement, reinterpret_cast<void**>(original));
        if (status == MH_OK) return true;
        elslog::Line("Could not hook %s: %s", Name(target), MH_StatusToString(status));
        return false;
    }

    bool EnableHooks()
    {
        const MH_STATUS status = MH_EnableHook(MH_ALL_HOOKS);
        if (status == MH_OK) return true;
        elslog::Line("Could not enable hooks: %s", MH_StatusToString(status));
        return false;
    }

    bool WriteCodeByte(uint32_t rva, uint8_t value, uint8_t* previous)
    {
        uint8_t* at = g_base + rva;
        DWORD protection = 0;
        if (!VirtualProtect(at, 1, PAGE_EXECUTE_READWRITE, &protection)) return false;
        if (previous != nullptr) *previous = *at;
        *at = value;
        VirtualProtect(at, 1, protection, &protection);
        FlushInstructionCache(GetCurrentProcess(), at, 1);
        return true;
    }

    ElsVehState* State(int32_t vehicle)
    {
        return reinterpret_cast<ElsVehState*>(Call(Function::Veh_GetState, reinterpret_cast<uint64_t>(g_base + kVehicleStateMap),
                                                   reinterpret_cast<uint64_t>(&vehicle)));
    }

    void StopSiren(int32_t vehicle) { Call(Function::Siren_PlayOrStop, Arg(vehicle), 0); }

    void UpdateHornSound(int32_t vehicle) { Call(Function::Horn_UpdateSound, Arg(vehicle)); }

    int32_t PresetFieldIndex(int32_t vehicle, int32_t lightGroup)
    {
        return static_cast<int32_t>(Call(Function::Lights_GetPresetFieldIndex, Arg(vehicle), Arg(lightGroup)));
    }

    void SetExtra(int32_t vehicle, int32_t extra, bool on)
    {
        Call(Function::Extras_SetExtraWithRules, Arg(vehicle), Arg(extra), on ? 1 : 0);
    }

    void DrawTakedowns(int32_t vehicle) { Call(Function::Lights_DrawExtra11, Arg(vehicle), 11); }

    void DrawSceneLights(int32_t vehicle) { Call(Function::Lights_DrawExtra12, Arg(vehicle)); }
}
