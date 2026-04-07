/// @file web_server.cpp
/// @brief Drogon integration: registers profiler routes using ProfilerHttpHandlers

#include "web_server.h"
#include "internal/web_resources.h"
#include "profiler/http_handlers.h"
#include <drogon/drogon.h>
#include <iostream>

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

void registerHttpHandlers(profiler::ProfilerManager& profiler) {
    auto handlers = std::make_shared<ProfilerHttpHandlers>(profiler);

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

    // --- Standard pprof: /pprof/profile ---
    drogon::app().registerHandler("/pprof/profile",
                                  [handlers]([[maybe_unused]] const drogon::HttpRequestPtr& req,
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
                                      sendResponse(handlers->handlePprofProfile(seconds), std::move(callback));
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

    // --- CPU analyze ---
    drogon::app().registerHandler("/api/cpu/analyze",
                                  [handlers]([[maybe_unused]] const drogon::HttpRequestPtr& req,
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
                                      sendResponse(handlers->handleCpuAnalyze(duration, output_type),
                                                   std::move(callback));
                                  },
                                  {drogon::Get, drogon::Post});

    // --- CPU raw SVG ---
    drogon::app().registerHandler("/api/cpu/svg_raw",
                                  [handlers]([[maybe_unused]] const drogon::HttpRequestPtr& req,
                                             std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                                      int duration = 10;
                                      auto dp = req->getParameter("duration");
                                      if (!dp.empty()) {
                                          try {
                                              duration = std::stoi(dp);
                                          } catch (...) {}
                                      }
                                      sendResponse(handlers->handleCpuSvgRaw(duration), std::move(callback));
                                  },
                                  {drogon::Get});

    // --- CPU FlameGraph raw ---
    drogon::app().registerHandler("/api/cpu/flamegraph_raw",
                                  [handlers]([[maybe_unused]] const drogon::HttpRequestPtr& req,
                                             std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                                      int duration = 10;
                                      auto dp = req->getParameter("duration");
                                      if (!dp.empty()) {
                                          try {
                                              duration = std::stoi(dp);
                                          } catch (...) {}
                                      }
                                      sendResponse(handlers->handleCpuFlamegraphRaw(duration), std::move(callback));
                                  },
                                  {drogon::Get});

    // --- Heap analyze ---
    drogon::app().registerHandler("/api/heap/analyze",
                                  [handlers]([[maybe_unused]] const drogon::HttpRequestPtr& req,
                                             std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                                      std::string output_type = req->getParameter("output_type");
                                      if (output_type.empty())
                                          output_type = "pprof";
                                      sendResponse(handlers->handleHeapAnalyze(output_type), std::move(callback));
                                  },
                                  {drogon::Get});

    // --- Heap raw / FlameGraph ---
    registerGet("/api/heap/svg_raw", &ProfilerHttpHandlers::handleHeapSvgRaw);
    registerGet("/api/heap/flamegraph_raw", &ProfilerHttpHandlers::handleHeapFlamegraphRaw);

    // --- Growth analyze ---
    drogon::app().registerHandler("/api/growth/analyze",
                                  [handlers]([[maybe_unused]] const drogon::HttpRequestPtr& req,
                                             std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                                      std::string output_type = req->getParameter("output_type");
                                      if (output_type.empty())
                                          output_type = "pprof";
                                      sendResponse(handlers->handleGrowthAnalyze(output_type), std::move(callback));
                                  },
                                  {drogon::Get});

    // --- Growth raw / FlameGraph ---
    registerGet("/api/growth/svg_raw", &ProfilerHttpHandlers::handleGrowthSvgRaw);
    registerGet("/api/growth/flamegraph_raw", &ProfilerHttpHandlers::handleGrowthFlamegraphRaw);
}

PROFILER_NAMESPACE_END
