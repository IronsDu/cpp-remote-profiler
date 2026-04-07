#include "profiler_manager.h"
#include "web_server.h"
#include "workload.h"
#include <chrono>
#include <iostream>
#include <thread>

#ifdef REMOTE_PROFILER_ENABLE_WEB
#include "backends/drogon/drogon_http_server.h"
using Server = profiler::DrogonHttpServer;
#endif

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    std::cout << "C++ Remote Profiler Example\n";
    std::cout << "============================\n\n";

    int port = 8080;
    std::string host = "0.0.0.0";

    std::cout << "Starting HTTP server on " << host << ":" << port << "\n";
    std::cout << "Open your browser and visit: http://localhost:" << port << "\n\n";

    // Start background workload thread
    std::thread worker([]() {
        while (true) {
            cpuIntensiveTask();
            memoryIntensiveTask();
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    });
    worker.detach();

    // Create ProfilerManager instance (no longer a singleton)
    profiler::ProfilerManager profiler;

#ifdef REMOTE_PROFILER_ENABLE_WEB
    // Create HTTP server (Drogon backend)
    Server server;

    // Register all HTTP route handlers
    std::cout << "Registering HTTP handlers...\n";
    profiler::registerHttpHandlers(profiler, server);

    // Start server (blocking)
    std::cout << "Starting server on " << host << ":" << port << "...\n";
    server.listen(host, port);
#else
    std::cout << "Web UI disabled. Running in core-only mode.\n";
    // Keep the main thread alive
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
#endif

    return 0;
}
