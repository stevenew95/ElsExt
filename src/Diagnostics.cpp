#include "Diagnostics.h"
#include "Els.h"
#include "Log.h"

#include <cstdio>
#include <utility>

namespace
{
    constexpr long kMaxFaultReports = 20;
    constexpr int kStallSeconds = 15;
    constexpr int kMaxStackScan = 400;
    constexpr int kMaxStackAddresses = 12;

    volatile long g_passes = 0;
    volatile long g_faults = 0;
    volatile long g_lastVehicle = 0;
    const char* volatile g_routine = nullptr;

    struct Watched
    {
        els::Function function;
        els::Routine original;
    };

    Watched g_watched[] =
    {
        { els::Function::Ai_ScanNearbyVehicles },
        { els::Function::Veh_OnPlayerVehicleChanged },
        { els::Function::Net_MarkRemotePlayerVehicles },
        { els::Function::Net_PublishSyncProps },
        { els::Function::Panel_DrawMain },
        { els::Function::Input_HandleToggleKeys },
        { els::Function::Input_HandlePatternChangeKeys },
        { els::Function::Input_HandleSirenToneKeys },
        { els::Function::Lights_UpdateStageFromToggles },
        { els::Function::Lights_UpdateBlackoutMode },
        { els::Function::Player_TogglePanicModeNearestVehicle },
        { els::Function::Veh_PurgeStaleEntries },
        { els::Function::Extras_ApplyOffStateForStage },
    };

    template <size_t I>
    uint64_t WatchedRoutine(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4)
    {
        const char* name = els::Name(g_watched[I].function);
        __try
        {
            const char* previous = diag::Enter(name);
            const uint64_t result = g_watched[I].original(a1, a2, a3, a4);
            diag::Leave(previous);
            return result;
        }
        __except (diag::LogFault(GetExceptionInformation(), name))
        {
            return 0;
        }
    }

    template <size_t... I>
    int HookWatched(std::index_sequence<I...>)
    {
        return (static_cast<int>(els::Hook(g_watched[I].function, &WatchedRoutine<I>, &g_watched[I].original)) + ...);
    }

    // Return addresses into ELS found on the stack, nearest frame first.
    void LogStackTrail(const CONTEXT& context)
    {
        char trail[256] = "";
        size_t used = 0;
        int found = 0;
        __try
        {
            const auto* stack = reinterpret_cast<const uint64_t*>(context.Rsp);
            for (int i = 0; i < kMaxStackScan && found < kMaxStackAddresses; ++i)
            {
                const uint32_t rva = els::CodeRva(stack[i]);
                if (rva == 0) continue;
                const int written = snprintf(trail + used, sizeof(trail) - used, " +0x%X", rva);
                if (written <= 0 || static_cast<size_t>(written) >= sizeof(trail) - used) break;
                used += static_cast<size_t>(written);
                ++found;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
        elslog::Line("  rcx %llX rdx %llX r8 %llX r9 %llX rax %llX; ELS return addresses:%s",
                     context.Rcx, context.Rdx, context.R8, context.R9, context.Rax, found > 0 ? trail : " none");
    }

    struct Watchdog
    {
        long lastPasses = 0;
        int quietSeconds = 0;
        bool started = false;
        bool reported = false;
    } g_watchdog;
}

namespace diag
{
    int LogFault(EXCEPTION_POINTERS* info, const char* routine)
    {
        if (InterlockedIncrement(&g_faults) > kMaxFaultReports || info == nullptr) return EXCEPTION_CONTINUE_SEARCH;

        const EXCEPTION_RECORD& record = *info->ExceptionRecord;
        const auto address = reinterpret_cast<uint64_t>(record.ExceptionAddress);
        const uint32_t rva = els::CodeRva(address);
        char where[32];
        if (rva != 0) snprintf(where, sizeof(where), "ELS.asi+0x%X", rva);
        else snprintf(where, sizeof(where), "%p", record.ExceptionAddress);

        if (record.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record.NumberParameters >= 2)
            elslog::Line("Fault in %s: access violation at %s %s %p (last AI vehicle %ld).", routine, where,
                         record.ExceptionInformation[0] == 1 ? "writing" : "reading",
                         reinterpret_cast<void*>(record.ExceptionInformation[1]), g_lastVehicle);
        else
            elslog::Line("Fault in %s: exception %08lX at %s (last AI vehicle %ld).", routine, record.ExceptionCode, where, g_lastVehicle);

        if (info->ContextRecord != nullptr) LogStackTrail(*info->ContextRecord);
        return EXCEPTION_CONTINUE_SEARCH;
    }

    const char* Enter(const char* routine)
    {
        const char* previous = g_routine;
        g_routine = routine;
        return previous;
    }

    void Leave(const char* previous) { g_routine = previous; }

    void SetLastVehicle(int32_t vehicle) { g_lastVehicle = vehicle; }

    long CountLightingPass() { return InterlockedIncrement(&g_passes); }

    long LightingPasses() { return g_passes; }

    void Install()
    {
        constexpr size_t count = sizeof(g_watched) / sizeof(g_watched[0]);
        const int hooked = HookWatched(std::make_index_sequence<count>{});
        elslog::Line("Fault logging on %d of %zu ELS routines.", hooked, count);
    }

    void Tick()
    {
        Watchdog& w = g_watchdog;
        const long passes = g_passes;
        if (passes != w.lastPasses)
        {
            if (w.reported) elslog::Line("ELS lighting passes resumed.");
            w = { passes, 0, true, false };
            return;
        }
        if (!w.started || w.reported || ++w.quietSeconds < kStallSeconds) return;

        w.reported = true;
        const char* routine = g_routine;
        elslog::Line("ELS has not completed a lighting pass for %d s. This is normal during loading screens; "
                     "otherwise ELS's script has stopped. Last routine: %s. Last AI vehicle: %ld.",
                     kStallSeconds, routine != nullptr ? routine : "none", g_lastVehicle);
    }
}
