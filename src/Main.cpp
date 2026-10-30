#include "Diagnostics.h"
#include "Els.h"
#include "ElsExt.h"
#include "Extension.h"
#include "Fixes.h"
#include "Lighting.h"
#include "Log.h"
#include "Natives.h"
#include "Requests.h"
#include "Settings.h"
#include "Sirens.h"
#include "Vcf.h"

#include <windows.h>
#include <atomic>
#include <cstdio>
#include <cstring>

namespace
{
    constexpr int kHeartbeatSeconds = 60;

    std::atomic<bool> g_active = false;
    char g_gameFolder[MAX_PATH];

    bool FindGameFolder()
    {
        const DWORD length = GetModuleFileNameA(nullptr, g_gameFolder, MAX_PATH);
        if (length == 0 || length >= MAX_PATH) return false;
        char* slash = strrchr(g_gameFolder, '\\');
        if (slash == nullptr) return false;
        slash[1] = '\0';
        return true;
    }

    void Start()
    {
        if (!els::Attach())
        {
            elslog::Line("Extension inactive.");
            return;
        }
        if (!sirens::Install() || !lighting::Install() || !fixes::Install())
        {
            els::Detach();
            elslog::Line("Extension inactive.");
            return;
        }
        diag::Install();
        if (!els::EnableHooks())
        {
            els::Detach();
            elslog::Line("Extension inactive.");
            return;
        }
        fixes::PatchLightingLoop();
        g_active = true;
        elslog::Line("ELS V 1.05 found. Extension active.");
    }

    void Stop()
    {
        g_active = false;
        fixes::Remove();
        els::Detach();
    }

    void ScriptMain()
    {
        // ScriptHookV restarts scripts when a session reloads; the hooks are installed once.
        static bool started = false;
        if (!started)
        {
            started = true;
            Start();
        }

        for (int seconds = 1;; ++seconds)
        {
            if (g_active)
            {
                requests::Prune();
                lighting::Prune();
                diag::Tick();
                if (seconds % kHeartbeatSeconds == 0)
                    elslog::Line("Running: %ld lighting passes, %d vehicles tracked, %d with settings.",
                                 diag::LightingPasses(), lighting::TrackedCount(), requests::Count());
            }
            scriptWait(1000);
        }
    }
}

bool ext::IsActive() { return g_active; }

ELSEXT_API int32_t ElsExt_GetApiVersion() { return ELSEXT_API_VERSION; }

ELSEXT_API int32_t ElsExt_IsActive() { return g_active ? 1 : 0; }

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(module);
        if (FindGameFolder())
        {
            char logPath[MAX_PATH];
            snprintf(logPath, sizeof(logPath), "%sElsExt.log", g_gameFolder);
            elslog::Open(logPath);
            elslog::Line("ElsExt loaded (API version %d).", ELSEXT_API_VERSION);
            settings::Load(g_gameFolder);
            // Before ELS reads the files, so a file that stops ELS from loading is already reported.
            vcf::Load(g_gameFolder);
        }
        scriptRegister(module, ScriptMain);
        break;

    case DLL_PROCESS_DETACH:
        if (reserved != nullptr) break;     // process exit: leave ELS's memory untouched
        scriptUnregister(module);
        Stop();
        elslog::Close();
        break;
    }
    return TRUE;
}
