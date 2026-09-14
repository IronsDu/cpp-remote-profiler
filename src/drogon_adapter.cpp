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
    } else if (hr.content_type == "image/svg+xml") {
        // CT_IMAGE_SVG_XML, not CT_TEXT_XML: the latter is the generic XML type.
        // The SVG type is what tells the browser to treat the body as an image
        // document (and what makes "save image as" work as an SVG).
        resp->setContentTypeCode(drogon::CT_IMAGE_SVG_XML);
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

/// Read the analysis endpoints' query parameters into a ChartOptions.
///
/// Defaults are deliberate: `duration` 10s because short windows collect too few
/// samples to render (measured: 3s failed 35-45% of the time, 10s never), and
/// `inline` so a plain link shows the SVG instead of downloading it.
ChartOptions chartOptionsFrom(const drogon::HttpRequestPtr& req) {
    ChartOptions options;

    options.renderer = parseChartRenderer(req->getParameter("renderer"));

    auto d = req->getParameter("duration");
    if (!d.empty()) {
        try {
            options.duration = std::stoi(d);
        } catch (...) {}
    }

    if (req->getParameter("output") == "attachment")
        options.inline_display = false;

    return options;
}

/// Whether a CPU request should be refused, and with what.
std::optional<HandlerResponse> cpuBusyRejection(const ProfilerHttpHandlers& handlers) {
    if (handlers.isCpuProfilerBusy())
        return handlers.cpuProfilerBusyResponse(/*pprof_style=*/false);
    return std::nullopt;
}

/// Whether a heap analysis request should be refused, and with what.
std::optional<HandlerResponse> heapBusyRejection(const ProfilerHttpHandlers& handlers) {
    if (handlers.isHeapAnalyzerBusy())
        return handlers.heapAnalyzerBusyResponse();
    return std::nullopt;
}

} // namespace

void registerDrogonHandlers(profiler::ProfilerManager& profiler) {
    auto handlers = std::make_shared<ProfilerHttpHandlers>(profiler);

    // Profiling takes seconds, so it must stay off the event-loop thread. The
    // executor also serializes the jobs, which gperftools requires: CPU
    // profiling keeps its session in process-global state.
    auto executor = std::make_shared<AsyncExecutor>();

    // ---------------------------------------------------------------------
    // Analysis endpoints: /api/pprof/{cpu,heap,growth}
    //
    // Each takes renderer / duration / output. The exclusivity verdict is
    // evaluated here, on the event-loop thread before the job is queued -- a
    // check performed inside the job runs only after the previous job finished,
    // so it could never observe that job still running.
    // ---------------------------------------------------------------------
    auto registerChart = [&](const std::string& path, HandlerResponse (ProfilerHttpHandlers::*fn)(const ChartOptions&),
                             std::optional<HandlerResponse> (*precheck)(const ProfilerHttpHandlers&)) {
        drogon::app().registerHandler(
            path,
            [handlers, executor, fn, precheck](const drogon::HttpRequestPtr& req,
                                               std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                if (precheck) {
                    if (auto rejection = precheck(*handlers)) {
                        sendResponse(*rejection, std::move(callback));
                        return;
                    }
                }
                const ChartOptions options = chartOptionsFrom(req);
                runAsync(executor, req, std::move(callback),
                         [handlers, fn, options]() { return ((*handlers).*fn)(options); });
            },
            {drogon::Get});
    };

    registerChart("/api/pprof/cpu", &ProfilerHttpHandlers::handleCpuChart, &cpuBusyRejection);
    registerChart("/api/pprof/heap", &ProfilerHttpHandlers::handleHeapChart, &heapBusyRejection);
    registerChart("/api/pprof/growth", &ProfilerHttpHandlers::handleGrowthChart, nullptr);

    // ---------------------------------------------------------------------
    // Heap snapshot: state-based, i.e. "what is in the heap now" rather than
    // "what was allocated during a window". Needs TCMALLOC_SAMPLE_PARAMETER.
    // ---------------------------------------------------------------------
    drogon::app().registerHandler("/api/pprof/heap/snapshot",
                                  [handlers, executor](const drogon::HttpRequestPtr& req,
                                                       std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                                      const ChartOptions options = chartOptionsFrom(req);
                                      const bool as_profile = req->getParameter("format") != "svg";
                                      runAsync(executor, req, std::move(callback), [handlers, options, as_profile]() {
                                          return handlers->handleHeapSnapshot(options, as_profile);
                                      });
                                  },
                                  {drogon::Get});

    // --- Status and auxiliary endpoints (fast enough to stay on the loop) ---
    drogon::app().registerHandler(
        "/api/status",
        [handlers](const drogon::HttpRequestPtr&, std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            sendResponse(handlers->handleStatus(), std::move(cb));
        },
        {drogon::Get});
    drogon::app().registerHandler(
        "/api/thread/stacks",
        [handlers](const drogon::HttpRequestPtr&, std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            sendResponse(handlers->handleThreadStacks(), std::move(cb));
        },
        {drogon::Get});

    // --- Web control panel ---
    drogon::app().registerHandler(
        "/",
        [](const drogon::HttpRequestPtr&, std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            sendResponse(HandlerResponse::html(WebResources::getIndexPage()), std::move(cb));
        },
        {drogon::Get});

    // ---------------------------------------------------------------------
    // Standard Go pprof interface -- unchanged. These are the machine-readable
    // endpoints `go tool pprof` consumes, as opposed to the /api/pprof visuals.
    // ---------------------------------------------------------------------
    drogon::app().registerHandler("/pprof/profile",
                                  [handlers, executor](const drogon::HttpRequestPtr& req,
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
                                      runAsync(executor, req, std::move(callback),
                                               [handlers, seconds]() { return handlers->handlePprofProfile(seconds); });
                                  },
                                  {drogon::Get});
    drogon::app().registerHandler("/pprof/heap",
                                  [handlers, executor](const drogon::HttpRequestPtr& req,
                                                       std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                                      runAsync(executor, req, std::move(callback),
                                               [handlers]() { return handlers->handlePprofHeap(); });
                                  },
                                  {drogon::Get});
    drogon::app().registerHandler("/pprof/growth",
                                  [handlers, executor](const drogon::HttpRequestPtr& req,
                                                       std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                                      runAsync(executor, req, std::move(callback),
                                               [handlers]() { return handlers->handlePprofGrowth(); });
                                  },
                                  {drogon::Get});
    drogon::app().registerHandler(
        "/pprof/symbol",
        [handlers](const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            sendResponse(handlers->handlePprofSymbol(std::string(req->body())), std::move(callback));
        },
        {drogon::Post});
}

PROFILER_NAMESPACE_END
