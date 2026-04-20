#include "Logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace Logger
{
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

    void Log(LogLevel level, const std::string& message)
    {
        // All logging disabled for release build
        (void)level;
        (void)message;
    }

    void LogDebug(const std::string& message) { (void)message; }
    void LogInfo(const std::string& message) { (void)message; }
    void LogWarning(const std::string& message) { (void)message; }
    void LogError(const std::string& message) { (void)message; }
}
