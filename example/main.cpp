#include "profiler/drogon_adapter.h"
#include "profiler_manager.h"
#include "workload.h"
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <pthread.h>
#include <string>
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

    // Give the sample threads recognisable names so /api/thread/stacks can be
    // checked against what the kernel reports (the endpoint reads
    // /proc/<tid>/comm, the same source ps and top use). The names below are
    // deliberately unmistakable: if they show up in the endpoint output, the
    // naming works; if they do not, it does not.
    auto nameCurrentThread = [](const std::string& name) {
        // Linux truncates thread names to 15 characters plus the terminator.
        pthread_setname_np(pthread_self(), name.substr(0, 15).c_str());
    };

    // Busy thread: alternately burns CPU and allocates, then idles 5s.
    std::thread worker(
        [](auto setName) {
            setName("profiler-worker");
            while (true) {
                cpuIntensiveTask();
                memoryIntensiveTask();
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        },
        nameCurrentThread);
    worker.detach();

    // A second thread that mostly sleeps, so the output shows a thread blocked in
    // a timer wait next to the busy one.
    std::thread sleeper(
        [](auto setName) {
            setName("profiler-sleeper");
            while (true) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1500));
            }
        },
        nameCurrentThread);
    sleeper.detach();

    // A third thread parked on a condition variable forever: its stack top should
    // read as a futex wait, which is the clearest "stuck here" signal.
    std::thread parked(
        [](auto setName) {
            setName("profiler-parked");
            std::mutex mutex;
            std::condition_variable never_signalled;
            std::unique_lock<std::mutex> lock(mutex);
            never_signalled.wait(lock, []() { return false; });
        },
        nameCurrentThread);
    parked.detach();

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
