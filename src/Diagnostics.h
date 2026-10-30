// Fault reporting for ELS's per-frame routines and detection of ELS's script stopping.
#pragma once
#include <windows.h>
#include <cstdint>

namespace diag
{
    // Exception filter: logs the fault and lets it continue to the handler ELS would have reached.
    int LogFault(EXCEPTION_POINTERS* info, const char* routine);

    // Records the ELS routine currently running, for the stall report. Returns the previous one.
    const char* Enter(const char* routine);
    void Leave(const char* previous);

    void SetLastVehicle(int32_t vehicle);

    long CountLightingPass();
    long LightingPasses();

    // Wraps ELS's remaining per-frame routines so faults inside them are logged. Optional:
    // a routine that cannot be wrapped is skipped.
    void Install();

    // Call once a second from the script thread.
    void Tick();
}
