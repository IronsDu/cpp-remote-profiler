#include <drogon/drogon.h>
#include "profiler_manager.h"
#include <thread>
#include <chrono>
#include <iostream>
#include <vector>
#include <random>
#include <sstream>
#include <fstream>

using namespace drogon;

// 模拟一些CPU密集型任务
void cpuIntensiveTask() {
    std::vector<int> data(10000);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 100);

    for (int i = 0; i < 1000; ++i) {
        for (auto& val : data) {
            val = dis(gen);
        }
        std::sort(data.begin(), data.end());

        auto fib = [](int n) {
            if (n <= 1) return n;
            int a = 0, b = 1;
            for (int i = 2; i <= n; ++i) {
                int temp = a + b;
                a = b;
                b = temp;
            }
            return b;
        };

        volatile int result = fib(50);
        (void)result;
    }
}

// 模拟内存分配任务
void memoryIntensiveTask() {
    std::vector<std::vector<int>> matrix;

    for (int i = 0; i < 100; ++i) {
        std::vector<int> row(1000);
        for (auto& val : row) {
            val = rand();
        }
        matrix.push_back(std::move(row));
    }

    for (int i = 0; i < 10; ++i) {
        auto* leak = new int[1000];
        (void)leak;
    }
}

// HTML 网页
std::string getWebPage() {
    std::ifstream htmlFile("../web/index.html");
    std::stringstream buffer;
    buffer << htmlFile.rdbuf();
    return buffer.str();
}

int main(int argc, char* argv[]) {
    std::cout << "C++ Remote Profiler Example\n";
    std::cout << "============================\n\n";

    int port = 8080;
    std::string host = "0.0.0.0";

    std::cout << "Starting HTTP server on " << host << ":" << port << "\n";
    std::cout << "Open your browser and visit: http://localhost:" << port << "\n\n";

    // 启动后台线程
    std::thread worker([&]() {
        while (true) {
            cpuIntensiveTask();
            memoryIntensiveTask();
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    });
    worker.detach();

    auto& profiler = profiler::ProfilerManager::getInstance();

    // 注册所有路由 - 必须在 addListener 之前！
    std::cout << "Registering HTTP handlers...\n";

    app().registerHandler(
        "/api/status",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            Json::Value root;
            root["cpu"]["running"] = profiler.isProfilerRunning(profiler::ProfilerType::CPU);
            root["heap"]["running"] = profiler.isProfilerRunning(profiler::ProfilerType::HEAP);

            auto cpuState = profiler.getProfilerState(profiler::ProfilerType::CPU);
            auto heapState = profiler.getProfilerState(profiler::ProfilerType::HEAP);

            root["cpu"]["output_path"] = cpuState.output_path;
            root["cpu"]["duration_ms"] = static_cast<Json::Int64>(cpuState.duration);

            root["heap"]["output_path"] = heapState.output_path;
            root["heap"]["duration_ms"] = static_cast<Json::Int64>(heapState.duration);

            auto resp = HttpResponse::newHttpJsonResponse(root);
            callback(resp);
        },
        {Get}
    );

    app().registerHandler(
        "/api/cpu/start",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            Json::Value root;
            bool success = profiler.startCPUProfiler();

            root["success"] = success;
            if (success) {
                root["message"] = "CPU profiler started successfully";
                auto state = profiler.getProfilerState(profiler::ProfilerType::CPU);
                root["output_path"] = state.output_path;
            } else {
                root["message"] = "Failed to start CPU profiler (may already be running)";
            }

            auto resp = HttpResponse::newHttpJsonResponse(root);
            callback(resp);
        },
        {Post}
    );

    app().registerHandler(
        "/api/cpu/stop",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            Json::Value root;
            bool success = profiler.stopCPUProfiler();

            root["success"] = success;
            if (success) {
                root["message"] = "CPU profiler stopped successfully";
                auto state = profiler.getProfilerState(profiler::ProfilerType::CPU);
                root["output_path"] = state.output_path;
                root["duration_ms"] = static_cast<Json::Int64>(state.duration);
            } else {
                root["message"] = "Failed to stop CPU profiler (may not be running)";
            }

            auto resp = HttpResponse::newHttpJsonResponse(root);
            callback(resp);
        },
        {Post}
    );

    app().registerHandler(
        "/api/heap/start",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            Json::Value root;
            bool success = profiler.startHeapProfiler();

            root["success"] = success;
            if (success) {
                root["message"] = "Heap profiler started successfully";
                auto state = profiler.getProfilerState(profiler::ProfilerType::HEAP);
                root["output_path"] = state.output_path;
            } else {
                root["message"] = "Failed to start Heap profiler (may already be running)";
            }

            auto resp = HttpResponse::newHttpJsonResponse(root);
            callback(resp);
        },
        {Post}
    );

    app().registerHandler(
        "/api/heap/stop",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            Json::Value root;
            bool success = profiler.stopHeapProfiler();

            root["success"] = success;
            if (success) {
                root["message"] = "Heap profiler stopped successfully";
                auto state = profiler.getProfilerState(profiler::ProfilerType::HEAP);
                root["output_path"] = state.output_path;
                root["duration_ms"] = static_cast<Json::Int64>(state.duration);
            } else {
                root["message"] = "Failed to stop Heap profiler (may not be running)";
            }

            auto resp = HttpResponse::newHttpJsonResponse(root);
            callback(resp);
        },
        {Post}
    );

    app().registerHandler(
        "/",
        [](const HttpRequestPtr& req,
           std::function<void(const HttpResponsePtr&)>&& callback) {
            std::string html = getWebPage();
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(html);
            resp->setContentTypeCode(CT_TEXT_HTML);
            callback(resp);
        },
        {Get}
    );

    // Flame graph viewer page
    app().registerHandler(
        "/flamegraph",
        [](const HttpRequestPtr& req,
           std::function<void(const HttpResponsePtr&)>&& callback) {
            std::ifstream htmlFile("../web/flamegraph.html");
            std::stringstream buffer;
            buffer << htmlFile.rdbuf();
            std::string html = buffer.str();

            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(html);
            resp->setContentTypeCode(CT_TEXT_HTML);
            callback(resp);
        },
        {Get}
    );

    // Browser rendering test page
    app().registerHandler(
        "/test",
        [](const HttpRequestPtr& req,
           std::function<void(const HttpResponsePtr&)>&& callback) {
            std::ifstream htmlFile("../tests/test_browser_rendering.html");
            std::stringstream buffer;
            buffer << htmlFile.rdbuf();
            std::string html = buffer.str();

            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(html);
            resp->setContentTypeCode(CT_TEXT_HTML);
            callback(resp);
        },
        {Get}
    );

    // Debug page
    app().registerHandler(
        "/debug",
        [](const HttpRequestPtr& req,
           std::function<void(const HttpResponsePtr&)>&& callback) {
            std::ifstream htmlFile("../web/debug.html");
            std::stringstream buffer;
            buffer << htmlFile.rdbuf();
            std::string html = buffer.str();

            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(html);
            resp->setContentTypeCode(CT_TEXT_HTML);
            callback(resp);
        },
        {Get}
    );

    // CPU SVG 火焰图
    app().registerHandler(
        "/api/cpu/svg",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            std::string svg = profiler.generateSVGFromProfile("cpu");

            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(svg);
            resp->setContentTypeCode(CT_IMAGE_SVG_XML);
            callback(resp);
        },
        {Get}
    );

    // Heap SVG 火焰图
    app().registerHandler(
        "/api/heap/svg",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            std::string svg = profiler.generateSVGFromProfile("heap");

            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(svg);
            resp->setContentTypeCode(CT_IMAGE_SVG_XML);
            callback(resp);
        },
        {Get}
    );

    // CPU 文本格式
    app().registerHandler(
        "/api/cpu/text",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            auto state = profiler.getProfilerState(profiler::ProfilerType::CPU);
            std::string text = profiler.getSymbolizedProfile(state.output_path);

            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(text);
            resp->setContentTypeCode(CT_TEXT_PLAIN);
            callback(resp);
        },
        {Get}
    );

    // Heap 文本格式
    app().registerHandler(
        "/api/heap/text",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            auto state = profiler.getProfilerState(profiler::ProfilerType::HEAP);
            std::string text = profiler.getSymbolizedProfile(state.output_path);

            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(text);
            resp->setContentTypeCode(CT_TEXT_PLAIN);
            callback(resp);
        },
        {Get}
    );

    // 下载 CPU profile (pprof 格式)
    app().registerHandler(
        "/api/cpu/pprof",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            auto state = profiler.getProfilerState(profiler::ProfilerType::CPU);

            std::ifstream file(state.output_path, std::ios::binary);
            if (!file.is_open()) {
                Json::Value root;
                root["error"] = "Cannot open CPU profile file";
                auto resp = HttpResponse::newHttpJsonResponse(root);
                callback(resp);
                return;
            }

            std::ostringstream oss;
            oss << file.rdbuf();

            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(oss.str());
            resp->setContentTypeCode(CT_APPLICATION_OCTET_STREAM);
            resp->addHeader("Content-Disposition", "attachment; filename=cpu.prof");
            callback(resp);
        },
        {Get}
    );

    // 下载 Heap profile (pprof 格式)
    app().registerHandler(
        "/api/heap/pprof",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            std::string profileData = profiler.getHeapProfileData();

            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(profileData);
            resp->setContentTypeCode(CT_TEXT_PLAIN);
            resp->addHeader("Content-Disposition", "attachment; filename=heap.prof");
            callback(resp);
        },
        {Get}
    );

    // CPU profile JSON (用于前端火焰图渲染)
    app().registerHandler(
        "/api/cpu/json",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            std::string jsonData = profiler.getProfileAsJSON("cpu");

            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(jsonData);
            resp->setContentTypeCode(CT_APPLICATION_JSON);
            callback(resp);
        },
        {Get}
    );

    // Heap profile JSON (用于前端火焰图渲染)
    app().registerHandler(
        "/api/heap/json",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            std::string jsonData = profiler.getProfileAsJSON("heap");

            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(jsonData);
            resp->setContentTypeCode(CT_APPLICATION_JSON);
            callback(resp);
        },
        {Get}
    );

    // CPU flame graph data (层次化的火焰图数据)
    app().registerHandler(
        "/api/cpu/flamegraph",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            std::string jsonData = profiler.getFlameGraphData("cpu");

            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(jsonData);
            resp->setContentTypeCode(CT_APPLICATION_JSON);
            callback(resp);
        },
        {Get}
    );

    // Heap flame graph data
    app().registerHandler(
        "/api/heap/flamegraph",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            std::string jsonData = profiler.getFlameGraphData("heap");

            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(jsonData);
            resp->setContentTypeCode(CT_APPLICATION_JSON);
            callback(resp);
        },
        {Get}
    );

    // Symbol resolution endpoint (类似 brpc pprof 的 /pprof/symbol)
    app().registerHandler(
        "/pprof/symbol",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            // 只支持 POST 请求
            if (req->method() != Post) {
                auto resp = HttpResponse::newHttpResponse();
                resp->setStatusCode(k405MethodNotAllowed);
                resp->setBody("Method not allowed. Use POST.");
                callback(resp);
                return;
            }

            // 从请求体获取地址列表
            std::string bodyStr(req->body());
            std::istringstream iss(bodyStr);
            std::string address;
            std::ostringstream result;

            // 逐行读取地址并解析
            while (std::getline(iss, address)) {
                if (address.empty() || address[0] == '#') {
                    continue;
                }

                // 移除 "0x" 前缀（如果有）
                if (address.size() > 2 && address[0] == '0' && address[1] == 'x') {
                    address = address.substr(2);
                }

                // 使用CPU profile路径解析符号
                auto cpuState = profiler.getProfilerState(profiler::ProfilerType::CPU);
                std::string symbol = profiler.resolveSymbol(cpuState.output_path, address);

                result << address << " " << symbol << "\n";
            }

            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(result.str());
            resp->setContentTypeCode(CT_TEXT_PLAIN);
            callback(resp);
        },
        {Post, Get}
    );

    std::cout << "Handlers registered. Starting Drogon framework...\n";

    // 设置并运行 Drogon 框架
    app().setLogPath("./")
         .setThreadNum(4)
         .setDocumentRoot("./")
         .addListener(host, port)
         .run();

    return 0;
}
