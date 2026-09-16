/// @file renderer_output_parsing.h
/// @brief Helpers for interpreting what the renderers (flamegraph.pl, pprof) produced
///
/// The renderers are external processes whose output is not fully under our
/// control: flamegraph.pl answers unusable input with a *valid* SVG whose only
/// content is an "ERROR: ..." message, so a structural check for `<?xml`/`<svg`
/// passes and a 200 carrying an error reaches the caller as if it were a graph.
/// Distinguishing that case needs the same parsing on both paths that render flame
/// graphs, so it lives here -- in one place, and out of reach of a divergence like
/// the one this header replaced (two copies of the same logic, one of which
/// underflowed when the message was not followed by a `<`).
///
/// These functions are pure: they take the renderer's bytes and return a verdict,
/// with no profiler state involved, which is what lets the fuzz targets in
/// tests/fuzz/ exercise them directly.

#pragma once

#include "profiler_version.h"
#include <optional>
#include <string>
#include <vector>

PROFILER_NAMESPACE_BEGIN

namespace internal {

/// @brief The kind of failure a renderer reported, if any
enum class RendererError {
    /// An "ERROR: ..." message was found; the message itself is returned alongside.
    FlameGraphReported,
};

/// @brief A renderer failure and the text that explains it
struct RendererFailure {
    RendererError kind;
    std::string message;

    /// @brief The user-facing reason, independent of which renderer produced it
    std::string explanation() const {
        switch (kind) {
        case RendererError::FlameGraphReported:
            return "No stack samples to render (profile is empty). The sample window may have been too "
                   "short or the process idle; increase the duration or sample under load. "
                   "flamegraph.pl said: " +
                   message;
        }
        return "The renderer failed for an unknown reason.";
    }
};

/// @brief Extract the message from flamegraph.pl's "ERROR: ..." output
///
/// flamegraph.pl emits `ERROR: <text>` without a trailing tag when it rejects its
/// input, and as the text content of an SVG element otherwise, so the message runs
/// either to the next `<` or to the end of the string.
///
/// @return the message, or std::nullopt when there is no "ERROR:" marker
inline std::optional<std::string> extractFlameGraphError(const std::string& output) {
    const auto pos = output.find("ERROR:");
    if (pos == std::string::npos)
        return std::nullopt;

    // Guard the length: when no '<' follows, the message is the rest of the
    // string. Subtracting here without the check would underflow (npos - pos) and
    // make substr() throw -- a real defect in the copy of this code that lived in
    // profiler_manager.cpp, which is why the parsing now happens in one place.
    const auto end = output.find('<', pos);
    const auto length = end == std::string::npos ? std::string::npos : end - pos;
    return output.substr(pos, length);
}

/// @brief Whether a renderer's output looks like a usable SVG
///
/// Structural only: this is the cheap check that an "ERROR:" payload passes, which
/// is exactly why the error check above exists as well.
inline bool looksLikeSvg(const std::string& output) {
    return output.find("<?xml") != std::string::npos || output.find("<svg") != std::string::npos;
}

/// @brief Decide whether renderer output is a graph, and if not, why
///
/// @return std::nullopt when the output is usable; the failure otherwise
inline std::optional<RendererFailure> interpretFlameGraphOutput(const std::string& output) {
    if (!looksLikeSvg(output))
        return RendererFailure{RendererError::FlameGraphReported, ""};

    if (auto message = extractFlameGraphError(output))
        return RendererFailure{RendererError::FlameGraphReported, *message};

    return std::nullopt;
}

/// @brief Split a pprof symbolz request body into addresses
///
/// `go tool pprof` sends addresses joined by '+'; newline-separated input is also
/// accepted, and '#' starts a comment line. Anything else is treated as an address
/// candidate -- validation belongs to the caller, which has to convert them.
inline std::vector<std::string> parseSymbolRequest(const std::string& body) {
    std::vector<std::string> addresses;

    if (body.find('+') != std::string::npos) {
        std::size_t start = 0;
        while (start <= body.size()) {
            const auto sep = body.find('+', start);
            const auto end = sep == std::string::npos ? body.size() : sep;
            if (end > start)
                addresses.push_back(body.substr(start, end - start));
            if (sep == std::string::npos)
                break;
            start = sep + 1;
        }
        return addresses;
    }

    std::size_t start = 0;
    while (start <= body.size()) {
        const auto sep = body.find('\n', start);
        const auto end = sep == std::string::npos ? body.size() : sep;
        if (end > start) {
            // Strip a trailing '\r' so CRLF input behaves like LF input.
            auto stop = end;
            if (stop > start && body[stop - 1] == '\r')
                --stop;
            if (stop > start && body[start] != '#')
                addresses.push_back(body.substr(start, stop - start));
        }
        if (sep == std::string::npos)
            break;
        start = sep + 1;
    }
    return addresses;
}

} // namespace internal

PROFILER_NAMESPACE_END
