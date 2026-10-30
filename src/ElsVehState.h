// Per-vehicle state block of ELS V 1.05, as returned by Veh_GetState.
//
// 0x000-0x1AF holds the vehicle's parsed configuration file; the rest is runtime state.
// Pattern fields hold a pattern number, or -1 when disabled.
#pragma once
#include <cstddef>
#include <cstdint>

struct ElsCoronaState
{
    int32_t patternType;
    int32_t cycle;
    int32_t flag;
};

struct ElsVehState
{
    int32_t vcfLoaded;                          // 0x000
    int32_t lstgActivationType;                 // 0x004  manual 0, invert 1, auto 2, euro 3
    int32_t infoPanelHeaderColor;               // 0x008
    int32_t infoPanelButtonLightColor;          // 0x00C
    int32_t vehicleIsSlicktop;                  // 0x010
    int32_t dfltSirenLtsActivateAtLstg;         // 0x014
    int32_t extraIsElsControlled[12];           // 0x018  Extra01-Extra12
    int32_t prmlLightingFormat;                 // 0x048  leds 0, dro1 1, dro2 2, dro3 3, rota 4, rmix 5, strb 6, chp 7
    int32_t prmlPatternCount;                   // 0x04C
    int32_t prmlExtrasActiveAtLstg2;            // 0x050  1and4 0, 2and3 1, all 2
    int32_t seclLightingFormat;                 // 0x054  leds 8, strb 9, traf 10, arrw 11, marq 12, chp 13
    int32_t seclPatternCount;                   // 0x058
    int32_t wrnlLightingFormat;                 // 0x05C  leds 14, strb 15, chp 16
    int32_t wrnlPatternCount;                   // 0x060
    int32_t wrnlForcedMainHorn;                 // 0x064
    int32_t wrnlForcedSrnTone[4];               // 0x068
    int32_t wrnlForcedPanicMde;                 // 0x078
    int32_t wrnlForcedOutOfVeh;                 // 0x07C
    int32_t prmlForcedMainHorn;                 // 0x080
    int32_t prmlForcedSrnTone[4];               // 0x084
    int32_t prmlForcedPanicMde;                 // 0x094
    int32_t prmlForcedOutOfVeh;                 // 0x098
    int32_t seclDisableAtLstg3;                 // 0x09C
    int32_t seclForcedMainHorn;                 // 0x0A0
    int32_t seclForcedSrnTone[4];               // 0x0A4
    int32_t seclForcedPanicMde;                 // 0x0B4
    int32_t seclForcedOutOfVeh;                 // 0x0B8
    int32_t wrnlScanPoolEnabled;                // 0x0BC
    int32_t wrnlScanPoolSequential;             // 0x0C0
    int32_t wrnlScanPoolCount;                  // 0x0C4
    int32_t prmlScanPoolEnabled;                // 0x0C8
    int32_t prmlScanPoolSequential;             // 0x0CC
    int32_t prmlScanPoolCount;                  // 0x0D0
    int32_t seclScanPoolEnabled;                // 0x0D4
    int32_t seclScanPoolSequential;             // 0x0D8
    int32_t seclScanPoolCount;                  // 0x0DC
    uint8_t unknown0E0[4];                      // 0x0E0
    int32_t cruiseDisableAtLstg3;               // 0x0E4
    int32_t cruiseUseExtra[4];                  // 0x0E8
    int32_t takedownsAllowUse;                  // 0x0F8
    int32_t takedownsMirrored;                  // 0x0FC
    int32_t sceneLightsAllowUse;                // 0x100
    int32_t sceneLightsIlluminateSidesOnly;     // 0x104
    int32_t useSteadyBurnLights;                // 0x108
    int32_t mainHornInterruptsSiren;            // 0x10C
    int32_t manToneAllowUse[2];                 // 0x110
    int32_t srnToneAllowUse[4];                 // 0x118
    int32_t auxSirenAllowUse;                   // 0x128
    int32_t panicMdeAllowUse;                   // 0x12C
    int32_t arrowboardType;                     // 0x130  off 0, bonnet 1, boot 2, boot2 3, boots 4
    int32_t extraEnvLightColor[9];              // 0x134  Extra01-Extra09, -1 when AllowEnvLight is off
    int32_t headlightsColor[2];                 // 0x158  left, right: red 0, blue 1, eblue 2, white 3, amber 4, green 5
    int32_t tailLightsColor[2];                 // 0x160
    int32_t indicatorsFColor[2];                // 0x168
    int32_t indicatorsBColor[2];                // 0x170
    int32_t reverseLightsColor[2];              // 0x178
    int32_t sirenHandsFreeMode;                 // 0x180
    int32_t headlightsDfltPattern;              // 0x184
    int32_t tailLightsDfltPattern;              // 0x188
    int32_t indicatorsFDfltPattern;             // 0x18C
    int32_t indicatorsBDfltPattern;             // 0x190
    int32_t reverseLightsDfltPattern;           // 0x194
    int32_t wrnlPresetLstg3;                    // 0x198
    int32_t prmlPresetLstg2;                    // 0x19C
    int32_t prmlPresetLstg3;                    // 0x1A0
    int32_t seclPresetLstg1;                    // 0x1A4
    int32_t seclPresetLstg2;                    // 0x1A8
    int32_t seclPresetLstg3;                    // 0x1AC

    int32_t initialisedFromVcf;                 // 0x1B0
    int32_t wrnlSavedPattern;                   // 0x1B4  user-selected pattern, restored after a forced one
    int32_t wrnlPrevPattern;                    // 0x1B8
    int32_t prmlSavedPattern;                   // 0x1BC
    int32_t prmlPrevPattern;                    // 0x1C0
    int32_t seclSavedPattern;                   // 0x1C4
    int32_t seclPrevPattern;                    // 0x1C8
    int32_t isPlayerControlled;                 // 0x1CC
    int32_t isRemotePlayerDriven;               // 0x1D0
    int32_t remotePlayerIndex;                  // 0x1D4
    int32_t wrnlForcedPattern;                  // 0x1D8  forced or stage-preset pattern in effect
    int32_t prmlForcedPattern;                  // 0x1DC
    int32_t seclForcedPattern;                  // 0x1E0
    int32_t wrnlScanPickedPattern;              // 0x1E4  -1 when not scanning
    int32_t prmlScanPickedPattern;              // 0x1E8
    int32_t seclScanPickedPattern;              // 0x1EC
    int32_t lightStage;                         // 0x1F0  0-3
    int32_t prmlStep;                           // 0x1F4
    int32_t seclOn;                             // 0x1F8
    int32_t seclStep;                           // 0x1FC
    int32_t wrnlOn;                             // 0x200
    int32_t wrnlStep;                           // 0x204
    int32_t cruiseLightsOn;                     // 0x208
    int32_t takedownOn;                         // 0x20C
    int32_t sceneLightsOn;                      // 0x210
    int32_t steadyBurnOn;                       // 0x214
    int32_t interiorLightOn;                    // 0x218
    int32_t blackoutOn;                         // 0x21C
    int32_t arrowboardSequence;                 // 0x220  idle 0, opening 1, closing 2, open 3
    int32_t sirenPlaying;                       // 0x224
    int32_t auxSirenPlaying;                    // 0x228
    int32_t sirenToneIndex;                     // 0x22C
    int32_t sirenScanMode;                      // 0x230
    int32_t hornActive;                         // 0x234
    int32_t hornKind;                           // 0x238  main horn 1, manual tones 2 and 3
    int32_t hornSoundId;                        // 0x23C
    int32_t hornKindPlaying;                    // 0x240
    int32_t sirenSoundId;                       // 0x244
    int32_t auxSirenSoundId;                    // 0x248
    int32_t wrnlPatternListIndex;               // 0x24C
    int32_t prmlPatternListIndex;               // 0x250
    int32_t seclPatternListIndex;               // 0x254
    int32_t rotatorAngle[4];                    // 0x258
    uint8_t unknown268[4];                      // 0x268
    ElsCoronaState coronas[5];                  // 0x26C  headlights, tail lights, front and rear indicators, reverse lights
    int32_t syncProp[20];                       // 0x2A8
};

static_assert(sizeof(ElsVehState) == 0x2F8);
static_assert(offsetof(ElsVehState, prmlLightingFormat) == 0x048);
static_assert(offsetof(ElsVehState, wrnlForcedOutOfVeh) == 0x07C);
static_assert(offsetof(ElsVehState, prmlForcedOutOfVeh) == 0x098);
static_assert(offsetof(ElsVehState, seclForcedOutOfVeh) == 0x0B8);
static_assert(offsetof(ElsVehState, takedownsAllowUse) == 0x0F8);
static_assert(offsetof(ElsVehState, wrnlPresetLstg3) == 0x198);
static_assert(offsetof(ElsVehState, initialisedFromVcf) == 0x1B0);
static_assert(offsetof(ElsVehState, isPlayerControlled) == 0x1CC);
static_assert(offsetof(ElsVehState, wrnlForcedPattern) == 0x1D8);
static_assert(offsetof(ElsVehState, lightStage) == 0x1F0);
static_assert(offsetof(ElsVehState, sirenPlaying) == 0x224);
static_assert(offsetof(ElsVehState, auxSirenSoundId) == 0x248);
static_assert(offsetof(ElsVehState, coronas) == 0x26C);
