/// @file default_log_sink.h
/// @brief Default log sink implementation using spdlog

#pragma once

#include "profiler/log_sink.h"
#include <memory>
#include <mutex>

PROFILER_NAMESPACE_BEGIN

namespace internal {

/// @class DefaultLogSink
/// @brief Default log sink that outputs to stderr using spdlog
class DefaultLogSink : public LogSink {
public:
    DefaultLogSink();
    ~DefaultLogSink() override;

    void log(LogLevel level,
             const char* file,
             int line,
             const char* function,
             const char* message) override;

    void flush() override;

    /// @brief Set minimum log level for this sink
    void setLogLevel(LogLevel level);

private:
    std::mutex mutex_;
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace internal

PROFILER_NAMESPACE_END
