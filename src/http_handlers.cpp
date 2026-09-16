/// @file http_handlers.cpp
/// @brief Framework-agnostic HTTP endpoint handlers implementation

#include "profiler/http_handlers.h"
#include "internal/renderer_output_parsing.h"
#include "internal/result_parsing.h"
#include "profiler_manager.h"
#include <chrono>
#include <fstream>
#include <optional>
#include <sstream>
#include <unistd.h>

PROFILER_NAMESPACE_BEGIN

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static HandlerResponse errorResp(int status, const std::string& message) {
    return HandlerResponse::error(status, message);
}

static int clampDuration(int duration, int lo, int hi) {
    if (duration < lo)
        return lo;
    if (duration > hi)
        return hi;
    return duration;
}

// ---------------------------------------------------------------------------
// ProfilerHttpHandlers
// ---------------------------------------------------------------------------

ProfilerHttpHandlers::ProfilerHttpHandlers(ProfilerManager& profiler) : profiler_(profiler) {}

bool ProfilerHttpHandlers::isCpuProfilerBusy() const {
    // Busy if an HTTP-triggered request holds the session, or if the host opened
    // one through startCPUProfiler() -- in both cases a new request must back off
    // rather than preempt it.
    return profiler_.isCPUProfilingInProgress() || profiler_.isProfilerRunning(profiler::ProfilerType::CPU);
}

bool ProfilerHttpHandlers::isHeapAnalyzerBusy() const {
    return profiler_.isHeapAnalysisInProgress() || profiler_.isProfilerRunning(profiler::ProfilerType::HEAP);
}

HandlerResponse ProfilerHttpHandlers::heapAnalyzerBusyResponse() const {
    return HandlerResponse::error(409, "heap profiling already in use");
}

HandlerResponse ProfilerHttpHandlers::cpuProfilerBusyResponse(bool pprof_style) const {
    if (!pprof_style) {
        // Our own /api/* endpoints: an accurate status code is allowed here.
        return HandlerResponse::error(409, "cpu profiling already in use");
    }

    // Mirrors Go's net/http/pprof, which answers a second concurrent CPU profile
    // request with 500 + text/plain + this exact wording, plus an X-Go-Pprof
    // marker. The marker tells `go tool pprof` that the body is an error message
    // rather than profile data, and no Content-Disposition is set so nothing is
    // treated as a downloadable profile.
    HandlerResponse resp;
    resp.status = 500;
    resp.content_type = "text/plain; charset=utf-8";
    resp.body = "Could not enable CPU profiling: cpu profiling already in use\n";
    resp.headers["X-Go-Pprof"] = "1";
    return resp;
}

// --- Status ---

HandlerResponse ProfilerHttpHandlers::handleStatus() {
    auto cpu = profiler_.getProfilerState(ProfilerType::CPU);
    auto heap = profiler_.getProfilerState(ProfilerType::HEAP);
    auto growth = profiler_.getProfilerState(ProfilerType::HEAP_GROWTH);

    std::ostringstream json;
    json << "{";
    json << "\"cpu\":{\"running\":" << (cpu.is_running ? "true" : "false") << ",\"output_path\":\"" << cpu.output_path
         << "\"" << ",\"duration_ms\":" << cpu.duration << "},";
    json << "\"heap\":{\"running\":" << (heap.is_running ? "true" : "false") << ",\"output_path\":\""
         << heap.output_path << "\"" << ",\"duration_ms\":" << heap.duration << "},";
    json << "\"growth\":{\"running\":" << (growth.is_running ? "true" : "false") << ",\"output_path\":\""
         << growth.output_path << "\"" << ",\"duration_ms\":" << growth.duration << "}";
    json << "}";

    return HandlerResponse::json(json.str());
}

// --- CPU endpoints ---

namespace {

/// Convert flamegraph.pl output into a response, or an error.
///
/// flamegraph.pl answers unusable input with a *valid* SVG whose only content is
/// an "ERROR: ..." message, so a structural check for <?xml or <svg passes and a
/// 200 carrying an error reaches the caller as if it were a graph. All flame
/// graphs funnel through here so the check cannot be forgotten at a call site.
HandlerResponse flameGraphResponse(const std::string& svg) {
    // Parse in one shared place: this logic used to exist twice, and only the copy
    // in profiler_manager.cpp had the substr() underflow that internal::
    // extractFlameGraphError() now guards against.
    if (auto failure = internal::interpretFlameGraphOutput(svg)) {
        if (!internal::looksLikeSvg(svg))
            return errorResp(500, "Failed to generate FlameGraph");
        return errorResp(500, failure->explanation());
    }
    return HandlerResponse::svg(svg);
}

} // namespace

int clampChartDuration(int duration) {
    // Clamp rather than reject: the HTTP layer documents duration as a window
    // that is clamped to this range, and the two profiler backends disagreed --
    // getRawCPUProfile() rejected out-of-range values while
    // getRawHeapProfileSample() clamped them, so the same query behaved
    // differently per endpoint. Clamping here makes one rule for all of them,
    // and keeps a stray value from being reported as a sampling failure.
    if (duration < 1)
        return 1;
    if (duration > 300)
        return 300;
    return duration;
}

ChartRenderer parseChartRenderer(const std::string& value) {
    // "callgraph" is the pprof script's graphviz diagram; "flamegraph" is
    // FlameGraph. Both were previously wrapped in one "output_type" whose values
    // named a tool rather than the resulting picture.
    if (value == "callgraph")
        return ChartRenderer::CallGraph;
    return ChartRenderer::FlameGraph;
}

namespace {

/// Apply the response's delivery mode and filename.
///
/// `inline_display` is the whole difference between "the browser shows the SVG"
/// and "the browser downloads it": omitting Content-Disposition makes the
/// browser render an image/svg+xml body as a document.
HandlerResponse finishChart(HandlerResponse resp, const std::string& title, bool inline_display) {
    if (!inline_display) {
        resp.headers["Content-Disposition"] = "attachment; filename=" + title + ".svg";
    }
    return resp;
}

/// Write @p text to @p path, returning false if that fails.
bool writeTempFile(const std::string& path, const std::string& text) {
    std::ofstream out(path);
    if (!out.is_open())
        return false;
    out << text;
    return out.good();
}

/// Whether a collapsed-stacks file contains real stack data.
///
/// pprof prints "Using local file ..." progress lines; counting those as data
/// would let an empty result through, and flamegraph.pl would then reject it two
/// layers away from the cause.
bool hasCollapsedData(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open())
        return false;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line[0] != '#' && line.rfind("Using ", 0) != 0)
            return true;
    }
    return false;
}

} // namespace

/// Render a gperftools profile as a FlameGraph SVG.
///
/// @param profile_path Path to the profile file to draw
/// @param title Flame graph title
/// @param diagram_name Value for the response's own naming (not shown in the graph)
static HandlerResponse renderFlameGraph(ProfilerManager& profiler, const std::string& profile_path,
                                        const std::string& title, const std::string& diagram_name,
                                        bool inline_display) {
    const std::string collapsed = "/tmp/" + diagram_name + "_collapsed.prof";
    const std::string exe = profiler.getExecutablePath();

    std::ostringstream cmd;
    cmd << "./pprof --collapsed --alloc_space " << exe << " " << profile_path << " > " << collapsed << " 2>/dev/null";

    std::string ignored;
    if (!profiler.executeCommand(cmd.str(), ignored))
        return errorResp(500, "Failed to execute pprof --collapsed command");
    if (!hasCollapsedData(collapsed))
        return errorResp(500, "No stack samples to render (profile is empty). The sample window may have "
                              "been too short or the process idle; increase the duration or sample under "
                              "load.");

    std::string svg;
    std::string fg = "perl ./flamegraph.pl --title=\"" + title + "\" --width=1200 " + collapsed + " 2>/dev/null";
    profiler.executeCommand(fg, svg);

    return finishChart(flameGraphResponse(svg), diagram_name, inline_display);
}

/// Render a gperftools profile as a call-graph SVG via the pprof script.
static HandlerResponse renderCallGraph(ProfilerManager& profiler, const std::string& profile_path,
                                       const std::string& diagram_name, bool inline_display) {
    const std::string exe = profiler.getExecutablePath();
    std::string svg;
    // --alloc_space: the default --inuse_space plots what is live now, which a
    // short collection window rarely has; the window's allocations were already
    // freed. stderr is kept so a failure explains itself.
    profiler.executeCommand("./pprof --svg --alloc_space " + exe + " " + profile_path + " 2>&1", svg);

    auto pos = svg.find("<?xml");
    if (pos == std::string::npos)
        pos = svg.find("<svg");
    if (pos != std::string::npos && pos > 0)
        svg = svg.substr(pos);

    if (svg.empty() || svg.find("<svg") == std::string::npos) {
        if (svg.find("No nodes to print") != std::string::npos)
            return errorResp(500, "The profile has no records to draw. Increase the duration or sample a "
                                  "process that is actively allocating.");
        return errorResp(500, "pprof could not render this profile into an SVG. Output: " + svg);
    }
    return finishChart(HandlerResponse::svg(svg), diagram_name, inline_display);
}

/// Render @p profile_text, which has been written to @p tmp_path, with the chosen renderer.
static HandlerResponse renderChart(ProfilerManager& profiler, const std::string& tmp_path,
                                   const std::string& profile_text, const ChartOptions& options,
                                   const std::string& diagram_name, const std::string& flamegraph_title) {
    if (!writeTempFile(tmp_path, profile_text))
        return errorResp(500, "Failed to write the profile to " + tmp_path);

    if (options.renderer == ChartRenderer::CallGraph)
        return renderCallGraph(profiler, tmp_path, diagram_name, options.inline_display);
    return renderFlameGraph(profiler, tmp_path, flamegraph_title, diagram_name, options.inline_display);
}

/// Turn a profiler-core return value into an error response, or nothing if it is fine.
static std::optional<HandlerResponse> coreError(const std::string& value) {
    if (internal::isJsonError(value))
        return errorResp(500, internal::jsonErrorMessage(value));
    return std::nullopt;
}

HandlerResponse ProfilerHttpHandlers::handleCpuChart(const ChartOptions& options) {
    if (isCpuProfilerBusy())
        return cpuProfilerBusyResponse(false);

    std::string profile = profiler_.getRawCPUProfile(clampChartDuration(options.duration));
    if (auto err = coreError(profile))
        return *err;
    if (profile.empty())
        return errorResp(500, "Failed to collect a CPU profile in the requested window.");

    return renderChart(profiler_, "/tmp/cpu_chart_" + std::to_string(::getpid()) + ".prof", profile, options,
                       "cpu_profile", "CPU Flame Graph");
}

HandlerResponse ProfilerHttpHandlers::handleHeapChart(const ChartOptions& options) {
    if (isHeapAnalyzerBusy())
        return heapAnalyzerBusyResponse();

    std::string sample = profiler_.getRawHeapProfileSample(clampChartDuration(options.duration));
    if (auto err = coreError(sample))
        return *err;
    if (sample.empty())
        return errorResp(500, "Failed to collect a heap sample in the requested window.");

    return renderChart(profiler_, "/tmp/heap_chart_" + std::to_string(::getpid()) + ".prof", sample, options,
                       "heap_profile", "Heap Flame Graph");
}

HandlerResponse ProfilerHttpHandlers::handleHeapSnapshot(const ChartOptions& options, bool as_profile) {
    std::string sample = profiler_.getRawHeapSample();
    if (sample.empty())
        return errorResp(500, "Failed to get heap sample. Make sure TCMALLOC_SAMPLE_PARAMETER is set "
                              "before the process was started.");

    if (as_profile) {
        // The raw profile: what `go tool pprof` consumes.
        auto resp = HandlerResponse::text(sample);
        return finishChart(resp, "heap", options.inline_display);
    }

    return renderChart(profiler_, "/tmp/heap_snapshot_" + std::to_string(::getpid()) + ".prof", sample, options,
                       "heap_snapshot", "Heap Snapshot");
}

HandlerResponse ProfilerHttpHandlers::handleGrowthChart(const ChartOptions& options) {
    std::string growth = profiler_.getRawHeapGrowthStacks();
    if (growth.empty())
        return errorResp(500, "Failed to get heap growth stacks. No heap growth data available.");

    return renderChart(profiler_, "/tmp/growth_chart_" + std::to_string(::getpid()) + ".prof", growth, options,
                       "growth_profile", "Heap Growth Flame Graph");
}

HandlerResponse ProfilerHttpHandlers::handlePprofProfile(int seconds) {
    seconds = clampDuration(seconds, 1, 300);

    // Fail fast rather than wait for, or disturb, a session that is already
    // sampling. That includes one opened through startCPUProfiler(), which
    // belongs to the caller and must not be preempted by an HTTP request.
    if (isCpuProfilerBusy()) {
        return cpuProfilerBusyResponse(true);
    }

    std::string data = profiler_.getRawCPUProfile(seconds);
    if (data.empty()) {
        // The session may have been taken between the check above and the call.
        if (isCpuProfilerBusy()) {
            return cpuProfilerBusyResponse(true);
        }
        return errorResp(500, "Failed to generate CPU profile");
    }

    auto resp = HandlerResponse::binary(data, "profile");
    resp.content_type = "application/octet-stream";
    return resp;
}

namespace {

/// Error response for the /pprof/* family, following Go's serveError(): plain
/// text plus the X-Go-Pprof marker, so `go tool pprof` shows the message instead
/// of trying to parse the body as profile data. Using the JSON helper here would
/// both mislabel the content type and hide the message from the tool.
HandlerResponse pprofErrorResponse(const std::string& message) {
    HandlerResponse resp;
    resp.status = 500;
    resp.content_type = "text/plain; charset=utf-8";
    resp.body = message + "\n";
    resp.headers["X-Go-Pprof"] = "1";
    return resp;
}

} // namespace

HandlerResponse ProfilerHttpHandlers::handlePprofHeap() {
    std::string data = profiler_.getRawHeapSample();
    if (data.empty()) {
        return pprofErrorResponse("Could not read heap sample: heap sampling is off or produced no "
                                  "data. Set TCMALLOC_SAMPLE_PARAMETER before starting the process.");
    }

    HandlerResponse resp;
    resp.status = 200;
    resp.content_type = "text/plain";
    resp.body = data;
    resp.headers["Content-Disposition"] = "attachment; filename=heap";
    return resp;
}

HandlerResponse ProfilerHttpHandlers::handlePprofGrowth() {
    std::string data = profiler_.getRawHeapGrowthStacks();
    if (data.empty()) {
        return pprofErrorResponse("Could not read heap growth stacks: no heap growth data available.");
    }

    HandlerResponse resp;
    resp.status = 200;
    resp.content_type = "text/plain";
    resp.body = data;
    resp.headers["Content-Disposition"] = "attachment; filename=growth";
    return resp;
}

HandlerResponse ProfilerHttpHandlers::handlePprofSymbol(const std::string& body) {
    // Go pprof (symbolz protocol) sends addresses separated by '+'
    // and expects tab-separated response: "0xaddr\tsymbol_name\n"
    // Also support newline-separated addresses for backward compatibility.

    // Splitting lives in internal::parseSymbolRequest() so it is unit tested and
    // fuzzed rather than only exercised through a live request.
    std::vector<std::string> addresses = internal::parseSymbolRequest(body);

    std::ostringstream result;
    for (const auto& address : addresses) {
        std::string addr_str = address;
        if (addr_str.size() > 2 && addr_str[0] == '0' && addr_str[1] == 'x') {
            addr_str = addr_str.substr(2);
        }

        try {
            uintptr_t addr = std::stoull(addr_str, nullptr, 16);
            std::string symbol = profiler_.resolveSymbolWithBackward(reinterpret_cast<void*>(addr));
            result << address << "\t" << symbol << "\n";
        } catch (...) {
            result << address << "\t" << address << "\n";
        }
    }

    return HandlerResponse::text(result.str());
}

// --- Thread stacks ---

HandlerResponse ProfilerHttpHandlers::handleThreadStacks() {
    std::string stacks = profiler_.getThreadCallStacks();
    if (stacks.empty()) {
        return errorResp(500, "Failed to get thread call stacks");
    }
    return HandlerResponse::text(stacks);
}

PROFILER_NAMESPACE_END
