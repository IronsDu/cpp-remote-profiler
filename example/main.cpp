#include "profiler_manager.h"
#include "web_server.h"
#include "workload.h"
#include <chrono>
#include <drogon/drogon.h>
#include <iostream>
#include <thread>

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    std::cout << "C++ Remote Profiler Example\n";
    std::cout << "============================\n\n";

    int port = 8080;
    std::string host = "0.0.0.0";

    std::cout << "Starting HTTP server on " << host << ":" << port << "\n";
    std::cout << "Open your browser and visit: http://localhost:" << port << "\n\n";

    // 启动后台线程 - 运行工作负载以生成 profiling 数据
    std::thread worker([]() {
        while (true) {
            cpuIntensiveTask();
            memoryIntensiveTask();
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    });
    worker.detach();

    // 获取 profiler 实例
    auto& profiler = profiler::ProfilerManager::getInstance();

    // 注册所有 HTTP 路由处理器
    std::cout << "Registering HTTP handlers...\n";
    profiler::registerHttpHandlers(profiler);

    // 设置监听地址和端口
    std::cout << "Starting server on " << host << ":" << port << "...\n";
    drogon::app().addListener(host, port);

    // 启动服务器（阻塞）
    drogon::app().run();

    return 0;
}
