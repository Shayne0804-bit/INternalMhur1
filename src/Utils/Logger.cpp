#include "Logger.h"
#include <Windows.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

namespace Logger
{
    namespace
    {
        constexpr bool kEnableFileLogging = false;
        constexpr const char* kLogDir = "C:\\Temp";
        constexpr const char* kLogPath = "C:\\Temp\\rugir_internal_debug.log";
        constexpr unsigned long kMaxMemoryFailureLogs = 2000;

        std::atomic_bool g_FileLoggingInitialized{ false };
        std::atomic<unsigned long> g_MemoryFailureLogs{ 0 };
        std::mutex g_LogMutex;

        const char* AccessViolationOperation(ULONG_PTR operation)
        {
            switch (operation)
            {
            case 0: return "read";
            case 1: return "write";
            case 8: return "execute";
            default: return "unknown";
            }
        }

        void FormatTimestamp(char* buffer, size_t bufferSize) noexcept
        {
            if (!buffer || bufferSize == 0)
                return;

            SYSTEMTIME time{};
            GetLocalTime(&time);
            std::snprintf(
                buffer,
                bufferSize,
                "%04hu-%02hu-%02hu %02hu:%02hu:%02hu.%03hu",
                time.wYear,
                time.wMonth,
                time.wDay,
                time.wHour,
                time.wMinute,
                time.wSecond,
                time.wMilliseconds);
        }

        void WriteLineNoThrow(const char* line) noexcept
        {
            if (!kEnableFileLogging)
                return;

            if (!line)
                return;

            CreateDirectoryA(kLogDir, nullptr);

            HANDLE file = CreateFileA(
                kLogPath,
                FILE_APPEND_DATA,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr,
                OPEN_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                nullptr);

            if (file == INVALID_HANDLE_VALUE)
                return;

            DWORD bytesWritten = 0;
            const DWORD length = static_cast<DWORD>(std::strlen(line));
            if (length > 0)
                WriteFile(file, line, length, &bytesWritten, nullptr);

            static constexpr char newline[] = "\r\n";
            WriteFile(file, newline, sizeof(newline) - 1, &bytesWritten, nullptr);
            CloseHandle(file);
        }

        void WriteTimestampedLineNoThrow(const char* level, const char* message) noexcept
        {
            char timestamp[64] = {};
            char line[2048] = {};
            FormatTimestamp(timestamp, sizeof(timestamp));

            std::snprintf(
                line,
                sizeof(line),
                "[%s][T:%lu][%s] %s",
                timestamp,
                GetCurrentThreadId(),
                level ? level : "INFO",
                message ? message : "");

            WriteLineNoThrow(line);
        }
    }

    const char* GetLogFilePath()
    {
        return kLogPath;
    }

    void InitializeFileLogging() noexcept
    {
        if (!kEnableFileLogging)
            return;

        bool expected = false;
        if (!g_FileLoggingInitialized.compare_exchange_strong(expected, true))
            return;

        CreateDirectoryA(kLogDir, nullptr);

        HANDLE file = CreateFileA(
            kLogPath,
            GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        if (file != INVALID_HANDLE_VALUE)
            CloseHandle(file);

        char line[512] = {};
        std::snprintf(
            line,
            sizeof(line),
            "===== RUGIR_INTERNAL debug log start pid=%lu moduleBase=0x%p =====",
            GetCurrentProcessId(),
            GetModuleHandleW(nullptr));
        WriteTimestampedLineNoThrow("INFO", line);
    }

    std::string GetTimestamp()
    {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);

        std::stringstream ss;
        struct tm timeinfo;
        localtime_s(&timeinfo, &time);
        ss << std::put_time(&timeinfo, "%H:%M:%S");
        return ss.str();
    }

    std::string LevelToString(LogLevel level)
    {
        switch (level)
        {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO";
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error: return "ERROR";
        default: return "UNKNOWN";
        }
    }

    void LogRaw(const char* message) noexcept
    {
        InitializeFileLogging();
        std::lock_guard<std::mutex> lock(g_LogMutex);
        WriteTimestampedLineNoThrow("RAW", message);
    }

    void LogSEHException(const char* context, _EXCEPTION_POINTERS* exceptionInfo) noexcept
    {
        if (!kEnableFileLogging)
            return;

        InitializeFileLogging();

        DWORD code = 0;
        void* exceptionAddress = nullptr;
        ULONG_PTR operation = static_cast<ULONG_PTR>(-1);
        void* faultAddress = nullptr;

        if (exceptionInfo && exceptionInfo->ExceptionRecord)
        {
            EXCEPTION_RECORD* record = exceptionInfo->ExceptionRecord;
            code = record->ExceptionCode;
            exceptionAddress = record->ExceptionAddress;

            if ((code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR) &&
                record->NumberParameters >= 2)
            {
                operation = record->ExceptionInformation[0];
                faultAddress = reinterpret_cast<void*>(record->ExceptionInformation[1]);
            }
        }

        char line[1024] = {};
        std::snprintf(
            line,
            sizeof(line),
            "[SEH] context=%s code=0x%08lX exceptionAddress=0x%p operation=%s faultAddress=0x%p",
            context ? context : "unknown",
            code,
            exceptionAddress,
            AccessViolationOperation(operation),
            faultAddress);

        WriteTimestampedLineNoThrow("ERROR", line);
    }

    void LogMemoryCheckFailure(
        const char* operation,
        const void* address,
        size_t size,
        const char* reason,
        const void* queryAddress,
        unsigned long state,
        unsigned long protect,
        const void* regionBase,
        size_t regionSize) noexcept
    {
        const unsigned long count = g_MemoryFailureLogs.fetch_add(1, std::memory_order_relaxed);
        if (count >= kMaxMemoryFailureLogs)
        {
            if (count == kMaxMemoryFailureLogs)
                WriteTimestampedLineNoThrow("WARN", "[SafeMemory] memory failure log limit reached");

            return;
        }

        InitializeFileLogging();

        char line[1024] = {};
        std::snprintf(
            line,
            sizeof(line),
            "[SafeMemory] %s rejected address=0x%p size=0x%zX reason=%s query=0x%p state=0x%lX protect=0x%lX regionBase=0x%p regionSize=0x%zX",
            operation ? operation : "access",
            address,
            size,
            reason ? reason : "unknown",
            queryAddress,
            state,
            protect,
            regionBase,
            regionSize);

        WriteTimestampedLineNoThrow("WARN", line);
    }

    void Log(LogLevel level, const std::string& message)
    {
        InitializeFileLogging();

        std::lock_guard<std::mutex> lock(g_LogMutex);
        WriteTimestampedLineNoThrow(LevelToString(level).c_str(), message.c_str());
    }

    void LogDebug(const std::string& message) { Log(LogLevel::Debug, message); }
    void LogInfo(const std::string& message) { Log(LogLevel::Info, message); }
    void LogWarning(const std::string& message) { Log(LogLevel::Warning, message); }
    void LogError(const std::string& message) { Log(LogLevel::Error, message); }
}
