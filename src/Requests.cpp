#include "Requests.h"
#include "Extension.h"
#include "Log.h"
#include "Natives.h"

#include <mutex>
#include <shared_mutex>

using requests::Request;

namespace
{
    struct Entry
    {
        int32_t vehicle;
        Request request;
    };

    constexpr int kMaxEntries = 256;

    std::shared_mutex g_lock;
    Entry g_entries[kMaxEntries];
    int g_count = 0;

    int IndexOf(int32_t vehicle)
    {
        for (int i = 0; i < g_count; ++i)
            if (g_entries[i].vehicle == vehicle) return i;
        return -1;
    }

    void RemoveAt(int index) { g_entries[index] = g_entries[--g_count]; }

    void LogChange(int32_t vehicle, const Request& r)
    {
        static const char* const kAdvisor[] = { "off", "left", "right", "both" };
        elslog::Line("Vehicle %d: siren %s, stage %d, takedowns %s, scene lights %s, advisor %s, patterns %d/%d/%d.",
                     vehicle, r.sirenMode == ELSEXT_SIREN_SILENT ? "silent" : "auto", r.lightStage == 0 ? 3 : r.lightStage,
                     r.takedowns ? "on" : "off", r.sceneLights ? "on" : "off", kAdvisor[r.advisor],
                     r.pattern[0], r.pattern[1], r.pattern[2]);
    }

    template <typename Change>
    int32_t Modify(int32_t vehicle, Change change)
    {
        if (!ext::IsActive() || vehicle == 0) return 0;

        Request before;
        Request after;
        {
            std::unique_lock lock(g_lock);
            int i = IndexOf(vehicle);
            if (i < 0)
            {
                if (g_count == kMaxEntries) return 0;
                i = g_count++;
                g_entries[i] = { vehicle, Request{} };
            }
            before = g_entries[i].request;
            change(g_entries[i].request);
            after = g_entries[i].request;
            if (after.IsDefault()) RemoveAt(i);
        }
        if (after != before) LogChange(vehicle, after);
        return 1;
    }
}

namespace requests
{
    Request Get(int32_t vehicle)
    {
        std::shared_lock lock(g_lock);
        const int i = IndexOf(vehicle);
        return i >= 0 ? g_entries[i].request : Request{};
    }

    int Count()
    {
        std::shared_lock lock(g_lock);
        return g_count;
    }

    void Prune()
    {
        std::unique_lock lock(g_lock);
        for (int i = 0; i < g_count;)
        {
            if (native::DoesEntityExist(g_entries[i].vehicle)) ++i;
            else RemoveAt(i);
        }
    }
}

ELSEXT_API int32_t ElsExt_SetAiSirenMode(int32_t vehicle, int32_t mode)
{
    if (mode != ELSEXT_SIREN_AUTO && mode != ELSEXT_SIREN_SILENT) return 0;
    return Modify(vehicle, [=](Request& r) { r.sirenMode = mode; });
}

ELSEXT_API int32_t ElsExt_GetAiSirenMode(int32_t vehicle)
{
    return requests::Get(vehicle).sirenMode;
}

ELSEXT_API int32_t ElsExt_SetAiLightStage(int32_t vehicle, int32_t stage)
{
    if (stage < 1 || stage > 3) return 0;
    return Modify(vehicle, [=](Request& r) { r.lightStage = stage == 3 ? 0 : stage; });
}

ELSEXT_API int32_t ElsExt_SetAiSceneLights(int32_t vehicle, int32_t takedowns, int32_t sceneLights)
{
    return Modify(vehicle, [=](Request& r) {
        r.takedowns = takedowns != 0;
        r.sceneLights = sceneLights != 0;
    });
}

ELSEXT_API int32_t ElsExt_SetAiAdvisor(int32_t vehicle, int32_t direction)
{
    if (direction < ELSEXT_ADVISOR_OFF || direction > ELSEXT_ADVISOR_BOTH) return 0;
    return Modify(vehicle, [=](Request& r) { r.advisor = direction; });
}

ELSEXT_API int32_t ElsExt_SetAiPattern(int32_t vehicle, int32_t lightGroup, int32_t pattern)
{
    if (lightGroup < ELSEXT_PRML || lightGroup > ELSEXT_WRNL || pattern > 999) return 0;
    return Modify(vehicle, [=](Request& r) { r.pattern[lightGroup] = pattern < 0 ? -1 : pattern; });
}

ELSEXT_API void ElsExt_ClearAll()
{
    {
        std::unique_lock lock(g_lock);
        g_count = 0;
    }
    elslog::Line("All vehicle settings cleared.");
}
