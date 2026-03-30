/// @file log_manager.h
/// @brief Internal log manager implementation

#pragma once

#include "profiler/log_sink.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <string>

PROFILER_NAMESPACE_BEGIN

namespace internal {

/// @class LogManager
/// @brief Internal singleton for managing log sink and level
class LogManager {
public:
    static LogManager& instance();

    /// @brief Set custom log sink
    void setSink(std::shared_ptr<LogSink> sink);

    /// @brief Get current log sink (never nullptr)
    LogSink* sink();

    /// @brief Set minimum log level
    void setLogLevel(LogLevel level);

    /// @brief Get current minimum log level
    LogLevel logLevel() const;

    /// @brief Check if a log level should be output
    bool shouldLog(LogLevel level) const;

private:
    LogManager();
    ~LogManager() = default;
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;

    std::shared_ptr<LogSink> sink_;
    std::atomic<LogLevel> level_{LogLevel::Info};
    mutable std::mutex mutex_;
};

/// @brief Internal function to write a log message (fmt-style formatting)
void logMessage(LogLevel level, const char* file, int line, const char* function, std::string&& message);

} // namespace internal

PROFILER_NAMESPACE_END
