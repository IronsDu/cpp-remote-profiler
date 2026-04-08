#pragma once

#include "profiler/log_sink.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <string>

PROFILER_NAMESPACE_BEGIN

namespace internal {

/// Non-singleton log manager, owned by ProfilerManager instances
class LogManager {
public:
    LogManager();
    ~LogManager() = default;
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;

    /// Set custom log sink
    void setSink(std::shared_ptr<LogSink> sink);

    /// Get current log sink (never nullptr)
    LogSink* sink();

    /// Set minimum log level
    void setLogLevel(LogLevel level);

    /// Get current minimum log level
    LogLevel logLevel() const;

    /// Check if a log level should be output
    bool shouldLog(LogLevel level) const;

private:
    std::shared_ptr<LogSink> sink_;
    std::atomic<LogLevel> level_{LogLevel::Info};
    mutable std::mutex mutex_;
};

} // namespace internal

PROFILER_NAMESPACE_END
