// Verifies, outside the game, that an ELS.asi matches src/ElsLayout.h and that every function in
// it can be hooked and unhooked cleanly. ELS.asi is mapped without running any of its code.
//
// Usage: hooktest <path to ELS.asi>
#include <windows.h>
#include <cstdio>
#include <cstring>

#include "../src/ElsLayout.h"
#include "MinHook.h"

namespace
{
    uint64_t Replacement(uint64_t, uint64_t, uint64_t, uint64_t) { return 0; }

    template <size_t N>
    bool CheckSignatures(const uint8_t* base, const els::Signature (&table)[N])
    {
        bool ok = true;
        for (const els::Signature& s : table)
        {
            const bool match = memcmp(base + s.rva, s.bytes, els::kSignatureLength) == 0;
            printf("  %-40s %s\n", s.name, match ? "ok" : "MISMATCH");
            ok &= match;
        }
        return ok;
    }
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        printf("usage: hooktest <path to ELS.asi>\n");
        return 2;
    }
    HMODULE module = LoadLibraryExA(argv[1], nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (module == nullptr)
    {
        printf("Could not map %s (error %lu)\n", argv[1], GetLastError());
        return 2;
    }
    auto* base = reinterpret_cast<uint8_t*>(module);

    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    bool ok = nt->FileHeader.TimeDateStamp == els::kTimestamp && nt->OptionalHeader.SizeOfImage == els::kImageSize;
    printf("Timestamp %08lX, image size 0x%lX: %s\n", nt->FileHeader.TimeDateStamp, nt->OptionalHeader.SizeOfImage,
           ok ? "V 1.05" : "not V 1.05");

    printf("Signatures:\n");
    ok &= CheckSignatures(base, els::kFunctions);
    ok &= CheckSignatures(base, els::kSites);

    constexpr int count = static_cast<int>(els::Function::Count);
    uint8_t original[count][16];
    void* trampolines[count] = {};
    if (MH_Initialize() != MH_OK)
    {
        printf("MinHook failed to initialise\n");
        return 1;
    }
    for (int i = 0; i < count; ++i)
    {
        uint8_t* target = base + els::kFunctions[i].rva;
        memcpy(original[i], target, sizeof(original[i]));
        const MH_STATUS status = MH_CreateHook(target, &Replacement, &trampolines[i]);
        if (status != MH_OK)
        {
            printf("Could not hook %s: %s\n", els::kFunctions[i].name, MH_StatusToString(status));
            ok = false;
        }
    }

    MH_STATUS status = MH_EnableHook(MH_ALL_HOOKS);
    printf("Enable %d hooks: %s\n", count, MH_StatusToString(status));
    ok &= status == MH_OK;
    for (int i = 0; i < count; ++i)
    {
        if (base[els::kFunctions[i].rva] == 0xE9 && trampolines[i] != nullptr) continue;
        printf("%s was not hooked\n", els::kFunctions[i].name);
        ok = false;
    }

    status = MH_DisableHook(MH_ALL_HOOKS);
    ok &= status == MH_OK;
    for (int i = 0; i < count; ++i)
    {
        if (memcmp(original[i], base + els::kFunctions[i].rva, sizeof(original[i])) == 0) continue;
        printf("%s was not restored\n", els::kFunctions[i].name);
        ok = false;
    }
    MH_Uninitialize();

    printf(ok ? "PASSED\n" : "FAILED\n");
    return ok ? 0 : 1;
}
