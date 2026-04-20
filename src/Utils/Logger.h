#pragma once
#include <string>
#include <vector>

namespace Logger
{
    enum class LogLevel
    {
        Debug,
        Info,
        Warning,
        Error
    };

    void Log(LogLevel level, const std::string& message);
    void LogDebug(const std::string& message);
    void LogInfo(const std::string& message);
    void LogWarning(const std::string& message);
    void LogError(const std::string& message);
}
