#pragma once

#include "profiler_version.h"
#include <functional>
#include <map>
#include <string>

PROFILER_NAMESPACE_BEGIN

/// HTTP request structure
struct Request {
    std::string method;                          ///< HTTP method (GET, POST, etc.)
    std::string path;                            ///< Request path
    std::string body;                            ///< Request body
    std::map<std::string, std::string> params;   ///< Query parameters
};

/// HTTP response structure
struct Response {
    int status_code = 200;                       ///< HTTP status code
    std::string body;                            ///< Response body
    std::string content_type = "text/plain";     ///< Content-Type header
    std::map<std::string, std::string> headers;  ///< Additional headers

    /// Set response as HTML content
    void setHtml(const std::string& html);

    /// Set response as JSON content
    void setJson(const std::string& json);

    /// Set response as SVG image
    void setSvg(const std::string& svg);

    /// Set response as binary download
    void setBinary(const std::string& data, const std::string& filename);
};

/// Abstract HTTP server interface
class HttpServer {
public:
    using Handler = std::function<void(const Request&, Response&)>;

    virtual ~HttpServer() = default;

    /// Register a GET route handler
    virtual void get(const std::string& path, Handler handler) = 0;

    /// Register a POST route handler
    virtual void post(const std::string& path, Handler handler) = 0;

    /// Start listening on the given host and port (blocking)
    virtual void listen(const std::string& host, int port) = 0;
};

PROFILER_NAMESPACE_END
