#pragma once

#include <Windows.h>
#include <cstdint>
#include <type_traits>
#include "Logger.h"

namespace SafeMemory
{
    inline const char* AccessMode(bool requireWrite, bool requireExecute)
    {
        if (requireWrite)
            return "write";

        if (requireExecute)
            return "execute";

        return "read";
    }

    inline bool HasReadableProtection(DWORD protect)
    {
        if ((protect & PAGE_GUARD) != 0 || (protect & PAGE_NOACCESS) != 0)
            return false;

        protect &= 0xFF;
        return protect == PAGE_READONLY ||
               protect == PAGE_READWRITE ||
               protect == PAGE_WRITECOPY ||
               protect == PAGE_EXECUTE_READ ||
               protect == PAGE_EXECUTE_READWRITE ||
               protect == PAGE_EXECUTE_WRITECOPY;
    }

    inline bool HasWritableProtection(DWORD protect)
    {
        if ((protect & PAGE_GUARD) != 0 || (protect & PAGE_NOACCESS) != 0)
            return false;

        protect &= 0xFF;
        return protect == PAGE_READWRITE ||
               protect == PAGE_WRITECOPY ||
               protect == PAGE_EXECUTE_READWRITE ||
               protect == PAGE_EXECUTE_WRITECOPY;
    }

    inline bool HasExecutableProtection(DWORD protect)
    {
        if ((protect & PAGE_GUARD) != 0 || (protect & PAGE_NOACCESS) != 0)
            return false;

        protect &= 0xFF;
        return protect == PAGE_EXECUTE ||
               protect == PAGE_EXECUTE_READ ||
               protect == PAGE_EXECUTE_READWRITE ||
               protect == PAGE_EXECUTE_WRITECOPY;
    }

    inline bool IsAccessibleRange(const void* address, size_t size, bool requireWrite, bool requireExecute)
    {
        const char* accessMode = AccessMode(requireWrite, requireExecute);

        if (!address || size == 0)
        {
            Logger::LogMemoryCheckFailure(accessMode, address, size, "null_or_zero_size");
            return false;
        }

        const auto start = reinterpret_cast<uintptr_t>(address);
        if (start < 0x10000)
        {
            Logger::LogMemoryCheckFailure(accessMode, address, size, "low_address");
            return false;
        }

        if (start + size < start)
        {
            Logger::LogMemoryCheckFailure(accessMode, address, size, "range_overflow");
            return false;
        }

        uintptr_t current = start;
        const uintptr_t end = start + size;

        while (current < end)
        {
            MEMORY_BASIC_INFORMATION mbi{};
            if (VirtualQuery(reinterpret_cast<const void*>(current), &mbi, sizeof(mbi)) != sizeof(mbi))
            {
                Logger::LogMemoryCheckFailure(
                    accessMode,
                    address,
                    size,
                    "virtualquery_failed",
                    reinterpret_cast<const void*>(current));
                return false;
            }

            if (mbi.State != MEM_COMMIT)
            {
                Logger::LogMemoryCheckFailure(
                    accessMode,
                    address,
                    size,
                    "not_committed",
                    reinterpret_cast<const void*>(current),
                    mbi.State,
                    mbi.Protect,
                    mbi.BaseAddress,
                    mbi.RegionSize);
                return false;
            }

            const bool protectionOk = requireWrite
                ? HasWritableProtection(mbi.Protect)
                : requireExecute
                    ? HasExecutableProtection(mbi.Protect)
                    : HasReadableProtection(mbi.Protect);

            if (!protectionOk)
            {
                Logger::LogMemoryCheckFailure(
                    accessMode,
                    address,
                    size,
                    "bad_protection",
                    reinterpret_cast<const void*>(current),
                    mbi.State,
                    mbi.Protect,
                    mbi.BaseAddress,
                    mbi.RegionSize);
                return false;
            }

            const uintptr_t regionBase = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
            const uintptr_t regionEnd = regionBase + mbi.RegionSize;
            if (regionEnd <= current)
            {
                Logger::LogMemoryCheckFailure(
                    accessMode,
                    address,
                    size,
                    "bad_region_bounds",
                    reinterpret_cast<const void*>(current),
                    mbi.State,
                    mbi.Protect,
                    mbi.BaseAddress,
                    mbi.RegionSize);
                return false;
            }

            current = regionEnd;
        }

        return true;
    }

    inline bool IsReadable(const void* address, size_t size = sizeof(void*))
    {
        return IsAccessibleRange(address, size, false, false);
    }

    inline bool IsWritable(void* address, size_t size = sizeof(void*))
    {
        return IsAccessibleRange(address, size, true, false);
    }

    inline bool IsExecutable(const void* address)
    {
        return IsAccessibleRange(address, sizeof(void*), false, true);
    }

    template <typename T>
    inline bool IsValidPointer(const T* ptr, size_t size = sizeof(void*))
    {
        return IsReadable(ptr, size);
    }

    template <typename T>
    inline bool TryRead(const T* source, T& out) noexcept
    {
        static_assert(std::is_trivially_copyable_v<T>, "SafeMemory::TryRead requires a trivially copyable type");

        if (!IsReadable(source, sizeof(T)))
            return false;

        __try
        {
            out = *source;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            Logger::LogMemoryCheckFailure("read", source, sizeof(T), "seh_exception");
            return false;
        }
    }

    template <typename T>
    inline bool TryWrite(T* target, const T& value) noexcept
    {
        static_assert(std::is_trivially_copyable_v<T>, "SafeMemory::TryWrite requires a trivially copyable type");

        if (!IsWritable(target, sizeof(T)))
            return false;

        __try
        {
            *target = value;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            Logger::LogMemoryCheckFailure("write", target, sizeof(T), "seh_exception");
            return false;
        }
    }
}