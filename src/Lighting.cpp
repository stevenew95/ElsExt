#include "Lighting.h"
#include "Diagnostics.h"
#include "Els.h"
#include "Log.h"
#include "Natives.h"
#include "Requests.h"
#include "Settings.h"
#include "Vcf.h"

#include <windows.h>
#include <algorithm>
#include <cstring>
#include <initializer_list>

using els::Function;

namespace
{
    constexpr int kMaxTracked = 128;
    constexpr float kStillSpeed = 0.3f;     // m/s
    constexpr float kMovingSpeed = 1.5f;

    struct GroupFields
    {
        int32_t ElsVehState::*format;
        int32_t ElsVehState::*forced;
        int32_t ElsVehState::*scanPicked;
        int32_t ElsVehState::*saved;
        int32_t ElsVehState::*step;
        int32_t ElsVehState::*outOfVeh;
    };

    constexpr GroupFields kGroups[] =
    {
        { &ElsVehState::prmlLightingFormat, &ElsVehState::prmlForcedPattern, &ElsVehState::prmlScanPickedPattern,
          &ElsVehState::prmlSavedPattern, &ElsVehState::prmlStep, &ElsVehState::prmlForcedOutOfVeh },
        { &ElsVehState::seclLightingFormat, &ElsVehState::seclForcedPattern, &ElsVehState::seclScanPickedPattern,
          &ElsVehState::seclSavedPattern, &ElsVehState::seclStep, &ElsVehState::seclForcedOutOfVeh },
        { &ElsVehState::wrnlLightingFormat, &ElsVehState::wrnlForcedPattern, &ElsVehState::wrnlScanPickedPattern,
          &ElsVehState::wrnlSavedPattern, &ElsVehState::wrnlStep, &ElsVehState::wrnlForcedOutOfVeh },
    };

    // A vehicle ELS is lighting. Only touched on the script thread.
    struct Tracked
    {
        int32_t vehicle;
        const vcf::Model* model;
        ULONGLONG stillSince;       // 0 = moving
        ULONGLONG parkedSince;      // 0 = not parked
        long lastPass;
        int32_t leader;             // vehicle it is synced to, 0 = none
        int32_t shownStage;         // stage applied for an AI driver, 0 = ELS default
        bool takedownsOn;
        bool sceneLightsOn;
        bool playerCar;
        native::Vector3 position;
    };

    Tracked g_tracked[kMaxTracked];
    int g_trackedCount = 0;

    els::Routine g_updateActiveVehicles = nullptr;
    els::Routine g_applyForcedPatterns = nullptr;
    els::Routine g_stepPrimary = nullptr;
    els::Routine g_stepSecondary = nullptr;
    els::Routine g_stepWarning = nullptr;

    Tracked* Track(int32_t vehicle)
    {
        for (int i = 0; i < g_trackedCount; ++i)
            if (g_tracked[i].vehicle == vehicle) return &g_tracked[i];
        if (g_trackedCount == kMaxTracked) return nullptr;

        Tracked& t = g_tracked[g_trackedCount++];
        t = {};
        t.vehicle = vehicle;
        t.model = vcf::Find(native::GetEntityModel(vehicle));
        return &t;
    }

    void UpdateParked(Tracked& t, ULONGLONG now)
    {
        const float speed = native::GetEntitySpeed(t.vehicle);
        if (speed > kMovingSpeed)
        {
            t.stillSince = 0;
            t.parkedSince = 0;
            return;
        }
        if (speed >= kStillSpeed || t.parkedSince != 0) return;

        if (t.stillSince == 0) t.stillSince = now;
        const int delay = t.model->parkDelayMs >= 0 ? t.model->parkDelayMs : settings::Get().parkDelayMs;
        if (now - t.stillSince >= static_cast<ULONGLONG>(delay)) t.parkedSince = now;
    }

    int32_t* PresetField(ElsVehState& s, int group)
    {
        switch (group)
        {
        case ELSEXT_PRML:
            return s.lightStage == 2 ? &s.prmlPresetLstg2 : s.lightStage == 3 ? &s.prmlPresetLstg3 : nullptr;
        case ELSEXT_SECL:
            return s.lightStage == 1 ? &s.seclPresetLstg1 : s.lightStage == 2 ? &s.seclPresetLstg2
                 : s.lightStage == 3 ? &s.seclPresetLstg3 : nullptr;
        default:
            return s.lightStage == 3 ? &s.wrnlPresetLstg3 : nullptr;
        }
    }

    // The stage requested for an AI vehicle ELS is lighting, or 0 to keep ELS's stage 3.
    int32_t RequestedStage(int32_t vehicle, const ElsVehState* s)
    {
        if (s == nullptr || s->isPlayerControlled != 0 || s->isRemotePlayerDriven != 0 || s->lightStage != 3) return 0;
        return requests::Get(vehicle).lightStage;
    }

    // ELS lights AI vehicles at stage 3 only. Running a stepper with lightStage set to the
    // requested stage applies ELS's own rules for that stage; groups the stage lacks are skipped.
    uint64_t StepAtRequestedStage(els::Routine original, int32_t minimumStage, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4)
    {
        const int32_t vehicle = els::Handle(a1);
        ElsVehState* s = els::State(vehicle);
        const int32_t stage = RequestedStage(vehicle, s);
        if (stage == 0) return original(a1, a2, a3, a4);
        if (stage < minimumStage) return 0;

        s->lightStage = stage;
        const uint64_t result = original(a1, a2, a3, a4);
        s->lightStage = 3;
        return result;
    }

    uint64_t OnStepPrimary(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4)
    {
        return StepAtRequestedStage(g_stepPrimary, 2, a1, a2, a3, a4);
    }

    uint64_t OnStepSecondary(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4)
    {
        return StepAtRequestedStage(g_stepSecondary, 1, a1, a2, a3, a4);
    }

    uint64_t OnStepWarning(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4)
    {
        return StepAtRequestedStage(g_stepWarning, 3, a1, a2, a3, a4);
    }

    // Precedence: fixed pattern, traffic advisor (SECL only), park pattern. -1 when none applies.
    int32_t PatternFor(const Tracked& t, const requests::Request& r, int group, bool parked)
    {
        if (r.pattern[group] >= 0) return r.pattern[group];
        if (group == ELSEXT_SECL && t.model != nullptr && r.advisor != ELSEXT_ADVISOR_OFF && t.model->advisor[r.advisor - 1] >= 0)
            return t.model->advisor[r.advisor - 1];
        return parked ? t.model->park[group] : -1;
    }

    void ShowExtra(int32_t vehicle, int32_t extra, bool on, bool& wasOn, void (*draw)(int32_t))
    {
        if (on)
        {
            els::SetExtra(vehicle, extra, true);
            draw(vehicle);
        }
        else if (wasOn)
        {
            els::SetExtra(vehicle, extra, false);
        }
        wasOn = on;
    }

    // ELS picks each light group's pattern here every frame, in the order horn, siren tone,
    // driver out of vehicle, stage preset. Requested patterns are written into the last two for
    // the duration of the call, so horn and siren-tone patterns still take precedence and ELS
    // switches patterns with its own transition logic.
    uint64_t OnApplyForcedPatterns(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4)
    {
        const int32_t vehicle = els::Handle(a1);
        ElsVehState* s = els::State(vehicle);
        Tracked* t = Track(vehicle);
        if (s == nullptr || t == nullptr) return g_applyForcedPatterns(a1, a2, a3, a4);

        t->lastPass = diag::LightingPasses();
        t->playerCar = s->isPlayerControlled == 1;
        const requests::Request request = t->playerCar ? requests::Request{} : requests::Get(vehicle);
        if (t->model == nullptr && request.IsDefault() && t->shownStage == 0 && !t->takedownsOn && !t->sceneLightsOn)
            return g_applyForcedPatterns(a1, a2, a3, a4);

        if (t->model != nullptr) UpdateParked(*t, GetTickCount64());

        // Switching every light extra off once on a stage change darkens the groups the new
        // stage lacks and restarts the others cleanly.
        const int32_t stage = RequestedStage(vehicle, s);
        if (stage != t->shownStage)
        {
            for (int32_t extra = 1; extra <= 9; ++extra) els::SetExtra(vehicle, extra, false);
            t->shownStage = stage;
        }
        if (stage != 0) s->lightStage = stage;

        const settings::Settings& config = settings::Get();
        const bool parked = t->parkedSince != 0 && t->model != nullptr && t->model->HasPark() && config.parkEnabled
                         && (!t->playerCar || config.parkPlayerCar);

        int32_t* fields[6];
        int32_t saved[6];
        int count = 0;
        for (int group = ELSEXT_PRML; group <= ELSEXT_WRNL; ++group)
        {
            const int32_t pattern = PatternFor(*t, request, group, parked);
            if (pattern < 0) continue;
            for (int32_t* field : { &(s->*kGroups[group].outOfVeh), PresetField(*s, group) })
            {
                if (field == nullptr) continue;
                fields[count] = field;
                saved[count++] = *field;
                *field = pattern;
            }
        }
        const uint64_t result = g_applyForcedPatterns(a1, a2, a3, a4);
        for (int i = 0; i < count; ++i) *fields[i] = saved[i];
        if (stage != 0) s->lightStage = 3;

        ShowExtra(vehicle, 11, request.takedowns && s->takedownsAllowUse == 1, t->takedownsOn, &els::DrawTakedowns);
        ShowExtra(vehicle, 12, request.sceneLights && s->sceneLightsAllowUse == 1, t->sceneLightsOn, &els::DrawSceneLights);
        return result;
    }

    // Mirrors the two paths of ELS's lighting loop: player vehicles step PRML at stages 2 and 3
    // and SECL and WRNL by their own switches; AI vehicles step every group at stage 3, less
    // what a lower requested stage leaves out.
    bool GroupRunning(const ElsVehState& s, int32_t vehicle, int group)
    {
        if (s.isPlayerControlled == 1 || s.isRemotePlayerDriven == 1)
        {
            switch (group)
            {
            case ELSEXT_PRML: return s.lightStage == 2 || s.lightStage == 3;
            case ELSEXT_SECL: return s.seclOn == 1;
            default: return s.wrnlOn == 1;
            }
        }
        if (s.lightStage != 3) return false;

        const int32_t requested = requests::Get(vehicle).lightStage;
        const int32_t stage = requested != 0 ? requested : 3;
        switch (group)
        {
        case ELSEXT_PRML: return stage >= 2;
        case ELSEXT_SECL: return stage < 3 || s.seclDisableAtLstg3 == 0;
        default: return stage == 3;
        }
    }

    bool SamePattern(const ElsVehState& a, const ElsVehState& b, const GroupFields& g)
    {
        if (a.*g.format != b.*g.format || a.*g.forced != b.*g.forced || a.*g.scanPicked != b.*g.scanPicked) return false;
        return a.*g.forced >= 0 || a.*g.saved == b.*g.saved;
    }

    // An empty group name syncs a model only with itself.
    bool SameSyncGroup(const vcf::Model& a, const vcf::Model& b)
    {
        return strcmp(a.syncGroup, b.syncGroup) == 0 && (a.syncGroup[0] != '\0' || a.hash == b.hash);
    }

    bool InSyncRange(const Tracked& a, const Tracked& b)
    {
        const float defaultRadius = settings::Get().syncRadius;
        const float ra = a.model->syncRadius > 0.0f ? a.model->syncRadius : defaultRadius;
        const float rb = b.model->syncRadius > 0.0f ? b.model->syncRadius : defaultRadius;
        const float range = std::min(ra, rb);
        const float dx = a.position.x - b.position.x;
        const float dy = a.position.y - b.position.y;
        const float dz = a.position.z - b.position.z;
        return dx * dx + dy * dy + dz * dz <= range * range;
    }

    // Every lit vehicle advances one pattern step on the same ELS tick, so vehicles showing the
    // same pattern stay in step once their step counters match. Each parked vehicle takes the
    // counters of the earliest-parked vehicle of its group within range. A group is copied only
    // while ELS is stepping it on both vehicles; copying a stopped counter would freeze the other.
    void SyncParkedVehicles(long pass)
    {
        const settings::Settings& config = settings::Get();
        if (!config.syncEnabled) return;

        Tracked* parked[kMaxTracked];
        int count = 0;
        for (int i = 0; i < g_trackedCount; ++i)
        {
            Tracked& t = g_tracked[i];
            const bool takesPart = t.lastPass == pass && t.parkedSince != 0 && t.model != nullptr && t.model->sync
                                && (!t.playerCar || config.syncPlayerCar);
            if (takesPart) parked[count++] = &t;
            else t.leader = 0;
        }
        std::sort(parked, parked + count, [](const Tracked* a, const Tracked* b) {
            return a->parkedSince != b->parkedSince ? a->parkedSince < b->parkedSince : a->vehicle < b->vehicle;
        });
        for (int i = 0; i < count; ++i) parked[i]->position = native::GetEntityCoords(parked[i]->vehicle);

        for (int i = 0; i < count; ++i)
        {
            Tracked& follower = *parked[i];
            const Tracked* leader = nullptr;
            for (int j = 0; j < i && leader == nullptr; ++j)
                if (SameSyncGroup(*follower.model, *parked[j]->model) && InSyncRange(follower, *parked[j])) leader = parked[j];
            if (leader == nullptr)
            {
                follower.leader = 0;
                continue;
            }

            ElsVehState* f = els::State(follower.vehicle);
            const ElsVehState* l = els::State(leader->vehicle);
            if (f == nullptr || l == nullptr) continue;

            bool inStep = false;
            for (int group = ELSEXT_PRML; group <= ELSEXT_WRNL; ++group)
            {
                const GroupFields& g = kGroups[group];
                if (!GroupRunning(*l, leader->vehicle, group) || !GroupRunning(*f, follower.vehicle, group) || !SamePattern(*l, *f, g))
                    continue;
                f->*g.step = l->*g.step;
                inStep = true;
            }

            if (follower.leader == leader->vehicle) continue;
            follower.leader = leader->vehicle;
            elslog::Line("Vehicle %d (%s) parked with vehicle %d (%s), sync group '%s': %s.",
                         follower.vehicle, follower.model->name, leader->vehicle, leader->model->name,
                         follower.model->syncGroup[0] != '\0' ? follower.model->syncGroup : "(model)",
                         inStep ? "in step" : "different pattern, not synced");
        }
    }

    uint64_t OnUpdateActiveVehicles(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4)
    {
        const char* name = els::Name(Function::Lights_UpdateActiveVehicles);
        __try
        {
            const char* previous = diag::Enter(name);
            const long pass = diag::CountLightingPass();
            const uint64_t result = g_updateActiveVehicles(a1, a2, a3, a4);
            diag::Enter("parked vehicle sync");
            SyncParkedVehicles(pass);
            diag::Leave(previous);
            return result;
        }
        __except (diag::LogFault(GetExceptionInformation(), name))
        {
            return 0;
        }
    }
}

namespace lighting
{
    bool Install()
    {
        return els::Hook(Function::Lights_UpdateActiveVehicles, &OnUpdateActiveVehicles, &g_updateActiveVehicles)
            && els::Hook(Function::Lights_ApplyForcedPatterns, &OnApplyForcedPatterns, &g_applyForcedPatterns)
            && els::Hook(Function::Lights_StepPrimaryPattern, &OnStepPrimary, &g_stepPrimary)
            && els::Hook(Function::Lights_StepSecondaryPattern, &OnStepSecondary, &g_stepSecondary)
            && els::Hook(Function::Lights_StepWarningPattern, &OnStepWarning, &g_stepWarning);
    }

    int TrackedCount() { return g_trackedCount; }

    void Prune()
    {
        for (int i = 0; i < g_trackedCount;)
        {
            if (native::DoesEntityExist(g_tracked[i].vehicle)) ++i;
            else g_tracked[i] = g_tracked[--g_trackedCount];
        }
    }
}
