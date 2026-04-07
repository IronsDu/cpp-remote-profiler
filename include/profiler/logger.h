/// @file logger.h
/// @brief Logger configuration interface
///
/// Log sink and log level configuration is now managed through
/// ProfilerManager instances. Include profiler_manager.h and use:
///   profiler.setLogSink(mySink);
///   profiler.setLogLevel(profiler::LogLevel::Debug);

#pragma once

#include "log_sink.h"

PROFILER_NAMESPACE_BEGIN

// Log configuration is now per-ProfilerManager instance.
// See profiler_manager.h for setLogSink() and setLogLevel() methods.

PROFILER_NAMESPACE_END
