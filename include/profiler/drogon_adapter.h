/// @file drogon_adapter.h
/// @brief Drogon integration: registers profiler routes via ProfilerHttpHandlers

#pragma once

#include "profiler_manager.h"

PROFILER_NAMESPACE_BEGIN

/// @brief Register all HTTP route handlers with the global Drogon app
///
/// This is a convenience function that registers all profiling-related
/// API endpoints and web pages using Drogon. It internally creates a
/// ProfilerHttpHandlers and wraps each handler with Drogon adapters.
///
/// After calling this, start the Drogon server with:
///   drogon::app().addListener(host, port).run();
///
/// @param profiler Reference to the ProfilerManager instance to use
void registerDrogonHandlers(profiler::ProfilerManager& profiler);

PROFILER_NAMESPACE_END
