// ScriptHookV imports and the game natives the extension calls. Script thread only.
#pragma once
#include <windows.h>
#include <cstdint>
#include <type_traits>

__declspec(dllimport) void nativeInit(uint64_t hash);
__declspec(dllimport) void nativePush64(uint64_t value);
__declspec(dllimport) uint64_t* nativeCall();
__declspec(dllimport) void scriptRegister(HMODULE module, void (*main)());
__declspec(dllimport) void scriptUnregister(HMODULE module);
__declspec(dllimport) void scriptWait(DWORD ms);

namespace native
{
    // Natives return each component in its own 8-byte slot.
    struct Vector3
    {
        float x;
        uint32_t paddingX;
        float y;
        uint32_t paddingY;
        float z;
        uint32_t paddingZ;
    };

    inline uint64_t Arg(int32_t value) { return static_cast<uint32_t>(value); }
    inline uint64_t Arg(bool value) { return value ? 1 : 0; }
    inline uint64_t Arg(const char* value) { return reinterpret_cast<uint64_t>(value); }

    template <typename R, typename... Args>
    R Invoke(uint64_t hash, Args... args)
    {
        nativeInit(hash);
        (nativePush64(Arg(args)), ...);
        if constexpr (std::is_void_v<R>)
            nativeCall();
        else
            return *reinterpret_cast<R*>(nativeCall());
    }

    inline bool DoesEntityExist(int32_t entity) { return Invoke<bool>(0x7239B21A38F536BA, entity); }
    inline uint32_t GetEntityModel(int32_t entity) { return Invoke<uint32_t>(0x9F47B058362C84B5, entity); }
    inline float GetEntitySpeed(int32_t entity) { return Invoke<float>(0xD5037BA82E12416F, entity); }
    inline Vector3 GetEntityCoords(int32_t entity) { return Invoke<Vector3>(0x3FEF770D40960D5A, entity, false); }
    inline void StopSound(int32_t soundId) { Invoke<void>(0xA3B0C41BA5CC0BB5, soundId); }
    inline void ReleaseSoundId(int32_t soundId) { Invoke<void>(0x353FC880830B88FA, soundId); }
}
