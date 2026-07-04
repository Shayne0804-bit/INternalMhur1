#pragma once
#include <cstddef>
#include <string>

struct _EXCEPTION_POINTERS;

namespace Logger
{
    enum class LogLevel
    {
        Debug,
        Info,
        Warning,
        Error
    };

    const char* GetLogFilePath();
    void InitializeFileLogging() noexcept;
    void LogRaw(const char* message) noexcept;
    void LogSEHException(const char* context, _EXCEPTION_POINTERS* exceptionInfo) noexcept;
    void LogMemoryCheckFailure(
        const char* operation,
        const void* address,
        size_t size,
        const char* reason,
        const void* queryAddress = nullptr,
        unsigned long state = 0,
        unsigned long protect = 0,
        const void* regionBase = nullptr,
        size_t regionSize = 0) noexcept;

    void Log(LogLevel level, const std::string& message);
    void LogDebug(const std::string& message);
    void LogInfo(const std::string& message);
    void LogWarning(const std::string& message);
    void LogError(const std::string& message);
}