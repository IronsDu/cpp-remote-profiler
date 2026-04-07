/// @file drogon_http_server.cpp
/// @brief Drogon adapter implementation for the HttpServer interface

#include "drogon_http_server.h"
#include <drogon/drogon.h>
#include <iostream>

using namespace drogon;

PROFILER_NAMESPACE_BEGIN

void DrogonHttpServer::get(const std::string& path, Handler handler) {
    routes_[path] = {"GET", std::move(handler)};
}

void DrogonHttpServer::post(const std::string& path, Handler handler) {
    routes_[path] = {"POST", std::move(handler)};
}

void DrogonHttpServer::listen(const std::string& host, int port) {
    // Register all collected routes with Drogon
    for (auto& [path, route] : routes_) {
        auto& handler = route.handler;
        auto method = route.method;

        if (method == "GET") {
            app().registerHandler(
                path,
                [handler]([[maybe_unused]] const HttpRequestPtr& req,
                          std::function<void(const HttpResponsePtr&)>&& callback) {
                    // Adapt Request
                    Request profiler_req;
                    profiler_req.method = "GET";
                    profiler_req.path = std::string(req->path());
                    profiler_req.body = std::string(req->body());

                    // Copy query parameters
                    for (auto& [key, val] : req->getParameters()) {
                        profiler_req.params[key] = val;
                    }

                    // Call handler
                    Response profiler_resp;
                    handler(profiler_req, profiler_resp);

                    // Adapt Response -> Drogon Response
                    auto resp = HttpResponse::newHttpResponse();
                    resp->setStatusCode(static_cast<HttpStatusCode>(profiler_resp.status_code));
                    resp->setBody(profiler_resp.body);

                    // Set content type
                    if (profiler_resp.content_type == "text/html") {
                        resp->setContentTypeCode(CT_TEXT_HTML);
                    } else if (profiler_resp.content_type == "application/json") {
                        resp->setContentTypeCode(CT_APPLICATION_JSON);
                    } else if (profiler_resp.content_type == "image/svg+xml" ||
                               profiler_resp.content_type == "text/xml") {
                        resp->setContentTypeCode(CT_TEXT_XML);
                    } else if (profiler_resp.content_type == "application/octet-stream") {
                        resp->setContentTypeCode(CT_APPLICATION_OCTET_STREAM);
                    } else {
                        resp->setContentTypeCode(CT_TEXT_PLAIN);
                    }

                    // Set extra headers
                    for (auto& [key, val] : profiler_resp.headers) {
                        resp->addHeader(key, val);
                    }

                    callback(resp);
                },
                {Get});
        } else if (method == "POST") {
            app().registerHandler(
                path,
                [handler]([[maybe_unused]] const HttpRequestPtr& req,
                          std::function<void(const HttpResponsePtr&)>&& callback) {
                    Request profiler_req;
                    profiler_req.method = "POST";
                    profiler_req.path = std::string(req->path());
                    profiler_req.body = std::string(req->body());

                    for (auto& [key, val] : req->getParameters()) {
                        profiler_req.params[key] = val;
                    }

                    Response profiler_resp;
                    handler(profiler_req, profiler_resp);

                    auto resp = HttpResponse::newHttpResponse();
                    resp->setStatusCode(static_cast<HttpStatusCode>(profiler_resp.status_code));
                    resp->setBody(profiler_resp.body);

                    if (profiler_resp.content_type == "text/html") {
                        resp->setContentTypeCode(CT_TEXT_HTML);
                    } else if (profiler_resp.content_type == "application/json") {
                        resp->setContentTypeCode(CT_APPLICATION_JSON);
                    } else if (profiler_resp.content_type == "image/svg+xml" ||
                               profiler_resp.content_type == "text/xml") {
                        resp->setContentTypeCode(CT_TEXT_XML);
                    } else if (profiler_resp.content_type == "application/octet-stream") {
                        resp->setContentTypeCode(CT_APPLICATION_OCTET_STREAM);
                    } else {
                        resp->setContentTypeCode(CT_TEXT_PLAIN);
                    }

                    for (auto& [key, val] : profiler_resp.headers) {
                        resp->addHeader(key, val);
                    }

                    callback(resp);
                },
                {Post});
        }
    }

    // Add listener and run
    app().addListener(host, port);
    std::cout << "DrogonHttpServer listening on " << host << ":" << port << std::endl;
    app().run();
}

PROFILER_NAMESPACE_END
