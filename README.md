# ElsExt

ElsExt is a runtime extension for Emergency Lighting System V (ELS) 1.05. It extends ELS with parked-vehicle lighting behaviour and a programmatic interface for controlling AI-driven emergency vehicles, and it corrects several defects in the ELS 1.05 release.

ElsExt operates entirely in memory. It does not modify, replace or redistribute any ELS file, and it restores ELS to its original state when unloaded.

## Contents

1. [Capabilities](#capabilities)
2. [System Requirements](#system-requirements)
3. [Installation](#installation)
4. [Configuration](#configuration)
5. [Vehicle Configuration Reference](#vehicle-configuration-reference)
6. [Integration Guide](#integration-guide)
7. [Building from Source](#building-from-source)
8. [Architecture](#architecture)
9. [Troubleshooting](#troubleshooting)
10. [License](#license)
11. [Acknowledgements](#acknowledgements)

## Capabilities

### Parked-vehicle lighting

| Feature | Description |
|---|---|
| Park patterns | Each vehicle model may define the pattern shown by each light group while the vehicle is [parked](#parked-vehicles). The park pattern replaces the ELS `OutOfVeh` pattern and the active stage preset. Horn and siren-tone patterns retain precedence. |
| Synchronisation | [Parked](#parked-vehicles) vehicles belonging to the same sync group, and within range of one another, flash in step. ElsExt aligns pattern timing only and never changes the pattern a vehicle displays. Vehicles in a group must therefore share the same park patterns to appear synchronised. |

Both features apply whether the driver is in the vehicle or not.

### AI vehicle control

Third-party modifications can control the following for any AI-driven vehicle that ELS is lighting:

- Siren mode (ELS default or lights only)
- Light stage 1 or 2, in addition to the stage 3 that ELS applies to all AI vehicles
- Takedown and scene lights
- Traffic advisor direction
- A fixed pattern for any light group

### Corrections to ELS 1.05

| Defect | Resolution |
|---|---|
| When ELS released a vehicle, its siren and horn sounds continued playing with no means of stopping them. | Sounds are stopped before the vehicle is released. |
| When a lit vehicle was deleted, every vehicle after it in the ELS vehicle list skipped that lighting pass. | The lighting loop continues to the next vehicle. |
| A pattern key pressed during a forced pattern, at a light stage without a preset for that group, overwrote memory in the ELS vehicle table. | The key changes the vehicle's saved pattern instead. |

### Diagnostics

- **Vehicle configuration validation.** ELS reads every setting in a vehicle configuration file without verifying that it exists, and a single missing setting can prevent ELS from loading. ElsExt validates every file at startup and records each missing setting in `ElsExt.log`.
- **Fault reporting.** Exceptions raised in the ELS per-frame routines are recorded with the routine, address and call stack. An interruption of the ELS script is also recorded.

## System Requirements

| Component | Version |
|---|---|
| Grand Theft Auto V | PC |
| ScriptHookV | Current release for the installed game version |
| Emergency Lighting System V | 1.05 |

ElsExt verifies the installed ELS build at startup. With any build other than 1.05, ElsExt remains inactive and records the reason in its log.

## Installation

1. Close the game.
2. Copy `ElsExt.asi` and `ElsExt.ini` into the game installation folder, alongside `ELS.asi`.
3. Start the game.
4. Open `ElsExt.log` in the game installation folder and confirm that it contains `Extension active.`

To uninstall, delete `ElsExt.asi` and `ElsExt.ini`.

## Configuration

Global settings are read from `ElsExt.ini` in the game installation folder. If the file is absent, the defaults below apply.

| Section | Key | Default | Range | Description |
|---|---|---|---|---|
| `Park` | `Enabled` | `1` | `0`, `1` | Enables park patterns. |
| `Park` | `PlayerCar` | `1` | `0`, `1` | Applies park patterns to the vehicle the player is driving. |
| `Park` | `DelaySeconds` | `2` | `0` to `600` | Time a vehicle must remain stationary before it counts as parked. Applies to park patterns and synchronisation. |
| `Sync` | `Enabled` | `1` | `0`, `1` | Enables synchronisation. |
| `Sync` | `PlayerCar` | `1` | `0`, `1` | Includes the vehicle the player is driving. |
| `Sync` | `RadiusMetres` | `60` | `1` to `1000` | Maximum distance between synchronised vehicles. |

Values outside the stated range are ignored and the default is used.

## Vehicle Configuration Reference

Per-model behaviour is defined in an `<EXT>` element inside the `<vcfroot>` element of the model's ELS configuration file (`ELS\<pack>\<model>.xml`). ELS ignores this element. Files without it are unaffected.

```xml
<EXT>
    <Park Prml="32" Secl="32" Wrnl="2" Delay="2" />
    <Sync Group="LSPD" Radius="60" />
    <Advisor Left="12" Right="13" Both="14" />
</EXT>
```

### Parked vehicles

Park patterns and synchronisation apply to parked vehicles. A vehicle is parked once it has been stationary (below 0.3 m/s) with its emergency lights on for the park delay: the model's `Delay`, or `DelaySeconds` in `ElsExt.ini`. It stops being parked as soon as it moves faster than 1.5 m/s.

A parked vehicle may have its driver in the seat or not. The driver's position affects only which light stages a park pattern can show at; see `<Park>` below.

### `<Park>`

| Attribute | Type | Description |
|---|---|---|
| `Prml`, `Secl`, `Wrnl` | Pattern number, `0` to `999` | Pattern for the corresponding light group. Omit an attribute to leave that group unchanged. |
| `Delay` | Seconds, `0` to `600` | Overrides `DelaySeconds` for this model. Also sets when this model starts synchronising. |
| `Enabled` | `true`, `false` | Disables the element when `false`. Default `true`. |

A parked vehicle with the driver seated shows its park pattern only at light stages for which ELS defines a preset for that group:

| Light group | Stages |
|---|---|
| PRML | 2, 3 |
| SECL | 1, 2, 3 |
| WRNL | 3 |

With the driver's seat empty, the park pattern shows at every stage.

### `<Sync>`

| Attribute | Type | Description |
|---|---|---|
| `Group` | Text, case-insensitive | Parked vehicles of models with the same group name synchronise with each other. When omitted, the model synchronises only with vehicles of the same model. |
| `Radius` | Metres, `1` to `1000` | Overrides `RadiusMetres` for this model. Where two vehicles specify different values, the smaller applies. |
| `Enabled` | `true`, `false` | Disables the element when `false`. Default `true`. |

### `<Advisor>`

| Attribute | Type | Description |
|---|---|---|
| `Left`, `Right`, `Both` | Pattern number, `0` to `999` | SECL patterns used when a traffic advisor direction is set through `ElsExt_SetAiAdvisor`. |

## Integration Guide

### Interface

The interface is declared in [include/ElsExt.h](include/ElsExt.h). All functions use C linkage, are thread-safe and return immediately. Vehicles are identified by script handle. Settings for a vehicle are discarded automatically when the vehicle no longer exists.

| Function | Parameters | Description |
|---|---|---|
| `ElsExt_GetApiVersion()` | | Returns the interface version of the loaded module. |
| `ElsExt_IsActive()` | | Returns `1` when ELS 1.05 was verified and the extension is running, otherwise `0`. |
| `ElsExt_SetAiSirenMode(vehicle, mode)` | `mode`: `0` ELS default, `1` lights only | Sets the siren mode. |
| `ElsExt_GetAiSirenMode(vehicle)` | | Returns the current siren mode. |
| `ElsExt_SetAiLightStage(vehicle, stage)` | `stage`: `1` SECL only, `2` PRML and SECL, `3` ELS default | Sets the light stage. |
| `ElsExt_SetAiSceneLights(vehicle, takedowns, sceneLights)` | `0` or `1` each | Switches takedown and scene lights. Has no effect where the vehicle configuration does not permit them. |
| `ElsExt_SetAiAdvisor(vehicle, direction)` | `direction`: `0` off, `1` left, `2` right, `3` both | Sets the traffic advisor direction. |
| `ElsExt_SetAiPattern(vehicle, lightGroup, pattern)` | `lightGroup`: `0` PRML, `1` SECL, `2` WRNL; `pattern`: `0` to `999`, or `-1` to release | Fixes a light group to a pattern. Takes precedence over park and advisor patterns. Horn and siren-tone patterns retain precedence. |
| `ElsExt_ClearAll()` | | Discards every setting made through the interface. |

### Return values

Each `Set` function returns `1` when the setting is stored. It returns `0` when:

- the extension is inactive,
- an argument is outside its valid range, or
- the per-vehicle table (256 entries) is full.

Settings take effect while ELS is lighting the vehicle for an AI driver.

### Binding

ScriptHookV loads `ElsExt.asi` at startup. Bind to it at runtime and treat an absent module as "not installed". Do not link against it at build time.

**C++**

```cpp
#include "ElsExt.h"

HMODULE elsExt = GetModuleHandleA("ElsExt.asi");
if (elsExt != nullptr)
{
    auto setSirenMode = reinterpret_cast<decltype(&ElsExt_SetAiSirenMode)>(
        GetProcAddress(elsExt, "ElsExt_SetAiSirenMode"));
    setSirenMode(vehicle, ELSEXT_SIREN_SILENT);
}
```

**C#**

```csharp
[DllImport("ElsExt.asi")] static extern int ElsExt_IsActive();
[DllImport("ElsExt.asi")] static extern int ElsExt_SetAiSirenMode(int vehicle, int mode);

if (ElsExt_IsActive() == 1)
    ElsExt_SetAiSirenMode(vehicleHandle, 1);
```

In C#, the first call throws `DllNotFoundException` when ElsExt is not installed. Handle this exception and disable the integration.

### Versioning

`ElsExt_GetApiVersion()` returns `ELSEXT_API_VERSION`. The version increases when functions are added. Check it before calling a function introduced after version 1.

## Building from Source

### Prerequisites

- Visual Studio 2022 with the **Desktop development with C++** workload
- Python 3, only to regenerate `src/ElsLayout.h`

The ScriptHookV SDK is not required. The build generates an import library from `src/ScriptHookV.def`.

### Build

```
msbuild ElsExt.vcxproj -p:Configuration=Release -p:Platform=x64
```

The output is written to `bin\ElsExt.asi`. The module links the C runtime statically and has no redistributable dependency.

### Verification tools

Run `tools\build.bat` to build the following into `obj\tools`.

| Tool | Usage | Purpose |
|---|---|---|
| `hooktest` | `hooktest <path to ELS.asi>` | Verifies an `ELS.asi` against `src/ElsLayout.h` and confirms that every hook installs and uninstalls cleanly. Runs outside the game. |
| `vcftest` | `vcftest <game folder>` | Runs vehicle configuration validation and the `<EXT>` reader, and prints the results. |

### Layout generation

`src/ElsLayout.h` contains the ELS 1.05 build identity and the signature of every function and code location ElsExt uses. Regenerate it with:

```
python tools\gen_layout.py <path to ELS.asi>
```

The generator rejects any signature that overlaps a relocation entry.

## Architecture

### Verification

At startup ElsExt compares the loaded `ELS.asi` against the 1.05 build timestamp, image size, and a 24-byte signature at every function and code location it uses. Any mismatch leaves ELS untouched and ElsExt inactive.

### Hooking

Functions are hooked with MinHook. Most hooks adjust the ELS per-vehicle state for the duration of a single call to the original ELS routine and restore it on return, so that ELS's own logic carries out each change. This preserves ELS behaviour, including its pattern transitions, wherever ElsExt has nothing to apply.

### Source layout

| Path | Contents |
|---|---|
| `include/ElsExt.h` | Public interface |
| `src/Main.cpp` | Module entry point, startup and shutdown |
| `src/Els.*` | ELS verification, hook management and calls into ELS |
| `src/ElsLayout.h` | Generated ELS 1.05 signatures |
| `src/ElsVehState.h` | ELS per-vehicle state structure |
| `src/Requests.*` | Per-vehicle settings and exported functions |
| `src/Lighting.*` | Park patterns, synchronisation, light stage and scene lighting |
| `src/Sirens.*` | Siren mode |
| `src/Fixes.*` | Corrections to ELS defects |
| `src/Diagnostics.*` | Fault reporting and script monitoring |
| `src/Vcf.*` | Vehicle configuration validation and `<EXT>` reader |
| `src/Settings.*` | `ElsExt.ini` reader |
| `tools/` | Verification tools and layout generator |
| `third_party/minhook/` | MinHook 1.3.3 |

### Unloading

When the module unloads, all hooks and the lighting-loop patch are removed. On process exit, ELS memory is left as is.

## Troubleshooting

All messages are written to `ElsExt.log` in the game installation folder. The file is replaced each time the game starts.

| Log message | Cause | Action |
|---|---|---|
| `ELS.asi is not loaded.` | ELS is not installed or failed to load. | Install ELS 1.05 and confirm it loads without ElsExt. |
| `ELS.asi is not V 1.05` | A different ELS build is installed. | Install ELS 1.05. |
| `ELS.asi differs from V 1.05 at <location>` | `ELS.asi` has been modified, on disk or in memory, by another modification. | Restore the original `ELS.asi` and remove the other modification. |
| `Extension inactive.` | Follows any of the messages above. | Resolve the preceding message. |
| `ElsExt.ini not found; using defaults.` | `ElsExt.ini` is missing. | Copy `ElsExt.ini` into the game folder, or continue with the defaults. |
| `<file>:` followed by missing settings | A vehicle configuration file lacks settings that ELS requires. | Add the listed settings. ELS may fail to load until they are present. |
| `ELS has not completed a lighting pass for 15 s.` | Expected during loading screens. Otherwise, the ELS script has stopped. | If it occurs outside a loading screen, review any preceding `Fault in` entry. |
| `Fault in <routine>` | An exception occurred inside ELS. | Include the full log when reporting the issue. |

## License

ElsExt is released under the MIT License. See [LICENSE](LICENSE). The copyright notice and permission notice must be retained in all copies and in all modified or derived versions.

`LICENSE` also contains the notices for the bundled third-party components. Include the file with every source and binary distribution.

## Acknowledgements

- **Emergency Lighting System V** by Lt.Caine, with LMS and Sam. ELS is not included and must be obtained separately.
- **ScriptHookV** by Alexander Blade. Only its exported function names are referenced.
- **MinHook** by Tsuda Kageyu, licensed under the BSD 2-Clause License. See [third_party/minhook/LICENSE.txt](third_party/minhook/LICENSE.txt).
