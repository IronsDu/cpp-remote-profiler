/// @file log_sink.h
/// @brief Log sink interface for custom logging implementations

#pragma once

#include "profiler_version.h"
#include <string>

PROFILER_NAMESPACE_BEGIN

/// @enum LogLevel
/// @brief Log severity levels
enum class LogLevel {
    Trace,    ///< Detailed debug information
    Debug,    ///< Debug information
    Info,     ///< General information
    Warning,  ///< Warning messages
    Error,    ///< Error messages
    Fatal     ///< Fatal errors (typically terminates program)
};

/// @class LogSink
/// @brief Abstract interface for custom log output destinations
///
/// Users can inherit from this class to implement custom logging behavior,
/// such as integrating with their application's logging system.
///
/// @example
/// @code
/// class MyLogSink : public profiler::LogSink {
/// public:
///     void log(profiler::LogLevel level, const char* file, int line,
///              const char* function, const char* message) override {
///         // Forward to your application's logger
///         MyAppLogger::log(level, file, line, message);
///     }
/// };
///
/// // Set custom sink
/// profiler::setSink(std::make_shared<MyLogSink>());
/// @endcode
class LogSink {
public:
    virtual ~LogSink() = default;

    /// @brief Write a log message
    /// @param level Log severity level
    /// @param file Source file name (may be nullptr)
    /// @param line Source line number
    /// @param function Function name (may be nullptr)
    /// @param message Formatted log message
    virtual void log(LogLevel level,
                     const char* file,
                     int line,
                     const char* function,
                     const char* message) = 0;

    /// @brief Flush any buffered log output
    /// @note Default implementation does nothing
    virtual void flush() {}
};

PROFILER_NAMESPACE_END
