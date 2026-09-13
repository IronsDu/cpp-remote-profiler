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
        return {200, "application/octet-stream", data, {{"Content-Disposition", "attachment; filename=" + filename}}};
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
    /// @brief Whether a CPU profiling session is already claimed
    ///
    /// CPU profiling is exclusive: gperftools keeps one process-global sampling
    /// session, so at most one request may sample at a time.
    bool isCpuProfilerBusy() const;

    /// @brief The response to use when @ref isCpuProfilerBusy is true
    ///
    /// Provided so a web adapter can refuse a request on the event-loop thread —
    /// before it is queued for execution — using the same wording and status the
    /// handlers themselves produce. Calling it only makes sense while
    /// @ref isCpuProfilerBusy is true.
    ///
    /// `pprof_style` selects the shape:
    ///  - true  — the shape Go's net/http/pprof uses on /pprof/profile: HTTP 500,
    ///            `text/plain`, plus an `X-Go-Pprof: 1` marker so `go tool pprof`
    ///            reports the message instead of parsing it as profile data
    ///  - false — HTTP 409 with a JSON error, for the custom /api/* endpoints
    HandlerResponse cpuProfilerBusyResponse(bool pprof_style) const;

    /// @brief Whether a heap analysis is already running
    ///
    /// Heap analysis reconfigures the single process-global heap profiler, so
    /// concurrent calls are exclusive for the same reason CPU profiling is.
    bool isHeapAnalyzerBusy() const;

    /// @brief The response to use when @ref isHeapAnalyzerBusy is true
    ///
    /// Mirrors @ref cpuProfilerBusyResponse but for heap analysis, which has no
    /// Go pprof equivalent to match: HTTP 409 with a JSON error.
    HandlerResponse heapAnalyzerBusyResponse() const;

    HandlerResponse handleCpuAnalyze(int duration, const std::string& output_type);
    HandlerResponse handleCpuSvgRaw(int duration);
    HandlerResponse handleCpuFlamegraphRaw(int duration);

    // --- Heap endpoints ---
    /// @param duration Collection window in seconds (clamped 1..300). Not a
    ///        sampling rate -- that is fixed by TCMALLOC_SAMPLE_PARAMETER at
    ///        process start -- but a longer window covers more allocations.
    HandlerResponse handleHeapAnalyze(int duration, const std::string& output_type);

    /// @brief Render the current heap sample with the pprof script
    /// @param duration Collection window in seconds (clamped 1..300)
    HandlerResponse handleHeapSvgRaw(int duration);

    /// @brief Render the current heap sample as a FlameGraph
    /// @param duration Collection window in seconds (clamped 1..300)
    HandlerResponse handleHeapFlamegraphRaw(int duration);

    // --- Growth endpoints ---
    HandlerResponse handleGrowthAnalyze(const std::string& output_type);
    HandlerResponse handleGrowthSvgRaw();
    HandlerResponse handleGrowthFlamegraphRaw();

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
