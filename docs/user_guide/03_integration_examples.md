# 集成示例

本文档提供了各种实际场景下的集成示例。

## 目录
- [场景 1: 仅使用核心 profiling 功能](#场景-1-仅使用核心-profiling-功能)
- [场景 2: 集成 Web 界面](#场景-2-集成-web-界面)
- [场景 3: 与现有 HTTP 服务器集成](#场景-3-与现有-http-服务器集成)
- [场景 4: 定时 profiling](#场景-4-定时-profiling)
- [场景 5: 条件触发 profiling](#场景-5-条件触发-profiling)
- [场景 6: 多进程 profiling](#场景-6-多进程-profiling)

---

## 场景 1: 仅使用核心 profiling 功能

**适用场景**: 命令行工具、批处理程序、不需要 Web 界面的应用

### 示例代码

```cpp
// simple_profiling_app.cpp
#include "profiler_manager.h"
#include <iostream>
#include <thread>
#include <chrono>

class MyApplication {
public:
    void run() {
        auto& profiler = profiler::ProfilerManager::getInstance();

        std::cout << "应用启动" << std::endl;

        // 启动 CPU profiling
        profiler.startCPUProfiler("app_profile.prof");

        // 运行主逻辑
        processTasks();

        // 停止 profiling
        profiler.stopCPUProfiler();

        std::cout << "Profiling 完成，文件: app_profile.prof" << std::endl;
        std::cout << "使用以下命令查看:" << std::endl;
        std::cout << "  go tool pprof -http=:8080 app_profile.prof" << std::endl;
    }

private:
    void processTasks() {
        for (int i = 0; i < 100; i++) {
            doHeavyComputation();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void doHeavyComputation() {
        double result = 0;
        for (int i = 0; i < 100000; i++) {
            result += std::sqrt(i);
        }
    }
};

int main() {
    MyApplication app;
    app.run();
    return 0;
}
```

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.15)
project(SimpleProfilingApp)

set(CMAKE_CXX_STANDARD 20)

# 找到 profiler 库
find_package(PkgConfig REQUIRED)
pkg_check_modules(GPERFTOOLS REQUIRED libprofiler libtcmalloc)

add_executable(simple_app simple_profiling_app.cpp)

target_include_directories(simple_app PRIVATE
    /path/to/cpp-remote-profiler/include
)

target_link_libraries(simple_app
    profiler_lib
    ${GPERFTOOLS_LIBRARIES}
    pthread
)
```

### 编译和运行

```bash
mkdir build && cd build
cmake ..
make
./simple_app
```

---

## 场景 2: 集成 Web 界面

**适用场景**: 需要远程监控、可视化查看 profiling 结果

### 示例代码

```cpp
// web_profiling_server.cpp
#include <drogon/drogon.h>
#include "profiler_manager.h"
#include "web_server.h"
#include <iostream>

int main() {
    std::cout << "=== C++ Remote Profiler Server ===" << std::endl;

    // 获取 profiler 实例
    profiler::ProfilerManager& profiler = profiler::ProfilerManager::getInstance();

    // 注册所有 profiling 相关的 HTTP 端点
    profiler::registerHttpHandlers(profiler);

    // 配置 Drogon 服务器
    drogon::app().addListener("0.0.0.0", 8080);
    drogon::app().setThreadNum(4);
    drogon::app().setDocumentRoot("./");

    std::cout << "服务器启动成功！" << std::endl;
    std::cout << "Web 界面: http://localhost:8080" << std::endl;
    std::cout << "API 端点:" << std::endl;
    std::cout << "  - GET /pprof/profile?seconds=N" << std::endl;
    std::cout << "  - GET /pprof/heap" << std::endl;
    std::cout << "  - GET /api/cpu/analyze?duration=N" << std::endl;
    std::cout << "  - GET /api/heap/analyze" << std::endl;
    std::cout << "  - GET /api/thread/stacks" << std::endl;

    // 启动服务器（阻塞）
    drogon::app().run();

    return 0;
}
```

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.15)
project(WebProfilingServer)

set(CMAKE_CXX_STANDARD 20)

find_package(Drogon CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GPERFTOOLS REQUIRED libprofiler libtcmalloc)

add_executable(web_server web_profiling_server.cpp)

target_link_libraries(web_server
    profiler_lib
    Drogon::Drogon
    ${GPERFTOOLS_LIBRARIES}
)
```

### 使用方法

1. 编译并运行服务器
2. 浏览器访问 `http://localhost:8080`
3. 点击按钮进行 profiling，查看火焰图

---

## 场景 3: 与现有 HTTP 服务器集成

**适用场景**: 已有 Drogon 服务器，想添加 profiling 功能

### 示例代码

```cpp
// existing_server_with_profiler.cpp
#include <drogon/drogon.h>
#include "profiler_manager.h"
#include "web_server.h"

// 你的业务逻辑处理器
void businessHandler(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setBody("Business logic response");
    callback(resp);
}

int main() {
    // 1. 初始化 profiler
    profiler::ProfilerManager& profiler = profiler::ProfilerManager::getInstance();
    std::cout << "Profiler 初始化完成" << std::endl;

    // 2. 注册 profiling 端点
    profiler::registerHttpHandlers(profiler);

    // 3. 注册你自己的业务端点
    drogon::app().registerHandler(
        "/api/business",
        &businessHandler,
        {drogon::Get}
    );

    // 4. 启动服务器
    drogon::app().addListener("0.0.0.0", 8080);
    std::cout << "服务器启动: http://localhost:8080" << std::endl;
    std::cout << "Business API: http://localhost:8080/api/business" << std::endl;
    std::cout << "Profiler UI: http://localhost:8080/" << std::endl;

    drogon::app().run();

    return 0;
}
```

### 关键点

- Profiling 端点和业务端点共存
- 可以选择性启用 profiling（例如：只在开发环境）
- 不影响现有业务逻辑

---

## 场景 4: 定时 profiling

**适用场景**: 长期运行的服务，定期采集性能数据

### 示例代码

```cpp
// scheduled_profiling_app.cpp
#include "profiler_manager.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <ctime>

class ScheduledProfiler {
public:
    ScheduledProfiler(int interval_seconds)
        : interval_seconds_(interval_seconds),
          running_(false) {}

    void start() {
        running_ = true;
        profiler_thread_ = std::thread(&ScheduledProfiler::profilingLoop, this);
    }

    void stop() {
        running_ = false;
        if (profiler_thread_.joinable()) {
            profiler_thread_.join();
        }
    }

private:
    void profilingLoop() {
        auto& profiler = profiler::ProfilerManager::getInstance();
        int profile_count = 0;

        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(interval_seconds_));

            profile_count++;

            // 生成带时间戳的文件名
            time_t now = time(nullptr);
            char filename[256];
            strftime(filename, sizeof(filename),
                    "profile_%Y%m%d_%H%M%S.prof",
                    localtime(&now));

            std::cout << "[" << profile_count << "] 开始 profiling..." << std::endl;

            // 采样 30 秒
            profiler.startCPUProfiler(filename);
            std::this_thread::sleep_for(std::chrono::seconds(30));
            profiler.stopCPUProfiler();

            std::cout << "[" << profile_count << "] 完成: " << filename << std::endl;
        }
    }

    int interval_seconds_;
    std::atomic<bool> running_;
    std::thread profiler_thread_;
};

int main() {
    std::cout << "定时 Profiling 应用" << std::endl;
    std::cout << "每 5 分钟采样一次，每次采样 30 秒" << std::endl;

    ScheduledProfiler profiler(300); // 每 300 秒（5 分钟）
    profiler.start();

    std::cout << "按 Ctrl+C 退出..." << std::endl;

    // 主线程等待
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    profiler.stop();
    return 0;
}
```

### 使用场景

- 生产环境性能监控
- 长时间运行任务的性能跟踪
- 自动化性能测试

---

## 场景 5: 条件触发 profiling

**适用场景**: 检测到性能问题时自动启用 profiling

### 示例代码

```cpp
// conditional_profiling_app.cpp
#include "profiler_manager.h"
#include <iostream>
#include <thread>
#include <chrono>

class PerformanceMonitor {
public:
    void checkPerformance() {
        auto& profiler = profiler::ProfilerManager::getInstance();

        while (running_) {
            auto start = std::chrono::high_resolution_clock::now();

            // 执行任务
            doTask();

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

            // 检查是否超时
            if (duration.count() > threshold_ms_) {
                std::cout << "⚠️  检测到性能问题: " << duration.count() << "ms" << std::endl;

                if (!profiler.isProfilerRunning(profiler::ProfilerType::CPU)) {
                    std::cout << "自动启动 CPU profiling..." << std::endl;

                    // 自动启动 profiling
                    profiler.startCPUProfiler("auto_profile.prof");
                    std::this_thread::sleep_for(std::chrono::seconds(10));
                    profiler.stopCPUProfiler();

                    std::cout << "Profiling 完成，请查看 auto_profile.prof" << std::endl;
                }
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    void start() { running_ = true; }
    void stop() { running_ = false; }

private:
    void doTask() {
        // 模拟任务
        double result = 0;
        for (int i = 0; i < 1000000; i++) {
            result += std::sqrt(i);
        }
    }

    int threshold_ms_ = 100; // 100ms 阈值
    std::atomic<bool> running_{false};
};

int main() {
    std::cout << "性能监控应用（阈值: 100ms）" << std::endl;

    PerformanceMonitor monitor;
    monitor.start();

    // 运行监控
    std::this_thread::sleep_for(std::chrono::minutes(5));

    monitor.stop();

    return 0;
}
```

### 使用场景

- 自动化性能问题检测
- 生产环境问题诊断
- 智能监控

---

## 场景 6: 多进程 profiling

**适用场景**: 微服务架构、多进程应用

### 架构设计

```
┌─────────────────────────────────────────────────┐
│           Nginx / Reverse Proxy                 │
└─────────────────────────────────────────────────┘
           │         │         │
           ▼         ▼         ▼
    ┌──────────┐ ┌──────────┐ ┌──────────┐
    │ Service 1│ │ Service 2│ │ Service 3│
    │ :8081    │ │ :8082    │ │ :8083    │
    └──────────┘ └──────────┘ └──────────┘
```

### 示例代码

```cpp
// service.cpp
#include <drogon/drogon.h>
#include "profiler_manager.h"
#include "web_server.h"
#include <iostream>
#include <cstdlib>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "用法: " << argv[0] << " <port>" << std::endl;
        return 1;
    }

    int port = std::atoi(argv[1]);

    std::cout << "Service 启动在端口: " << port << std::endl;

    // 初始化 profiler
    profiler::ProfilerManager& profiler = profiler::ProfilerManager::getInstance();
    profiler::registerHttpHandlers(profiler);

    // 业务逻辑处理器
    auto handler = [](const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setBody("Service on port " + req->getParameter("port"));
        callback(resp);
    };

    drogon::app().registerHandler("/api/service", handler);

    // 监听指定端口
    drogon::app().addListener("0.0.0.0", port);
    drogon::app().run();

    return 0;
}
```

### 启动脚本

```bash
#!/bin/bash
# start_services.sh

echo "启动多个服务实例..."

# 启动 3 个服务实例
./service 8081 &
PID1=$!

./service 8082 &
PID2=$!

./service 8083 &
PID3=$!

echo "服务启动完成:"
echo "  Service 1: http://localhost:8081"
echo "  Service 2: http://localhost:8082"
echo "  Service 3: http://localhost:8083"

echo "按 Ctrl+C 停止所有服务"

# 等待中断信号
trap "kill $PID1 $PID2 $PID3; exit" INT TERM

wait
```

### 使用方法

```bash
# 启动所有服务
./start_services.sh

# 分别查看每个服务的 profiling
curl http://localhost:8081/api/cpu/analyze?duration=10
curl http://localhost:8082/api/cpu/analyze?duration=10
curl http://localhost:8083/api/cpu/analyze?duration=10
```

---

## 完整示例：集成到游戏服务器

```cpp
// game_server.cpp
#include "profiler_manager.h"
#include <iostream>
#include <thread>
#include <map>

class GameServer {
public:
    void start() {
        std::cout << "游戏服务器启动" << std::endl;

        // 初始化 profiler
        auto& profiler = profiler::ProfilerManager::getInstance();

        // 可以选择性地启用 profiling（例如：通过配置文件）
        if (enableProfiling_) {
            std::cout << "Profiling 已启用" << std::endl;
            profiler.startCPUProfiler("game_server.prof");
        }

        // 启动游戏循环
        gameLoop();
    }

    void stop() {
        auto& profiler = profiler::ProfilerManager::getInstance();

        if (enableProfiling_) {
            profiler.stopCPUProfiler();
            std::cout << "Profiling 已保存" << std::endl;
        }
    }

    void setProfilingEnabled(bool enabled) {
        enableProfiling_ = enabled;
    }

private:
    void gameLoop() {
        while (running_) {
            processInput();
            updateGameLogic();
            render();

            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
        }
    }

    void processInput() {
        // 处理输入
    }

    void updateGameLogic() {
        // 更新游戏逻辑
    }

    void render() {
        // 渲染
    }

    bool running_ = true;
    bool enableProfiling_ = false;
};

int main() {
    GameServer server;

    // 从配置或命令行参数读取是否启用 profiling
    server.setProfilingEnabled(true);

    server.start();

    return 0;
}
```

---

## 最佳实践

### 1. 环境隔离

```cpp
// 开发环境启用 profiling
#ifdef DEBUG_MODE
    profiler.startCPUProfiler("debug.prof");
#else
    // 生产环境默认不启用
#endif
```

### 2. 配置驱动

```cpp
// 从配置文件读取
if (config.getBool("profiling.enabled", false)) {
    int duration = config.getInt("profiling.duration", 10);
    profiler.analyzeCPUProfile(duration);
}
```

### 3. 资源管理

```cpp
// 使用 RAII 管理 profiler
class ScopedProfiler {
public:
    ScopedProfiler(const std::string& name) : name_(name) {
        profiler::ProfilerManager::getInstance().startCPUProfiler(name_);
    }

    ~ScopedProfiler() {
        profiler::ProfilerManager::getInstance().stopCPUProfiler();
    }

private:
    std::string name_;
};

// 使用
{
    ScopedProfiler profiler("critical_section.prof");
    // 这里的代码会被 profiling
} // 自动停止
```

### 4. 线程安全

```cpp
// 所有 API 都是线程安全的
void thread1() {
    profiler.startCPUProfiler("thread1.prof");
}

void thread2() {
    profiler.analyzeCPUProfile(10);
}

// 两个线程可以安全地同时调用
```

---

## 性能建议

1. **降低开销**:
   - 仅在需要时启用 profiling
   - 使用较短的采样时长
   - 避免频繁启动/停止

2. **存储管理**:
   - 定期清理旧的 profile 文件
   - 使用压缩存储历史数据
   - 设置磁盘空间监控

3. **生产环境**:
   - 使用条件触发而非持续 profiling
   - 设置 profiling 开销阈值
   - 监控 profiling 对性能的影响

---

## 更多信息

- 📖 [API 参考手册](02_api_reference.md)
- 🔧 [故障排除指南](04_troubleshooting.md)
- 🏠 [返回主文档](../README.md)
