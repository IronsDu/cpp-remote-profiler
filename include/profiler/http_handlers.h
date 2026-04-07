/// @file http_handlers.h
/// @brief Framework-agnostic HTTP endpoint handlers for the profiler
///
/// Each handler returns a HandlerResponse struct with status code,
/// content type, body, and headers. Users wrap these with their
/// own web framework's request/response types.

#pragma once

#include "profiler_version.h"
#include <map>
#include <string>

PROFILER_NAMESPACE_BEGIN

class ProfilerManager;

/// @brief Framework-agnostic HTTP response
struct HandlerResponse {
    int status = 200;
    std::string content_type = "text/plain";
    std::string body;
    std::map<std::string, std::string> headers;

    static HandlerResponse html(const std::string& content) {
        return {200, "text/html", content, {}};
    }
    static HandlerResponse json(const std::string& content) {
        return {200, "application/json", content, {}};
    }
    static HandlerResponse svg(const std::string& content) {
        return {200, "image/svg+xml", content, {}};
    }
    static HandlerResponse text(const std::string& content) {
        return {200, "text/plain", content, {}};
    }
    static HandlerResponse binary(const std::string& data, const std::string& filename) {
        return {200, "application/octet-stream", data,
                {{"Content-Disposition", "attachment; filename=" + filename}}};
    }
    static HandlerResponse error(int status, const std::string& message) {
        return {status, "application/json", "{\"error\":\"" + message + "\"}", {}};
    }
};

/// @brief Framework-agnostic profiler HTTP endpoint handlers
///
/// Usage example with any framework:
/// @code
///   ProfilerManager profiler;
///   ProfilerHttpHandlers handlers(profiler);
///
///   // In your framework's route handler:
///   auto resp = handlers.handleStatus();
///   // wrap resp.status, resp.content_type, resp.body into your framework's response
/// @endcode
class ProfilerHttpHandlers {
public:
    explicit ProfilerHttpHandlers(ProfilerManager& profiler);

    // --- Status ---
    HandlerResponse handleStatus();

    // --- CPU endpoints ---
    HandlerResponse handleCpuAnalyze(int duration, const std::string& output_type);
    HandlerResponse handleCpuSvgRaw(int duration);
    HandlerResponse handleCpuFlamegraphRaw(int duration);

    // --- Heap endpoints ---
    HandlerResponse handleHeapAnalyze(const std::string& output_type);
    HandlerResponse handleHeapSvgRaw();
    HandlerResponse handleHeapFlamegraphRaw();

    // --- Growth endpoints ---
    HandlerResponse handleGrowthAnalyze(const std::string& output_type);
    HandlerResponse handleGrowthSvgRaw();
    HandlerResponse handleGrowthFlamegraphRaw();

    // --- Convenience: single dispatch by path ---
    /// Dispatch a request to the appropriate handler based on path.
    /// Returns a 404 response if path is not recognized.
    HandlerResponse dispatch(const std::string& method, const std::string& path,
                             const std::map<std::string, std::string>& params = {},
                             const std::string& body = "");

    // --- Standard pprof endpoints ---
    HandlerResponse handlePprofProfile(int seconds);
    HandlerResponse handlePprofHeap();
    HandlerResponse handlePprofGrowth();
    HandlerResponse handlePprofSymbol(const std::string& body);

    // --- Thread stacks ---
    HandlerResponse handleThreadStacks();

private:
    ProfilerManager& profiler_;
};

PROFILER_NAMESPACE_END
