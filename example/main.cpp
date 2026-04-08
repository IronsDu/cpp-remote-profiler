#include "profiler/drogon_adapter.h"
#include "profiler_manager.h"
#include "workload.h"
#include <chrono>
#include <iostream>
#include <thread>

#ifdef REMOTE_PROFILER_ENABLE_WEB
#include <drogon/drogon.h>
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
    // Register all HTTP route handlers with Drogon
    std::cout << "Registering HTTP handlers...\n";
    profiler::registerDrogonHandlers(profiler);

    // Start server (blocking)
    std::cout << "Starting server on " << host << ":" << port << "...\n";
    drogon::app().addListener(host, port).run();
#else
    std::cout << "Web UI disabled. Running in core-only mode.\n";
    // Keep the main thread alive
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
#endif

    return 0;
}
