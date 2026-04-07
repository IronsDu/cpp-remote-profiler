/// @file drogon_http_server.h
/// @brief Drogon-based implementation of the HttpServer interface

#pragma once

#include "profiler/http_server.h"
#include <map>
#include <string>

PROFILER_NAMESPACE_BEGIN

/// @brief Drogon-based HTTP server implementing the HttpServer interface
class DrogonHttpServer : public HttpServer {
public:
    DrogonHttpServer() = default;
    ~DrogonHttpServer() override = default;

    void get(const std::string& path, Handler handler) override;
    void post(const std::string& path, Handler handler) override;
    void listen(const std::string& host, int port) override;

private:
    struct Route {
        std::string method; // "GET" or "POST"
        Handler handler;
    };
    std::map<std::string, Route> routes_;
};

PROFILER_NAMESPACE_END
