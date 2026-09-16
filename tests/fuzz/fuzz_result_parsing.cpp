/// @file fuzz_result_parsing.cpp
/// @brief libFuzzer target for the profiler-core return-value parsers
///
/// `internal::isJsonError()` / `jsonErrorMessage()` decide whether a string the
/// core returned is a `{"error": ...}` report and what it says, and
/// `isEmptyHeapSample()` decides whether a heap profile carries data. All three are
/// pure predicates on values that originate outside this process (tcmalloc,
/// gperftools, the renderers), so they are cheap to fuzz, and their answers matter:
/// misreading a failure as data is what let a 500 ship as a 200 earlier in this
/// project.
///
/// A note on the assertions. The first version of this target required an error's
/// message to be non-empty, which sounds self-evident and is wrong: the extractor
/// legitimately returns nothing for `{"error":""}`, and `isJsonError()` is a
/// structural check that also accepts malformed input like `{{"error{`. The target
/// duly reported a "crash" that was the assertion's fault. Only properties the
/// implementation actually guarantees are asserted here.

#include "internal/result_parsing.h"
#include <cstddef>
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    const std::string input(reinterpret_cast<const char*>(data), size);

    using namespace profiler::v0_1_0::internal;

    // 1. isJsonError() / jsonErrorMessage(). Neither may throw on any input: they
    //    run on a request path over strings the process did not author.
    const bool is_error = isJsonError(input);
    const std::string message = jsonErrorMessage(input);

    // The predicate's own definition: a `{"...` string carrying the key is an
    // error report. If this stops holding, the HTTP layer stops recognising core
    // failures -- which is the regression worth catching.
    if (input.size() > 10 && input[0] == '{' && input.find("\"error\"") != std::string::npos && !is_error)
        __builtin_trap();

    // A leading '{' is required, so anything else must not classify as an error.
    if (!input.empty() && input[0] != '{' && is_error)
        __builtin_trap();

    // The extractor falls back to echoing the input when it cannot find a message,
    // so its result is never longer than what it was given.
    if (message.size() > input.size())
        __builtin_trap();

    // 2. isEmptyHeapSample(). Only the forms the implementation documents as empty,
    //    plus the empty string itself, which callers report as "no data".
    const bool empty = isEmptyHeapSample(input);

    if (input.empty() && !empty)
        __builtin_trap();

    if (input.rfind("%warn", 0) == 0 && !empty)
        __builtin_trap();

    if (input.find("@ heap_v2/0") != std::string::npos && !empty)
        __builtin_trap();

    return 0;
}
