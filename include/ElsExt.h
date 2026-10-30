// ElsExt public interface.
//
// All functions are exported with C linkage from ElsExt.asi, may be called from any thread and
// return immediately. Vehicles are identified by their script handle. Settings made for a vehicle
// are discarded when the vehicle no longer exists.
#pragma once
#include <cstdint>

#ifdef ELSEXT_EXPORTS
#define ELSEXT_API extern "C" __declspec(dllexport)
#else
#define ELSEXT_API extern "C" __declspec(dllimport)
#endif

#define ELSEXT_API_VERSION 1

enum ElsExtSirenMode : int32_t
{
    ELSEXT_SIREN_AUTO = 0,      // ELS default: siren and horn while a driver is seated
    ELSEXT_SIREN_SILENT = 1,    // lights only
};

enum ElsExtLightGroup : int32_t
{
    ELSEXT_PRML = 0,
    ELSEXT_SECL = 1,
    ELSEXT_WRNL = 2,
};

enum ElsExtAdvisor : int32_t
{
    ELSEXT_ADVISOR_OFF = 0,
    ELSEXT_ADVISOR_LEFT = 1,
    ELSEXT_ADVISOR_RIGHT = 2,
    ELSEXT_ADVISOR_BOTH = 3,
};

// ELSEXT_API_VERSION of the loaded module.
ELSEXT_API int32_t ElsExt_GetApiVersion();

// 1 when ELS V 1.05 was recognised and the extension is running, otherwise 0.
// While 0, every Set function returns 0 and has no effect.
ELSEXT_API int32_t ElsExt_IsActive();

// The Set functions below return 1 when the setting was stored and 0 when the extension is not
// active, an argument is out of range, or the per-vehicle table is full. They apply to vehicles
// ELS is lighting for an AI driver.

ELSEXT_API int32_t ElsExt_SetAiSirenMode(int32_t vehicle, int32_t mode);
ELSEXT_API int32_t ElsExt_GetAiSirenMode(int32_t vehicle);

// 1 = secondary lights only, 2 = primary and secondary, 3 = ELS default (all groups).
ELSEXT_API int32_t ElsExt_SetAiLightStage(int32_t vehicle, int32_t stage);

// Takedowns (extra 11) and scene lights (extra 12), each 0 or 1. Ignored where the vehicle's
// configuration file does not allow them.
ELSEXT_API int32_t ElsExt_SetAiSceneLights(int32_t vehicle, int32_t takedowns, int32_t sceneLights);

// Runs the SECL pattern the model names for the direction in <EXT><Advisor>.
ELSEXT_API int32_t ElsExt_SetAiAdvisor(int32_t vehicle, int32_t direction);

// Fixes one light group to a pattern number (0-999); -1 releases it. Overrides park and advisor
// patterns. Horn and siren-tone forced patterns still take precedence.
ELSEXT_API int32_t ElsExt_SetAiPattern(int32_t vehicle, int32_t lightGroup, int32_t pattern);

// Discards every setting made through this interface.
ELSEXT_API void ElsExt_ClearAll();
