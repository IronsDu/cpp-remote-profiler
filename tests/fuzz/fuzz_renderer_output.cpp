/// @file fuzz_renderer_output.cpp
/// @brief libFuzzer target for the renderer-output and request parsers
///
/// These functions receive bytes produced by external processes (flamegraph.pl,
/// pprof) or by a client (the symbolz request body) and decide what they mean.
/// That makes them the project's most fuzzable surface: pure, string-in/verdict-out,
/// and fed by something we do not control.
///
/// A note on the assertions. An earlier version of this target asserted properties
/// that felt obvious but are not actually guaranteed -- for example that a parsed
/// symbol token never contains a newline. That is false: when the body contains a
/// '+', plus-separation wins and newlines legitimately stay inside a token. The
/// target then "found" a crash on its first run that was the assertion's fault, not
/// the code's. Every check below is therefore limited to what the implementation
/// genuinely promises; everything else is left to ASan/UBSan, which cannot cry
/// wolf.

#include "internal/renderer_output_parsing.h"
#include <cstddef>
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    const std::string input(reinterpret_cast<const char*>(data), size);

    using namespace profiler::v0_1_0::internal;

    // 1. Renderer verdicts. The value here is that none of these throws and that
    //    the invariants the HTTP layer depends on hold.
    const bool svg_shaped = looksLikeSvg(input);
    const auto message = extractFlameGraphError(input);
    const auto failure = interpretFlameGraphOutput(input);

    // A message exists only when the parser actually found the marker.
    if (message && input.find("ERROR:") == std::string::npos)
        __builtin_trap();

    // A message is a slice of the input, so it can never be longer than it.
    if (message && message->size() > input.size())
        __builtin_trap();

    // An "ERROR:" marker must always be reported as a failure; handing a
    // renderer's error text back as a graph is the defect this check exists for.
    if (input.find("ERROR:") != std::string::npos && !failure)
        __builtin_trap();

    // Output that does not look like an SVG must never be accepted as a graph.
    if (!svg_shaped && !failure)
        __builtin_trap();

    // 2. pprof symbolz request bodies. The caller converts each token with
    //    stoull(), so a token must be a slice of the body. '+' can never survive
    //    (it wins as the separator whenever it appears); a newline may, because
    //    newline-splitting only applies when there is no '+'.
    const auto addresses = parseSymbolRequest(input);
    const bool plus_separated = input.find('+') != std::string::npos;
    for (const auto& address : addresses) {
        if (address.size() > input.size())
            __builtin_trap();
        if (address.find('+') != std::string::npos)
            __builtin_trap();
        if (!plus_separated && address.find('\n') != std::string::npos)
            __builtin_trap();
    }

    return 0;
}
