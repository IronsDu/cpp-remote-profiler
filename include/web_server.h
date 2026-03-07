/// @file web_server.h
/// @brief HTTP server setup and route registration

#pragma once

#include "profiler_manager.h"
#include "profiler_version.h"
#include <drogon/drogon.h>

PROFILER_NAMESPACE_BEGIN

/// @brief Register all HTTP route handlers
///
/// This function registers all profiling-related API endpoints and web pages.
/// Call this function after initializing ProfilerManager to enable
/// the web interface for remote profiling.
///
/// @param profiler Reference to the ProfilerManager instance to use
///
/// @par Endpoints registered:
/// - /pprof/profile - CPU profile (pprof compatible)
/// - /pprof/heap - Heap profile (pprof compatible)
/// - /pprof/growth - Heap growth stacks
/// - /pprof/symbol - Symbol lookup
/// - /api/cpu/analyze - CPU flame graph analysis
/// - /api/heap/analyze - Heap flame graph analysis
/// - /api/thread/stacks - Thread stack capture
/// - / - Web UI main page
void registerHttpHandlers(profiler::ProfilerManager& profiler);

PROFILER_NAMESPACE_END
