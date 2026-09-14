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

/// @brief Which graphical form a chart endpoint should produce
///
/// These are two genuinely different representations, not two styles of one:
/// a flame graph is a stack of frames, a call graph is a node/edge diagram.
enum class ChartRenderer {
    /// Stacked frames (FlameGraph). Self-contained SVG; the classic choice.
    FlameGraph,
    /// Node/edge call graph produced by the pprof script via graphviz dot.
    /// Carries better symbolisation (it resolves inline frames).
    CallGraph,
};

/// @brief Options shared by the /api/pprof/{cpu,heap,growth} analysis endpoints
struct ChartOptions {
    ChartRenderer renderer = ChartRenderer::FlameGraph;
    /// Collection window in seconds, clamped to 1..300. Note this is how long
    /// allocations/samples are *collected*, not a sampling rate.
    int duration = 10;
    /// true  -> omit Content-Disposition so a browser renders the SVG in place
    /// false -> send `Content-Disposition: attachment` to force a download
    bool inline_display = true;
};

/// @brief Parse the `renderer` query value; unknown values fall back to FlameGraph
ChartRenderer parseChartRenderer(const std::string& value);

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

    // --- Exclusivity probes ---
    //
    // Both are exposed so a web adapter can refuse a request on the event-loop
    // thread — before queueing it — using the same wording the handlers produce.
    /// @brief Whether a CPU sampling session is claimed (by a request or the host)
    bool isCpuProfilerBusy() const;

    /// @brief Whether a heap analysis is running, or the host opened a heap session
    bool isHeapAnalyzerBusy() const;

    /// @brief The response for a CPU request that arrived while the profiler is busy
    /// @param pprof_style true -> Go's /pprof/profile shape (500 + text/plain +
    ///        `X-Go-Pprof: 1`); false -> 409 with a JSON error for /api/* routes
    HandlerResponse cpuProfilerBusyResponse(bool pprof_style) const;

    /// @brief The response for a heap analysis that arrived while one is running
    HandlerResponse heapAnalyzerBusyResponse() const;

    // --- Analysis endpoints: one entry point per profiler ---
    /// @brief Sample CPU for the requested window and render a chart
    ///
    /// Exclusive: refuses while another sampling session is active (409) rather
    /// than queueing or preempting it.
    HandlerResponse handleCpuChart(const ChartOptions& options);

    /// @brief Sample heap for the requested window and render a chart
    /// @note Exclusive in the same way CPU is.
    HandlerResponse handleHeapChart(const ChartOptions& options);

    /// @brief Render the current heap snapshot (state-based, not a window)
    ///
    /// Answers "what is in the heap now" rather than "what was allocated during
    /// the window", so it needs TCMALLOC_SAMPLE_PARAMETER to have been set before
    /// the process started. `renderer` is ignored when @p as_profile is true.
    ///
    /// @param options Chart options (renderer used only when @p as_profile is false)
    /// @param as_profile Return the raw profile text instead of an SVG
    HandlerResponse handleHeapSnapshot(const ChartOptions& options, bool as_profile);

    /// @brief Sample heap growth and render a chart
    HandlerResponse handleGrowthChart(const ChartOptions& options);

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
