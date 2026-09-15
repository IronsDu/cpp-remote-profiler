/// @file fuzz_http_params.cpp
/// @brief libFuzzer target for the analysis endpoints' query-parameter handling
///
/// `duration` and `seconds` arrive as strings from the client and are converted
/// with std::stoi(). That conversion and the clamping that follows are small, but
/// this exact area produced a real defect: nothing clamped `duration`, so
/// out-of-range values reached the profiler backends, which disagreed about them
/// (one rejected, one clamped), and a negative value was passed to sleep().
///
/// The invariants below are the ones the HTTP layer promises in its documentation:
/// every accepted duration lands in 1..300, and no input can make the parsers
/// throw or produce a value outside that range.

#include "profiler/http_handlers.h"
#include <cstddef>
#include <cstdint>
#include <string>

namespace {

/// The conversion the Drogon adapter performs (`chartOptionsFrom`), reproduced so
/// the fuzzer drives the same code path the server does.
int parseDurationLikeTheAdapter(const std::string& value, int fallback) {
    if (value.empty())
        return fallback;
    try {
        return std::stoi(value);
    } catch (...) {
        return fallback;
    }
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    const std::string input(reinterpret_cast<const char*>(data), size);

    using profiler::v0_1_0::ChartOptions;
    using profiler::v0_1_0::ChartRenderer;
    using profiler::v0_1_0::clampChartDuration;
    using profiler::v0_1_0::parseChartRenderer;

    // 1. The clamping contract the README states: 1..300, for any input at all.
    const int raw = parseDurationLikeTheAdapter(input, 10);
    const int clamped = clampChartDuration(raw);
    if (clamped < 1 || clamped > 300)
        __builtin_trap();

    // Clamping is idempotent -- applying it twice must not move a value.
    if (clampChartDuration(clamped) != clamped)
        __builtin_trap();

    // Out-of-range values are clamped rather than rejected, in the documented
    // direction. This is the behaviour the two backends disagreed about before.
    if (raw < 1 && clamped != 1)
        __builtin_trap();
    if (raw > 300 && clamped != 300)
        __builtin_trap();
    if (raw >= 1 && raw <= 300 && clamped != raw)
        __builtin_trap();

    // 2. Renderer parsing must always yield one of the two known values; an
    //    unrecognised string falls back to FlameGraph rather than failing a
    //    request, which is what the parameter documentation says.
    const ChartRenderer renderer = parseChartRenderer(input);
    if (renderer != ChartRenderer::FlameGraph && renderer != ChartRenderer::CallGraph)
        __builtin_trap();
    if (input != "callgraph" && renderer != ChartRenderer::FlameGraph)
        __builtin_trap();

    // 3. A default-constructed ChartOptions must itself be within the documented
    //    range, since that is what an omitted parameter produces.
    const ChartOptions defaults;
    if (clampChartDuration(defaults.duration) != defaults.duration)
        __builtin_trap();

    return 0;
}
