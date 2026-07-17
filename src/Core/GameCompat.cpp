#include "GameCompat.h"

#include <Windows.h>
#include <cstdint>

// Offsets SDK — copie locale des RVA hardcodées de Basic.hpp. On ne #include PAS
// le SDK ici : ce module doit rester compilable et exécutable même quand le SDK
// pointe sur du vide. Si ces valeurs changent dans Basic.hpp, mettre à jour ici.
namespace
{
    constexpr uintptr_t kOff_GObjects     = 0x06C78FD0;
    constexpr uintptr_t kOff_ProcessEvent = 0x01A2D530;
    constexpr uintptr_t kOff_AppendString = 0x0183E8B0;

    // TUObjectArray (FUObjectArray) — layout confirmé :
    //   +0x00  FUObjectItem* Objects   (chunk table)
    //   +0x10  int32 MaxElements
    //   +0x14  int32 NumElements
    //   +0x1C  int32 NumChunks
    // FUObjectItem : +0x00 UObject* Object.  UObject : +0x00 void** vtable.
    constexpr uintptr_t kUOA_MaxElements = 0x10;
    constexpr uintptr_t kUOA_NumElements = 0x14;
    constexpr uintptr_t kUOA_Objects     = 0x00;
    constexpr size_t    kElementsPerChunk = 0x10000;
    constexpr size_t    kFUObjectItemSize = 0x18;

    // Filtre SEH local. On n'attrape QUE les fautes mémoire ; tout le reste
    // remonte. Identique en esprit à HandleAccessViolation de DllEntry.cpp.
    int MemFaultFilter(unsigned int code)
    {
        return (code == EXCEPTION_ACCESS_VIOLATION ||
                code == EXCEPTION_IN_PAGE_ERROR)
                   ? EXCEPTION_EXECUTE_HANDLER
                   : EXCEPTION_CONTINUE_SEARCH;
    }

    struct ModuleRange
    {
        uintptr_t base;
        uintptr_t end;   // base + SizeOfImage (exclusif)
        bool      valid;
    };

    // Range du .exe principal, lu depuis les en-têtes PE déjà mappés. Aucune
    // lecture de mémoire de jeu risquée (les headers sont toujours valides tant
    // que le module est chargé), mais on garde le SEH par principe.
    ModuleRange MainModuleRange()
    {
        ModuleRange r{ 0, 0, false };

        HMODULE hMod = GetModuleHandleW(nullptr);
        if (!hMod)
            return r;

        __try
        {
            auto base = reinterpret_cast<uintptr_t>(hMod);
            auto dos  = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
            if (dos->e_magic != IMAGE_DOS_SIGNATURE)
                return r;

            auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
            if (nt->Signature != IMAGE_NT_SIGNATURE)
                return r;

            r.base  = base;
            r.end   = base + nt->OptionalHeader.SizeOfImage;
            r.valid = true;
        }
        __except (MemFaultFilter(GetExceptionCode()))
        {
            r.valid = false;
        }

        return r;
    }

    // VirtualQuery + vérifie que [addr] est committed, lisible, et (si execCheck)
    // dans une page exécutable dont le 1er octet est non nul.
    bool ProbeRegion(uintptr_t addr, bool execCheck)
    {
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi)) != sizeof(mbi))
            return false;

        if (mbi.State != MEM_COMMIT)
            return false;

        DWORD prot = mbi.Protect;
        if (prot & (PAGE_NOACCESS | PAGE_GUARD))
            return false;

        if (execCheck)
        {
            const DWORD execMask = PAGE_EXECUTE | PAGE_EXECUTE_READ |
                                   PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
            if (!(prot & execMask))
                return false;

            // 1er octet du code non nul (une page de code n'ouvre pas sur 0x00).
            bool ok = false;
            __try
            {
                ok = (*reinterpret_cast<volatile const uint8_t*>(addr) != 0);
            }
            __except (MemFaultFilter(GetExceptionCode()))
            {
                ok = false;
            }
            if (!ok)
                return false;
        }
        else
        {
            const DWORD readMask = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                                   PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                                   PAGE_EXECUTE_WRITECOPY;
            if (!(prot & readMask))
                return false;
        }

        return true;
    }

    // Cœur du probe GObjects, entièrement sous SEH. Retourne false sur toute
    // faute ou toute valeur incohérente.
    bool ProbeGObjects(const ModuleRange& mod)
    {
        __try
        {
            uintptr_t gobj = mod.base + kOff_GObjects;

            // La structure elle-même doit être lisible.
            if (!ProbeRegion(gobj, false))
                return false;

            int32_t maxEl = *reinterpret_cast<volatile const int32_t*>(gobj + kUOA_MaxElements);
            int32_t numEl = *reinterpret_cast<volatile const int32_t*>(gobj + kUOA_NumElements);

            // Bornes plausibles : 0 < Num <= Max, Max raisonnable (< ~50M).
            if (numEl <= 0 || maxEl <= 0 || numEl > maxEl || maxEl > 50'000'000)
                return false;

            auto objects = *reinterpret_cast<void* const volatile*>(gobj + kUOA_Objects);
            if (!objects)
                return false;
            if (!ProbeRegion(reinterpret_cast<uintptr_t>(objects), false))
                return false;

            // Chunked array : Objects[0] = 1er chunk (FUObjectItem*).
            auto chunk0 = *reinterpret_cast<void* const volatile*>(objects);
            if (!chunk0)
                return false;
            if (!ProbeRegion(reinterpret_cast<uintptr_t>(chunk0), false))
                return false;

            // 1er FUObjectItem -> UObject* (à +0x00).
            auto uobj = *reinterpret_cast<void* const volatile*>(
                reinterpret_cast<uintptr_t>(chunk0) + kUOA_Objects);
            if (!uobj)
                return false;
            if (!ProbeRegion(reinterpret_cast<uintptr_t>(uobj), false))
                return false;

            // vtable de l'UObject : doit pointer DANS le module principal.
            auto vtable = *reinterpret_cast<uintptr_t const volatile*>(uobj);
            if (vtable < mod.base || vtable >= mod.end)
                return false;

            (void)kElementsPerChunk;
            (void)kFUObjectItemSize;
            return true;
        }
        __except (MemFaultFilter(GetExceptionCode()))
        {
            return false;
        }
    }
}

namespace GameCompat
{
    uint32_t GameTimeDateStamp()
    {
        HMODULE hMod = GetModuleHandleW(nullptr);
        if (!hMod)
            return 0;

        uint32_t tds = 0;
        __try
        {
            auto base = reinterpret_cast<uintptr_t>(hMod);
            auto dos  = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
            if (dos->e_magic != IMAGE_DOS_SIGNATURE)
                return 0;

            auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
            if (nt->Signature != IMAGE_NT_SIGNATURE)
                return 0;

            tds = nt->FileHeader.TimeDateStamp;
        }
        __except (MemFaultFilter(GetExceptionCode()))
        {
            tds = 0;
        }

        return tds;
    }

    bool IsGameCompatible()
    {
        // Override de test : force l'incompatibilité pour simuler un game-update
        // sans réellement patcher le jeu.
#ifdef SELFUPDATE_FORCE_INCOMPAT
        return false;
#endif

        ModuleRange mod = MainModuleRange();
        if (!mod.valid)
            return false;

        // 1) GObjects cohérent (array + 1er objet + vtable dans le module).
        if (!ProbeGObjects(mod))
            return false;

        // 2) ProcessEvent & AppendString pointent sur du code exécutable non nul.
        if (!ProbeRegion(mod.base + kOff_ProcessEvent, true))
            return false;
        if (!ProbeRegion(mod.base + kOff_AppendString, true))
            return false;

        return true;
    }
}
