/// @file result_parsing.h
/// @brief Helpers for interpreting profiler-core return values
///
/// The ProfilerManager methods signal failure inconsistently by design (they are
/// older than the HTTP layer): the analyze*() family returns a `{"error": ...}`
/// JSON string, while the raw getters return an empty string. Web adapters need
/// to tell those apart and surface the actual reason, so the parsing lives here
/// where it can be unit tested in isolation from any real profiler state.

#pragma once

#include "profiler_version.h"
#include <string>

PROFILER_NAMESPACE_BEGIN

namespace internal {

/// @brief Whether @p value is one of the core's `{"error": ...}` strings
///
/// The check is structural rather than a JSON parse -- the core only ever emits
/// this exact shape, and callers sit on a request path. It does require the key,
/// so an unrelated JSON document that merely starts with `{"` is not mistaken
/// for a failure.
inline bool isJsonError(const std::string& value) {
    return value.size() > 10 && value[0] == '{' && value.find("\"error\"") != std::string::npos;
}

/// @brief Extract the message from a core `{"error": ...}` string
///
/// Needed because wrapping the raw JSON in another error response would both
/// double-wrap it and, since the inner quotes are unescaped, emit invalid JSON
/// (`{"error":"{"error": "..."}`). Substituting a fixed message instead would
/// discard the reason, which is how "cpu profiling already in use" used to
/// arrive at clients as a generic failure.
///
/// @param value A string previously accepted by isJsonError()
/// @return The inner message, or @p value unchanged if it cannot be parsed
inline std::string jsonErrorMessage(const std::string& value) {
    const std::string key = "\"error\"";
    auto key_pos = value.find(key);
    if (key_pos == std::string::npos) {
        return value;
    }
    auto open_quote = value.find('"', key_pos + key.size());
    if (open_quote == std::string::npos) {
        return value;
    }
    auto close_quote = value.rfind('"');
    if (close_quote == std::string::npos || close_quote <= open_quote) {
        return value;
    }
    return value.substr(open_quote + 1, close_quote - open_quote - 1);
}

/// @brief Whether a heap sample from GetHeapSample() carries no usable data
///
/// With heap sampling off (TCMALLOC_SAMPLE_PARAMETER unset, whose default is 0)
/// tcmalloc does not return an empty string. It returns pprof's `%warn`
/// advisory followed by a syntactically valid but empty profile:
///
/// ```
/// %warn
/// %warn This heap profile does not have any data in it, because
/// heap profile:      0:        0 [     0:        0] @ heap_v2/0
/// MAPPED_LIBRARIES:
/// ```
///
/// So testing for emptiness alone never fires, and callers render a graph out of
/// nothing. The zero sampling rate (or the leading `%warn`) marks the case.
inline bool isEmptyHeapSample(const std::string& sample) {
    return sample.empty() || sample.find("@ heap_v2/0") != std::string::npos || sample.rfind("%warn", 0) == 0;
}

} // namespace internal

PROFILER_NAMESPACE_END
