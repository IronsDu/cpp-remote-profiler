#include <drogon/drogon.h>
#include "profiler_manager.h"
#include <thread>
#include <chrono>
#include <iostream>
#include <vector>
#include <random>
#include <sstream>
#include <fstream>
#include <list>
#include <map>
#include <algorithm>

using namespace drogon;

// 更复杂的函数调用链，用于生成详细的火焰图

class DataProcessor {
public:
    void sortData(std::vector<int>& data) {
        std::sort(data.begin(), data.end());
    }

    void reverseData(std::vector<int>& data) {
        std::reverse(data.begin(), data.end());
    }

    void shuffleData(std::vector<int>& data) {
        std::shuffle(data.begin(), data.end(), std::mt19937(std::random_device()()));
    }

    void processData(std::vector<int>& data) {
        sortData(data);
        reverseData(data);
        shuffleData(data);
        sortData(data); // 再次排序
    }
};

class FibonacciCalculator {
public:
    int recursive(int n) {
        if (n <= 1) return n;
        return recursive(n - 1) + recursive(n - 2);
    }

    int iterative(int n) {
        if (n <= 1) return n;
        int a = 0, b = 1;
        for (int i = 2; i <= n; ++i) {
            int temp = a + b;
            a = b;
            b = temp;
        }
        return b;
    }

    int memoized(int n, std::map<int, int>& cache) {
        if (n <= 1) return n;
        if (cache.find(n) != cache.end()) return cache[n];
        cache[n] = memoized(n - 1, cache) + memoized(n - 2, cache);
        return cache[n];
    }
};

class MatrixOperations {
public:
    std::vector<std::vector<int>> createMatrix(int rows, int cols) {
        std::vector<std::vector<int>> matrix;
        for (int i = 0; i < rows; ++i) {
            std::vector<int> row;
            for (int j = 0; j < cols; ++j) {
                row.push_back(rand() % 1000);
            }
            matrix.push_back(row);
        }
        return matrix;
    }

    std::vector<std::vector<int>> transposeMatrix(const std::vector<std::vector<int>>& matrix) {
        std::vector<std::vector<int>> result;
        for (size_t j = 0; j < matrix[0].size(); ++j) {
            std::vector<int> row;
            for (size_t i = 0; i < matrix.size(); ++i) {
                row.push_back(matrix[i][j]);
            }
            result.push_back(row);
        }
        return result;
    }

    std::vector<std::vector<int>> multiplyMatrices(const std::vector<std::vector<int>>& a,
                                                   const std::vector<std::vector<int>>& b) {
        std::vector<std::vector<int>> result(a.size(), std::vector<int>(b[0].size(), 0));
        for (size_t i = 0; i < a.size(); ++i) {
            for (size_t j = 0; j < b[0].size(); ++j) {
                for (size_t k = 0; k < b.size(); ++k) {
                    result[i][j] += a[i][k] * b[k][j];
                }
            }
        }
        return result;
    }
};

class HashCalculator {
public:
    size_t simpleHash(const std::string& str) {
        size_t hash = 0;
        for (char c : str) {
            hash = hash * 31 + c;
        }
        return hash;
    }

    std::map<std::string, size_t> batchHash(const std::vector<std::string>& strings) {
        std::map<std::string, size_t> results;
        for (const auto& str : strings) {
            results[str] = simpleHash(str);
        }
        return results;
    }

    std::vector<size_t> parallelHash(const std::vector<std::string>& strings) {
        std::vector<size_t> hashes;
        for (const auto& str : strings) {
            hashes.push_back(simpleHash(str));
        }
        return hashes;
    }
};

// 模拟CPU密集型任务 - 复杂调用链
void cpuIntensiveTask() {
    DataProcessor processor;
    FibonacciCalculator fib;
    MatrixOperations matrixOps;
    HashCalculator hashCalc;

    for (int i = 0; i < 100; ++i) {
        // 1. 数据处理分支
        std::vector<int> data(1000);
        for (auto& val : data) {
            val = rand();
        }

        processor.processData(data);

        // 2. Fibonacci计算分支
        volatile int result1 = fib.recursive(25);
        volatile int result2 = fib.iterative(30);
        (void)result1; (void)result2;

        // 3. 矩阵运算分支
        auto matrix1 = matrixOps.createMatrix(10, 10);
        auto matrix2 = matrixOps.createMatrix(10, 10);
        auto transposed = matrixOps.transposeMatrix(matrix1);
        auto multiplied = matrixOps.multiplyMatrices(matrix1, matrix2);

        // 4. Hash计算分支
        std::vector<std::string> strings = {"hello", "world", "test", "data"};
        auto hashes = hashCalc.parallelHash(strings);
        auto batchHashes = hashCalc.batchHash(strings);

        // 5. 递归计算
        std::map<int, int> cache;
        volatile int result3 = fib.memoized(20, cache);
        (void)result3;

        // 防止编译器优化
        volatile int sink = data[0] + result1 + result2 + result3 + hashes[0];
        (void)sink;
        (void)transposed;
        (void)multiplied;
        (void)batchHashes;
    }
}

// 模拟内存密集型任务 - 复杂分配模式
void memoryIntensiveTask() {
    // 使用各种内存分配模式
    std::vector<std::vector<int>> matrixData;
    std::vector<std::string> stringData;
    std::map<int, std::vector<int>> mapData;
    std::vector<std::list<int>> listData;

    for (int i = 0; i < 50; ++i) {
        // 1. 大数组分配
        std::vector<int> largeArray(10000);
        for (auto& val : largeArray) {
            val = rand();
        }

        // 2. 矩阵分配
        for (int j = 0; j < 20; ++j) {
            std::vector<int> row(1000);
            for (auto& val : row) {
                val = rand();
            }
            matrixData.push_back(row);
        }

        // 3. 字符串分配
        std::vector<std::string> strings;
        for (int j = 0; j < 100; ++j) {
            strings.push_back("Test string data " + std::to_string(j));
        }
        stringData.insert(stringData.end(), strings.begin(), strings.end());

        // 4. Map分配
        for (int j = 0; j < 10; ++j) {
            std::vector<int> values(100);
            for (auto& val : values) {
                val = j * 10 + rand() % 100;
            }
            mapData[j] = values;
        }

        // 5. 动态分配
        int* dynamicArray = new int[1000];
        for (int k = 0; k < 1000; ++k) {
            dynamicArray[k] = rand();
        }
        delete[] dynamicArray;

        // 6. 小对象频繁分配
        std::list<int> smallList;
        for (int k = 0; k < 200; ++k) {
            smallList.push_back(rand());
        }
        listData.push_back(smallList);
    }

    // 模拟内存泄漏（故意不释放）
    for (int i = 0; i < 10; ++i) {
        auto* leak = new int[5000];
        for (int j = 0; j < 5000; ++j) {
            leak[j] = i;
        }
        (void)leak; // 故意泄漏
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

                // 检查是否有 duration 参数
                auto duration_param = req->getParameter("duration");
                if (!duration_param.empty()) {
                    try {
                        int duration_sec = std::stoi(duration_param);
                        int duration_ms = duration_sec * 1000;
                        root["duration_ms"] = duration_ms;

                        // 在新线程中定时停止 profiler
                        std::thread([duration_ms]() {
                            std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
                            profiler::ProfilerManager::getInstance().stopCPUProfiler();
                        }).detach();
                    } catch (const std::exception& e) {
                        root["warning"] = "Invalid duration parameter, profiler will run until manually stopped";
                    }
                }
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

                // 检查是否有 duration 参数
                auto duration_param = req->getParameter("duration");
                if (!duration_param.empty()) {
                    try {
                        int duration_ms = std::stoi(duration_param) * 1000;
                        root["duration_ms"] = duration_ms;

                        // 在新线程中定时停止 profiler
                        std::thread([duration_ms, &profiler]() {
                            std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
                            profiler.stopHeapProfiler();
                        }).detach();
                    } catch (const std::exception& e) {
                        root["warning"] = "Invalid duration parameter, profiler will run until manually stopped";
                    }
                }
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

    // CPU profile (raw protobuf prof file for frontend)
    app().registerHandler(
        "/api/cpu/profile",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            auto state = profiler.getProfilerState(profiler::ProfilerType::CPU);

            std::ifstream file(state.output_path, std::ios::binary);
            if (!file.is_open()) {
                Json::Value root;
                root["error"] = "Cannot open CPU profile file: " + state.output_path;
                auto resp = HttpResponse::newHttpJsonResponse(root);
                resp->setStatusCode(k404NotFound);
                callback(resp);
                return;
            }

            std::ostringstream oss;
            oss << file.rdbuf();

            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(oss.str());
            resp->setContentTypeCode(CT_APPLICATION_OCTET_STREAM);
            callback(resp);
        },
        {Get}
    );

    // Heap profile (raw prof file for frontend)
    app().registerHandler(
        "/api/heap/profile",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            auto heapState = profiler.getProfilerState(profiler::ProfilerType::HEAP);
            std::ifstream file(heapState.output_path);
            if (!file.is_open()) {
                Json::Value root;
                root["error"] = "Cannot open heap profile file: " + heapState.output_path;
                auto resp = HttpResponse::newHttpJsonResponse(root);
                resp->setStatusCode(k404NotFound);
                callback(resp);
                return;
            }

            std::stringstream buffer;
            buffer << file.rdbuf();

            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(buffer.str());
            resp->setContentTypeCode(CT_TEXT_PLAIN);
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

    // CPU collapsed stacks for flame graph (deprecated)
    app().registerHandler(
        "/api/cpu/collapsed",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            std::string collapsedData = profiler.getCollapsedStacks("cpu");

            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(collapsedData);
            resp->setContentTypeCode(CT_TEXT_PLAIN);
            callback(resp);
        },
        {Get}
    );

    // CPU profile address stacks (for new architecture)
    app().registerHandler(
        "/api/cpu/addresses",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            std::string addressData = profiler.getCPUProfileAddresses();

            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(addressData);
            resp->setContentTypeCode(CT_TEXT_PLAIN);
            callback(resp);
        },
        {Get}
    );

    // Heap collapsed stacks for flame graph
    app().registerHandler(
        "/api/heap/collapsed",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            std::string collapsedData = profiler.getCollapsedStacks("heap");

            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(collapsedData);
            resp->setContentTypeCode(CT_TEXT_PLAIN);
            callback(resp);
        },
        {Get}
    );

    // Debug endpoint - 直接返回heap.prof文件内容
    app().registerHandler(
        "/debug/heap/raw",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            auto heapState = profiler.getProfilerState(profiler::ProfilerType::HEAP);
            std::ifstream file(heapState.output_path);
            std::stringstream buffer;
            buffer << file.rdbuf();

            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(buffer.str());
            resp->setContentTypeCode(CT_TEXT_PLAIN);
            callback(resp);
        },
        {Get}
    );

    // CPU flame graph data (层次化的火焰图数据) - 保持兼容
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

    // Heap flame graph data - 保持兼容
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

    // CPU text profile (for debugging)
    app().registerHandler(
        "/api/cpu/text",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            std::string collapsedData = profiler.getProfileSamples("cpu");

            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(collapsedData);
            resp->setContentTypeCode(CT_TEXT_PLAIN);
            callback(resp);
        },
        {Get}
    );

    // Heap text profile (for debugging)
    app().registerHandler(
        "/api/heap/text",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            std::string collapsedData = profiler.getProfileSamples("heap");

            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(collapsedData);
            resp->setContentTypeCode(CT_TEXT_PLAIN);
            callback(resp);
        },
        {Get}
    );

    // CPU profile samples (addresses for frontend rendering)
    app().registerHandler(
        "/api/cpu/samples",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            std::string jsonData = profiler.getProfileSamples("cpu");

            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(jsonData);
            resp->setContentTypeCode(CT_APPLICATION_JSON);
            callback(resp);
        },
        {Get}
    );

    // Heap profile samples
    app().registerHandler(
        "/api/heap/samples",
        [&profiler](const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback) {
            std::string jsonData = profiler.getProfileSamples("heap");

            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(jsonData);
            resp->setContentTypeCode(CT_APPLICATION_JSON);
            callback(resp);
        },
        {Get}
    );

    // Symbol resolution endpoint (类似 brpc pprof 的 /pprof/symbol)
    // 使用 backward-cpp 进行符号化，支持内联函数
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

            // 逐行读取地址并使用 backward-cpp 符号化
            while (std::getline(iss, address)) {
                if (address.empty() || address[0] == '#') {
                    continue;
                }

                // 移除 "0x" 前缀（如果有）
                std::string original_addr = address;
                if (address.size() > 2 && address[0] == '0' && address[1] == 'x') {
                    address = address.substr(2);
                }

                // 将十六进制地址转换为指针
                try {
                    uintptr_t addr = std::stoull(address, nullptr, 16);
                    void* ptr = reinterpret_cast<void*>(addr);

                    // 使用 backward-cpp 符号化（支持内联函数）
                    std::string symbol = profiler.resolveSymbolWithBackward(ptr);

                    // 返回格式: "原始地址 符号化结果"
                    result << original_addr << " " << symbol << "\n";
                } catch (const std::exception& e) {
                    // 转换失败，返回原始地址
                    result << original_addr << " " << original_addr << "\n";
                }
            }

            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(result.str());
            resp->setContentTypeCode(CT_TEXT_PLAIN);
            callback(resp);
        },
        {Post}
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
