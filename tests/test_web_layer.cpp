/// @file test_web_layer.cpp
/// @brief End-to-end test of the Drogon adapter and the embedded web resources
///
/// Why this exists: every other test target is core-only (no Drogon), so
/// `drogon_adapter.cpp` and `web_resources.cpp` were compiled under ASan but never
/// executed by anything except the example binary, which ctest does not run. The
/// sanitizer build therefore looked like it covered the web layer while nothing
/// actually ran. This test starts the real server, drives it over HTTP and shuts it
/// down, so route registration, request delivery (including the async path through
/// AsyncExecutor and queueInLoop), the embedded pages and the shutdown sequence all
/// execute under the running sanitizer configuration.
///
/// Requests are issued with a raw socket rather than libcurl, to avoid adding a
/// dependency purely for tests. A detail that makes the assertions meaningful: the
/// response writing happens on a *different* thread, so a response that arrives at
/// all proves the cross-thread hand-off worked, not merely that a handler ran.

#include "../include/profiler/drogon_adapter.h"
#include "../include/profiler_manager.h"
#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <drogon/drogon.h>
#include <future>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

/// An HTTP response reduced to what the assertions need.
struct HttpResponse {
    int status = 0;
    std::string body;
    bool received = false;
};

/// Send one request over a fresh connection and read the whole reply.
///
/// Deliberately minimal: HTTP/1.1, `Connection: close`, so the reply ends when the
/// peer closes and no content-length parsing is needed.
HttpResponse request(uint16_t port, const std::string& method, const std::string& target,
                     const std::string& body = {}) {
    HttpResponse result;

    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return result;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        ::close(fd);
        return result;
    }

    std::string req = method + " " + target + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n";
    if (!body.empty())
        req += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    req += "\r\n" + body;
    if (::write(fd, req.data(), req.size()) < 0) {
        ::close(fd);
        return result;
    }

    std::string raw;
    char buf[4096];
    ssize_t n = 0;
    while ((n = ::read(fd, buf, sizeof(buf))) > 0)
        raw.append(buf, static_cast<size_t>(n));
    ::close(fd);

    const auto header_end = raw.find("\r\n\r\n");
    if (header_end == std::string::npos)
        return result;

    result.received = true;
    const auto status_line = raw.substr(0, raw.find("\r\n"));
    const auto first_space = status_line.find(' ');
    if (first_space != std::string::npos)
        result.status = std::atoi(status_line.c_str() + first_space + 1);
    result.body = raw.substr(header_end + 4);
    return result;
}

/// A Drogon server with the profiler routes, on an ephemeral port.
///
/// The server is started once per *suite*, not once per test. `drogon::app()` is a
/// process-global singleton whose lifecycle is bind-once: calling addListener() a
/// second time, after a previous run() has finished, segfaults inside
/// ListenerManager. Structuring this as per-test SetUp/TearDown looked natural and
/// crashed on the second test.
///
/// Port 0 lets the OS pick a free port and getListeners() reports which one, so
/// parallel ctest runs cannot collide.
class WebLayerTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        profiler_ = new profiler::ProfilerManager();
        profiler::registerDrogonHandlers(*profiler_);

        drogon::app().addListener("127.0.0.1", 0);
        server_ = std::async(std::launch::async, []() { drogon::app().run(); });

        // Wait for the listener to be bound rather than sleeping a fixed amount.
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (std::chrono::steady_clock::now() < deadline) {
            const auto listeners = drogon::app().getListeners();
            if (!listeners.empty()) {
                port_ = listeners.front().toPort();
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        if (port_ == 0)
            throw std::runtime_error("server never bound a port");
    }

    static void TearDownTestSuite() {
        if (server_.valid()) {
            drogon::app().getLoop()->queueInLoop([]() { drogon::app().quit(); });
            server_.wait();
        }
        delete profiler_;
        profiler_ = nullptr;
    }

    static uint16_t port_;

private:
    static profiler::ProfilerManager* profiler_;
    static std::future<void> server_;
};

uint16_t WebLayerTest::port_ = 0;
profiler::ProfilerManager* WebLayerTest::profiler_ = nullptr;
std::future<void> WebLayerTest::server_;

} // namespace

TEST_F(WebLayerTest, ServesTheEmbeddedControlPanel) {
    // Exercises WebResources::getIndexPage(): the page is embedded as a C++ string
    // literal, so nothing else checks that it is well-formed or that the route
    // wiring returns it.
    const auto resp = request(port_, "GET", "/");

    ASSERT_TRUE(resp.received);
    EXPECT_EQ(resp.status, 200);
    EXPECT_NE(resp.body.find("<!DOCTYPE html>"), std::string::npos);
    // The panel must reference the current API surface, not a removed path.
    EXPECT_NE(resp.body.find("/api/pprof/"), std::string::npos);
}

TEST_F(WebLayerTest, ServesStatusAsJson) {
    const auto resp = request(port_, "GET", "/api/status");

    ASSERT_TRUE(resp.received);
    EXPECT_EQ(resp.status, 200);
    EXPECT_NE(resp.body.find("\"cpu\""), std::string::npos);
    EXPECT_NE(resp.body.find("\"heap\""), std::string::npos);
}

TEST_F(WebLayerTest, ServesThreadStacksThroughTheSignalPath) {
    const auto resp = request(port_, "GET", "/api/thread/stacks");

    ASSERT_TRUE(resp.received);
    EXPECT_EQ(resp.status, 200);
    // The capture machinery ran, which means the signal handler and the shared
    // stack buffer were exercised on a request path.
    EXPECT_NE(resp.body.find("Thread Call Stacks"), std::string::npos);
}

TEST_F(WebLayerTest, ServesAAnalysisRequestThroughTheAsyncExecutor) {
    // The analysis endpoints are the ones that go through AsyncExecutor and come
    // back via queueInLoop, i.e. the response is written from another thread. That
    // makes this the case most likely to leak or race, and growth is used because
    // it needs neither TCMALLOC_SAMPLE_PARAMETER nor a sampling window.
    const auto resp = request(port_, "GET", "/api/pprof/growth?renderer=flamegraph");

    ASSERT_TRUE(resp.received);
    // Either a rendered graph or a reported failure, but the reply itself proves the
    // cross-thread hand-off completed.
    EXPECT_TRUE(resp.status == 200 || resp.status == 500) << "status: " << resp.status;
    if (resp.status == 200)
        EXPECT_NE(resp.body.find("<svg"), std::string::npos);
}

TEST_F(WebLayerTest, DeliversPprofSymbolRequests) {
    // The only POST route, and the only one whose body is parsed as addresses.
    const auto resp = request(port_, "POST", "/pprof/symbol", "0x401d8a+0x401d8b");

    ASSERT_TRUE(resp.received);
    EXPECT_EQ(resp.status, 200);
    EXPECT_NE(resp.body.find("0x401d8a"), std::string::npos);
}

TEST_F(WebLayerTest, ReportsUnknownRoutesAsNotFound) {
    const auto resp = request(port_, "GET", "/api/does-not-exist");

    ASSERT_TRUE(resp.received);
    EXPECT_EQ(resp.status, 404);
}
