/// @file logger.h
/// @brief Logger configuration interface

#pragma once

#include "log_sink.h"
#include <memory>

PROFILER_NAMESPACE_BEGIN

/// @brief Set a custom log sink for the profiler
/// @param sink Shared pointer to a LogSink implementation
///
/// When a custom sink is set, all profiler log messages will be
/// forwarded to this sink instead of the default output.
///
/// Pass nullptr to revert to the default sink.
///
/// @example
/// @code
/// // Use custom sink
/// profiler::setSink(std::make_shared<MyLogSink>());
///
/// // Revert to default
/// profiler::setSink(nullptr);
/// @endcode
void setSink(std::shared_ptr<LogSink> sink);

/// @brief Set the minimum log level
/// @param level Minimum level for messages to be output
///
/// Messages with a level lower than the specified level will be suppressed.
/// Default level is LogLevel::Info.
///
/// @example
/// @code
/// // Enable debug logging
/// profiler::setLogLevel(profiler::LogLevel::Debug);
///
/// // Suppress all but errors
/// profiler::setLogLevel(profiler::LogLevel::Error);
/// @endcode
void setLogLevel(LogLevel level);

PROFILER_NAMESPACE_END
