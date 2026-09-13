/// @file drogon_adapter.cpp
/// @brief Drogon integration: registers profiler routes via ProfilerHttpHandlers

#include "profiler/drogon_adapter.h"
#include "internal/web_resources.h"
#include "profiler/async_executor.h"
#include "profiler/http_handlers.h"
#include <drogon/drogon.h>
#include <iostream>
#include <memory>
#include <optional>

PROFILER_NAMESPACE_BEGIN

/// Helper: adapt HandlerResponse to Drogon HttpResponse
static void sendResponse(const HandlerResponse& hr, std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(static_cast<drogon::HttpStatusCode>(hr.status));
    resp->setBody(hr.body);

    if (hr.content_type == "text/html") {
        resp->setContentTypeCode(drogon::CT_TEXT_HTML);
    } else if (hr.content_type == "application/json") {
        resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    } else if (hr.content_type == "image/svg+xml" || hr.content_type == "text/xml") {
        resp->setContentTypeCode(drogon::CT_TEXT_XML);
    } else if (hr.content_type == "application/octet-stream") {
        resp->setContentTypeCode(drogon::CT_APPLICATION_OCTET_STREAM);
    } else {
        resp->setContentTypeCode(drogon::CT_TEXT_PLAIN);
    }

    for (auto& [key, val] : hr.headers) {
        resp->addHeader(key, val);
    }

    callback(resp);
}

namespace {

/// Wrap a callback so it can be captured by a shared_ptr and invoked once.
using SharedCallback = std::shared_ptr<std::function<void(const drogon::HttpResponsePtr&)>>;

/// Hand a HandlerResponse back to the event loop that owns this request.
///
/// The analysis jobs run on a worker thread, so the response must be delivered
/// through queueInLoop() rather than invoking the callback inline.
void deliverAsync(const drogon::HttpRequestPtr& req, const SharedCallback& callback, const HandlerResponse& hr) {
    auto loop = drogon::app().getLoop();
    if (!loop) {
        return;
    }

    // Nothing to do if the client already went away.
    if (!req->connected()) {
        return;
    }

    auto response = std::make_shared<HandlerResponse>(hr);
    loop->queueInLoop([callback, response]() {
        sendResponse(*response, std::function<void(const drogon::HttpResponsePtr&)>(*callback));
    });
}

/// Schedule a blocking handler on the executor instead of the event loop.
///
/// `call` performs the actual profiling work and runs on the worker thread; the
/// event-loop thread returns immediately either way.
///
/// @param precheck Optional predicate evaluated on the event-loop thread
///        *before* the request is handed to the worker. Returning a response
///        from it short-circuits the request. This is where "may this request
///        proceed?" must be answered: a check performed inside the job would run
///        only after the previous job finished, so it could never observe that
///        job still running.
template <typename Call>
void runAsync(const std::shared_ptr<AsyncExecutor>& executor, const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback, Call&& call,
              std::function<std::optional<HandlerResponse>()> precheck = {}) {
    auto shared_cb = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(callback));

    // Reject here, on the event-loop thread, while the other job is still running.
    if (precheck) {
        if (auto rejection = precheck()) {
            sendResponse(*rejection, std::move(*shared_cb));
            return;
        }
    }

    bool accepted = executor->submitBlocking([req, shared_cb, call]() {
        HandlerResponse hr = call();
        deliverAsync(req, shared_cb, hr);
    });

    if (!accepted) {
        // Only reachable while the executor is shutting down.
        sendResponse(HandlerResponse::error(503, "Profiler is shutting down"), std::move(*shared_cb));
    }
}

} // namespace

void registerDrogonHandlers(profiler::ProfilerManager& profiler) {
    auto handlers = std::make_shared<ProfilerHttpHandlers>(profiler);

    // Profiling takes seconds, so it must stay off the event-loop thread. The
    // executor also serializes the jobs, which gperftools requires: CPU
    // profiling keeps its session in process-global state.
    auto executor = std::make_shared<AsyncExecutor>();

    // --- GET routes ---
    auto registerGet = [&](const std::string& path, auto fn) {
        drogon::app().registerHandler(
            path,
            [handlers, fn = std::move(fn)]([[maybe_unused]] const drogon::HttpRequestPtr& req,
                                           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                sendResponse(((*handlers).*fn)(), std::move(callback));
            },
            {drogon::Get});
    };

    // CPU profiling is exclusive (one process-global gperftools session), so a
    // request that arrives while another is sampling is refused on the spot,
    // exactly like Go's net/http/pprof. The verdict is produced by the handler
    // itself so the wording and status stay in one place.
    auto makeCpuBusyPrecheck = [handlers](bool pprof_style) {
        return [handlers, pprof_style]() -> std::optional<HandlerResponse> {
            if (handlers->isCpuProfilerBusy()) {
                return handlers->cpuProfilerBusyResponse(pprof_style);
            }
            return std::nullopt;
        };
    };
    using CpuBusyPrecheck = decltype(makeCpuBusyPrecheck(true));

    // /pprof/profile must answer the way `go tool pprof` expects; the custom
    // /api/* endpoints use an accurate JSON status instead.
    const CpuBusyPrecheck cpu_busy_pprof = makeCpuBusyPrecheck(true);
    const CpuBusyPrecheck cpu_busy_json = makeCpuBusyPrecheck(false);

    // Heap analysis is exclusive for the same reason: it reconfigures the one
    // process-global heap profiler, so a concurrent call would race for it.
    auto heap_busy = [handlers]() -> std::optional<HandlerResponse> {
        if (handlers->isHeapAnalyzerBusy()) {
            return handlers->heapAnalyzerBusyResponse();
        }
        return std::nullopt;
    };

    // Same, but offloaded to the executor: used for handlers that block for
    // seconds (sampling) or shell out to pprof/flamegraph.pl (rendering).
    auto registerGetAsync = [&](const std::string& path, auto fn) {
        drogon::app().registerHandler(
            path,
            [handlers, executor, fn = std::move(fn)](const drogon::HttpRequestPtr& req,
                                                     std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                runAsync(executor, req, std::move(callback), [handlers, fn]() { return ((*handlers).*fn)(); });
            },
            {drogon::Get});
    };

    // --- Static pages (served directly via WebResources) ---
    drogon::app().registerHandler("/",
                                  []([[maybe_unused]] const drogon::HttpRequestPtr& req,
                                     std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                                      sendResponse(HandlerResponse::html(WebResources::getIndexPage()),
                                                   std::move(callback));
                                  },
                                  {drogon::Get});
    drogon::app().registerHandler("/show_svg.html",
                                  []([[maybe_unused]] const drogon::HttpRequestPtr& req,
                                     std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                                      sendResponse(HandlerResponse::html(WebResources::getCpuSvgViewerPage()),
                                                   std::move(callback));
                                  },
                                  {drogon::Get});
    drogon::app().registerHandler("/show_heap_svg.html",
                                  []([[maybe_unused]] const drogon::HttpRequestPtr& req,
                                     std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                                      sendResponse(HandlerResponse::html(WebResources::getHeapSvgViewerPage()),
                                                   std::move(callback));
                                  },
                                  {drogon::Get});
    drogon::app().registerHandler("/show_growth_svg.html",
                                  []([[maybe_unused]] const drogon::HttpRequestPtr& req,
                                     std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                                      sendResponse(HandlerResponse::html(WebResources::getGrowthSvgViewerPage()),
                                                   std::move(callback));
                                  },
                                  {drogon::Get});

    // --- Status ---
    registerGet("/api/status", &ProfilerHttpHandlers::handleStatus);

    // --- Thread stacks ---
    registerGet("/api/thread/stacks", &ProfilerHttpHandlers::handleThreadStacks);

    // --- Standard pprof: /pprof/profile (blocking: samples for `seconds`) ---
    drogon::app().registerHandler(
        "/pprof/profile",
        [handlers, executor, cpu_busy_pprof](const drogon::HttpRequestPtr& req,
                                             std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            int seconds = 30;
            auto p = req->getParameter("seconds");
            if (!p.empty()) {
                try {
                    seconds = std::stoi(p);
                } catch (...) {}
                if (seconds < 1)
                    seconds = 1;
                if (seconds > 300)
                    seconds = 300;
            }
            runAsync(
                executor, req, std::move(callback),
                [handlers, seconds]() { return handlers->handlePprofProfile(seconds); }, cpu_busy_pprof);
        },
        {drogon::Get});

    // --- Standard pprof: /pprof/heap ---
    registerGet("/pprof/heap", &ProfilerHttpHandlers::handlePprofHeap);

    // --- Standard pprof: /pprof/growth ---
    registerGet("/pprof/growth", &ProfilerHttpHandlers::handlePprofGrowth);

    // --- /pprof/symbol (POST) ---
    drogon::app().registerHandler("/pprof/symbol",
                                  [handlers]([[maybe_unused]] const drogon::HttpRequestPtr& req,
                                             std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                                      sendResponse(handlers->handlePprofSymbol(std::string(req->body())),
                                                   std::move(callback));
                                  },
                                  {drogon::Post});

    // --- CPU analyze (blocking: samples for `duration` seconds) ---
    drogon::app().registerHandler(
        "/api/cpu/analyze",
        [handlers, executor, cpu_busy_json](const drogon::HttpRequestPtr& req,
                                            std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            int duration = 10;
            auto dp = req->getParameter("duration");
            if (!dp.empty()) {
                try {
                    duration = std::stoi(dp);
                } catch (...) {}
            }
            std::string output_type = req->getParameter("output_type");
            if (output_type.empty())
                output_type = "pprof";

            runAsync(
                executor, req, std::move(callback),
                [handlers, duration, output_type]() { return handlers->handleCpuAnalyze(duration, output_type); },
                cpu_busy_json);
        },
        {drogon::Get, drogon::Post});

    // --- CPU raw SVG (blocking) ---
    drogon::app().registerHandler(
        "/api/cpu/svg_raw",
        [handlers, executor, cpu_busy_json](const drogon::HttpRequestPtr& req,
                                            std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            int duration = 10;
            auto dp = req->getParameter("duration");
            if (!dp.empty()) {
                try {
                    duration = std::stoi(dp);
                } catch (...) {}
            }
            runAsync(
                executor, req, std::move(callback),
                [handlers, duration]() { return handlers->handleCpuSvgRaw(duration); }, cpu_busy_json);
        },
        {drogon::Get});

    // --- CPU FlameGraph raw (blocking) ---
    drogon::app().registerHandler(
        "/api/cpu/flamegraph_raw",
        [handlers, executor, cpu_busy_json](const drogon::HttpRequestPtr& req,
                                            std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            int duration = 10;
            auto dp = req->getParameter("duration");
            if (!dp.empty()) {
                try {
                    duration = std::stoi(dp);
                } catch (...) {}
            }
            runAsync(
                executor, req, std::move(callback),
                [handlers, duration]() { return handlers->handleCpuFlamegraphRaw(duration); }, cpu_busy_json);
        },
        {drogon::Get});

    // --- Heap analyze (blocking) ---
    drogon::app().registerHandler(
        "/api/heap/analyze",
        [handlers, executor, heap_busy](const drogon::HttpRequestPtr& req,
                                        std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            int duration = 1;
            auto dp = req->getParameter("duration");
            if (!dp.empty()) {
                try {
                    duration = std::stoi(dp);
                } catch (...) {}
            }
            std::string output_type = req->getParameter("output_type");
            if (output_type.empty())
                output_type = "pprof";
            runAsync(
                executor, req, std::move(callback),
                [handlers, duration, output_type]() { return handlers->handleHeapAnalyze(duration, output_type); },
                heap_busy);
        },
        {drogon::Get});

    // --- Heap raw / FlameGraph (blocking: sample window + pprof/flamegraph.pl) ---
    // These take a duration too, so the collection window is the caller's choice
    // rather than whatever the process happened to be started with.
    auto registerHeapRawAsync = [&](const std::string& path, auto fn) {
        drogon::app().registerHandler(
            path,
            [handlers, executor, fn = std::move(fn)](const drogon::HttpRequestPtr& req,
                                                     std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                int duration = 1;
                auto dp = req->getParameter("duration");
                if (!dp.empty()) {
                    try {
                        duration = std::stoi(dp);
                    } catch (...) {}
                }
                runAsync(executor, req, std::move(callback),
                         [handlers, fn, duration]() { return ((*handlers).*fn)(duration); });
            },
            {drogon::Get});
    };

    registerHeapRawAsync("/api/heap/svg_raw", &ProfilerHttpHandlers::handleHeapSvgRaw);
    registerHeapRawAsync("/api/heap/flamegraph_raw", &ProfilerHttpHandlers::handleHeapFlamegraphRaw);

    // --- Growth analyze (blocking: shells out to pprof/flamegraph.pl) ---
    drogon::app().registerHandler("/api/growth/analyze",
                                  [handlers, executor](const drogon::HttpRequestPtr& req,
                                                       std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                                      std::string output_type = req->getParameter("output_type");
                                      if (output_type.empty())
                                          output_type = "pprof";
                                      // No precheck: growth stacks come from GetHeapGrowthStacks() and read
                                      // no profiler session, so this may run alongside CPU sampling.
                                      runAsync(executor, req, std::move(callback), [handlers, output_type]() {
                                          return handlers->handleGrowthAnalyze(output_type);
                                      });
                                  },
                                  {drogon::Get});

    // --- Growth raw / FlameGraph (blocking: shells out to pprof/flamegraph.pl) ---
    registerGetAsync("/api/growth/svg_raw", &ProfilerHttpHandlers::handleGrowthSvgRaw);
    registerGetAsync("/api/growth/flamegraph_raw", &ProfilerHttpHandlers::handleGrowthFlamegraphRaw);
}

PROFILER_NAMESPACE_END
