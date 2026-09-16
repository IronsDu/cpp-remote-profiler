/// @file test_renderer_parsing.cpp
/// @brief Unit tests for the renderer-output and request-body parsers
///
/// The fuzz targets in tests/fuzz/ explore these functions' input space; these
/// tests pin the specific behaviours the HTTP layer depends on, so a regression
/// names itself instead of arriving as a crash artifact. The two are complements:
/// the corpus keeps interesting inputs, and these keep the meaning of the answers.

#include "../src/internal/renderer_output_parsing.h"
#include <gtest/gtest.h>
#include <string>

using namespace profiler::v0_1_0::internal;

namespace {

/// The exact shape flamegraph.pl emits when it rejects its input: a structurally
/// valid SVG whose only content is the error text.
const char* kFlamegraphErrorSvg =
    "<svg version=\"1.1\" width=\"1200\" height=\"60\">\n"
    "<text  x=\"600.00\" y=\"24\" >ERROR: No valid input provided to flamegraph.pl.</text>\n"
    "</svg>\n";

} // namespace

TEST(FlameGraphErrorTest, FindsTheMessageInsideTheSvgTextElement) {
    const auto message = extractFlameGraphError(kFlamegraphErrorSvg);

    ASSERT_TRUE(message.has_value());
    EXPECT_EQ(*message, "ERROR: No valid input provided to flamegraph.pl.");
}

TEST(FlameGraphErrorTest, MessageWithoutATrailingTagRunsToTheEnd) {
    // flamegraph.pl's other error path warns on stderr, but a caller that merged
    // stderr into stdout would hand us a bare line. There is no '<' after the
    // marker, so the message is the rest of the string -- and computing the length
    // as (npos - pos) is what this guards: it must not throw.
    const auto message = extractFlameGraphError("ERROR: No stack counts found");

    ASSERT_TRUE(message.has_value());
    EXPECT_EQ(*message, "ERROR: No stack counts found");
}

TEST(FlameGraphErrorTest, NoMarkerMeansNoMessage) {
    EXPECT_FALSE(extractFlameGraphError("<svg><text>42 samples</text></svg>").has_value());
    EXPECT_FALSE(extractFlameGraphError("").has_value());
}

TEST(FlameGraphErrorTest, MarkerAtTheVeryEndIsReported) {
    const auto message = extractFlameGraphError("ERROR:");
    ASSERT_TRUE(message.has_value());
    EXPECT_EQ(*message, "ERROR:");
}

TEST(FlameGraphErrorTest, SuccessfulSvgIsNotAFailure) {
    // The case that matters most: a graph with no error marker must be accepted.
    // A mis-parse here turns every successful render into a 500.
    const std::string ok = "<?xml version=\"1.0\"?><svg width=\"1200\" height=\"60\"></svg>";
    EXPECT_FALSE(interpretFlameGraphOutput(ok).has_value());
}

TEST(FlameGraphErrorTest, ErrorSvgIsAFailureWithAnExplanation) {
    const auto failure = interpretFlameGraphOutput(kFlamegraphErrorSvg);

    ASSERT_TRUE(failure.has_value());
    // The explanation is what the caller shows, so it must name the renderer's
    // reason as well as our own guidance.
    EXPECT_NE(failure->explanation().find("flamegraph.pl said:"), std::string::npos);
    EXPECT_NE(failure->explanation().find("No valid input provided"), std::string::npos);
}

TEST(FlameGraphErrorTest, NonSvgOutputIsAlwaysAFailure) {
    for (const char* junk : {"garbage", "", "ERROR:", "not xml at all"}) {
        EXPECT_TRUE(interpretFlameGraphOutput(junk).has_value()) << "input: " << junk;
    }
}

TEST(SymbolRequestTest, SplitsOnPlusAsGoPprofSends) {
    const auto addresses = parseSymbolRequest("0x401d8a+0x401d8b+0x401d8c");

    ASSERT_EQ(addresses.size(), 3u);
    EXPECT_EQ(addresses[0], "0x401d8a");
    EXPECT_EQ(addresses[2], "0x401d8c");
}

TEST(SymbolRequestTest, SplitsOnNewlinesAndSkipsComments) {
    const auto addresses = parseSymbolRequest("0x1\n# a comment\n0x2\n");

    ASSERT_EQ(addresses.size(), 2u);
    EXPECT_EQ(addresses[0], "0x1");
    EXPECT_EQ(addresses[1], "0x2");
}

TEST(SymbolRequestTest, HandlesCrlfInput) {
    const auto addresses = parseSymbolRequest("0x1\r\n0x2\r\n");

    ASSERT_EQ(addresses.size(), 2u);
    // A stray '\r' would reach stoull() and be rejected there, turning a valid
    // request into an unresolved address.
    EXPECT_EQ(addresses[0], "0x1");
    EXPECT_EQ(addresses[1], "0x2");
}

TEST(SymbolRequestTest, EmptyPartsAreSkipped) {
    EXPECT_TRUE(parseSymbolRequest("").empty());
    EXPECT_TRUE(parseSymbolRequest("+").empty());
    EXPECT_TRUE(parseSymbolRequest("++0x1++").size() == 1u);
    EXPECT_TRUE(parseSymbolRequest("\n\n").empty());
}

TEST(SymbolRequestTest, PlusWinsOverNewlineSeparation) {
    // Documented precedence: when a '+' is present, newlines are ordinary bytes
    // inside a token rather than separators. Asserted because the fuzz target's
    // first version got this wrong and "found" a bug that was not there.
    const auto addresses = parseSymbolRequest("0x1\n0x2+0x3");

    ASSERT_EQ(addresses.size(), 2u);
    EXPECT_EQ(addresses[0], "0x1\n0x2");
    EXPECT_EQ(addresses[1], "0x3");
}
