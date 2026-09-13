/// @file test_result_parsing.cpp
/// @brief Tests for interpreting profiler-core return values
///
/// These cover a class of bug that reached users: a failure the HTTP layer could
/// not recognise, so it either reported success or replaced the reason with a
/// generic message.

#include "../src/internal/result_parsing.h"
#include <gtest/gtest.h>
#include <string>

using profiler::internal::isEmptyHeapSample;
using profiler::internal::isJsonError;
using profiler::internal::jsonErrorMessage;

// ---------------------------------------------------------------------------
// isJsonError / jsonErrorMessage
// ---------------------------------------------------------------------------

TEST(JsonErrorTest, RecognisesCoreErrorStrings) {
    EXPECT_TRUE(isJsonError(R"({"error": "Failed to start CPU profiler"})"));
    EXPECT_TRUE(isJsonError(R"({"error": "cpu profiling already in use"})"));
    EXPECT_TRUE(isJsonError(R"({"error": "No heap profile data was produced. Details here."})"));
}

TEST(JsonErrorTest, DoesNotMistakeSvgOrTextForAnError) {
    EXPECT_FALSE(isJsonError(""));
    EXPECT_FALSE(isJsonError("<svg xmlns=\"http://www.w3.org/2000/svg\"></svg>"));
    EXPECT_FALSE(isJsonError("<?xml version=\"1.0\"?><svg/>"));
    EXPECT_FALSE(isJsonError(R"({"err": "wrong key"})")) << "only {\"error\" is an error payload";
    EXPECT_FALSE(isJsonError("heap profile: 1: 20000 @ heap_v2/524288"));
    EXPECT_FALSE(isJsonError("{}"));
}

TEST(JsonErrorTest, ExtractsTheInnerMessage) {
    // The exact shape the core emits, including an embedded apostrophe.
    EXPECT_EQ(jsonErrorMessage(R"({"error": "cpu profiling already in use"})"), "cpu profiling already in use");
    EXPECT_EQ(jsonErrorMessage(R"({"error": "Failed to start CPU profiler"})"), "Failed to start CPU profiler");
}

TEST(JsonErrorTest, ExtractedMessageRoundTripsThroughAnotherWrap) {
    // Regression guard for the double-wrap bug: the old code passed the whole
    // JSON string to HandlerResponse::error(), producing
    //   {"error":"{"error": "..."}
    // which is not valid JSON. Re-wrapping the *message* must stay parseable.
    const std::string core_error = R"({"error": "cpu profiling already in use"})";
    const std::string message = jsonErrorMessage(core_error);
    const std::string wrapped = "{\"error\":\"" + message + "\"}";

    EXPECT_EQ(wrapped.find_first_of('{', 1), std::string::npos)
        << "wrapped response must contain a single brace group: " << wrapped;
    EXPECT_EQ(wrapped, R"({"error":"cpu profiling already in use"})");
}

TEST(JsonErrorTest, KeepsLongMessagesIntact) {
    // The heap "no data" message is long and contains punctuation and a URL-ish
    // word; it must survive extraction unchanged.
    const std::string core_error =
        R"({"error": "No heap profile data was produced. The heap profiler only records samples for allocations made while it is running; try /tmp/cpp_profiler."})";
    const std::string message = jsonErrorMessage(core_error);
    EXPECT_EQ(message.rfind("No heap profile data was produced.", 0), 0);
    EXPECT_NE(message.find("try /tmp/cpp_profiler."), std::string::npos);
    EXPECT_EQ(message.back(), '.');
}

TEST(JsonErrorTest, FallsBackToTheInputWhenUnparseable) {
    // Defensive: a malformed string is returned as-is rather than truncated to
    // something meaningless.
    const std::string odd = "{\"err\": \"no error key here\"}";
    EXPECT_EQ(jsonErrorMessage(odd), odd);
    EXPECT_EQ(jsonErrorMessage("not json at all"), "not json at all");
}

// ---------------------------------------------------------------------------
// isEmptyHeapSample
// ---------------------------------------------------------------------------

TEST(EmptyHeapSampleTest, DetectsWhatTcmallocEmitsWhenSamplingIsOff) {
    // Verbatim shape produced by GetHeapSample() with TCMALLOC_SAMPLE_PARAMETER
    // unset: a %warn advisory that still looks like a valid profile, which is why
    // the old `empty()` check never fired.
    const std::string off = "%warn\n"
                            "%warn This heap profile does not have any data in it, because\n"
                            "%warn the application was run with heap sampling turned off.\n"
                            "%warn To get useful data from GetHeapSample(), you must\n"
                            "%warn set the environment variable TCMALLOC_SAMPLE_PARAMETER to\n"
                            "%warn a positive sampling period, such as 524288.\n"
                            "%warn\n"
                            "heap profile:      0:        0 [     0:        0] @ heap_v2/0\n"
                            "\n"
                            "MAPPED_LIBRARIES:\n"
                            "00400000-0040b000 r-xp 00000000 00:00 535369 /tmp/app\n";

    EXPECT_FALSE(off.empty()) << "precondition: this is why empty() was not enough";
    EXPECT_TRUE(isEmptyHeapSample(off));
}

TEST(EmptyHeapSampleTest, DetectsZeroRateEvenWithoutTheWarning) {
    // Some paths emit just the zero-rate header; the marker alone must suffice.
    EXPECT_TRUE(isEmptyHeapSample("heap profile:      0:        0 [     0:        0] @ heap_v2/0\n"));
}

TEST(EmptyHeapSampleTest, DetectsTheEmptyString) {
    EXPECT_TRUE(isEmptyHeapSample(""));
}

TEST(EmptyHeapSampleTest, AcceptsARealSample) {
    const std::string real = "heap profile:      1:    20000 [     1:    20000] @ heap_v2/524288\n"
                             "     1:    20000 [     1:    20000] @ 0x401d8a 0x4080cc\n"
                             "\n"
                             "MAPPED_LIBRARIES:\n";

    EXPECT_FALSE(isEmptyHeapSample(real));
}

TEST(EmptyHeapSampleTest, AProfileCarryingTheZeroRateMarkerIsRejectedWhereverItAppears) {
    // The marker is searched for anywhere in the text, not only at the start:
    // a profile whose rate line says heap_v2/0 has no samples regardless of what
    // else it carries, and pprof would report it as empty too.
    const std::string zero_rate_with_frames = "heap profile:      0:        0 [     0:        0] @ heap_v2/0\n"
                                              "MAPPED_LIBRARIES:\n"
                                              "00400000-0040b000 r-xp 00000000 00:00 535369 /tmp/app\n";

    EXPECT_TRUE(isEmptyHeapSample(zero_rate_with_frames));
}
