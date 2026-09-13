# 集成示例

本文档提供了各种实际场景下的集成示例。

## 目录
- [场景 1: 仅使用核心 profiling 功能](#场景-1-仅使用核心-profiling-功能)
- [场景 2: 使用 Drogon Web 界面](#场景-2-使用-drogon-web-界面)
- [场景 3: 与任意 Web 框架集成](#场景-3-与任意-web-框架集成)
- [场景 4: 定时 profiling](#场景-4-定时-profiling)
- [场景 5: 条件触发 profiling](#场景-5-条件触发-profiling)

---

## 场景 1: 仅使用核心 profiling 功能

**适用场景**: 命令行工具、批处理程序、不需要 Web 界面的应用

**CMake 配置**:
```cmake
find_package(cpp-remote-profiler REQUIRED)
target_link_libraries(my_app cpp-remote-profiler::profiler_core)
```

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
        profiler::ProfilerManager profiler;

        std::cout << "应用启动" << std::endl;

        // 启动 CPU profiling
        profiler.startCPUProfiler("app_profile.prof");

        // 运行主逻辑
        processTasks();

        // 停止 profiling
        profiler.stopCPUProfiler();

        std::cout << "Profiling 完成，文件: app_profile.prof" << std::endl;
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

---

## 场景 2: 使用 Drogon Web 界面

**适用场景**: 需要远程监控、可视化查看 profiling 结果，且使用 Drogon 框架

**CMake 配置**:
```cmake
find_package(cpp-remote-profiler REQUIRED)
find_package(Drogon CONFIG REQUIRED)
target_link_libraries(my_app
    cpp-remote-profiler::profiler_web
    Drogon::Drogon
)
```

### 示例代码

```cpp
// web_profiling_server.cpp
#include "profiler_manager.h"
#include "profiler/drogon_adapter.h"
#include <drogon/drogon.h>
#include <iostream>

int main() {
    std::cout << "=== C++ Remote Profiler Server ===" << std::endl;

    // 创建 ProfilerManager 实例
    profiler::ProfilerManager profiler;

    // 注册所有 profiling 相关的 HTTP 端点到 Drogon
    profiler::registerDrogonHandlers(profiler);

    // 配置 Drogon 服务器
    drogon::app().addListener("0.0.0.0", 8080);

    std::cout << "服务器启动成功！" << std::endl;
    std::cout << "Web 界面: http://localhost:8080" << std::endl;

    // 启动服务器（阻塞）
    drogon::app().run();

    return 0;
}
```

### 与现有 Drogon 服务器集成

```cpp
#include "profiler_manager.h"
#include "profiler/drogon_adapter.h"
#include <drogon/drogon.h>

int main() {
    profiler::ProfilerManager profiler;

    // 注册 profiling 端点
    profiler::registerDrogonHandlers(profiler);

    // 注册你自己的业务端点
    drogon::app().registerHandler(
        "/api/business",
        [](const drogon::HttpRequestPtr& req,
           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setBody("Business logic response");
            callback(resp);
        },
        {drogon::Get}
    );

    drogon::app().addListener("0.0.0.0", 8080).run();
}
```

---

## 场景 3: 与任意 Web 框架集成

**适用场景**: 已有非 Drogon 的 Web 服务器（如 oat++、crow、 Pistache 等），想添加 profiling 功能

**CMake 配置**:
```cmake
find_package(cpp-remote-profiler REQUIRED)
target_link_libraries(my_app
    cpp-remote-profiler::profiler_core   # 不需要 profiler_web
    your-web-framework
)
```

### 核心思路

`ProfilerHttpHandlers` 提供框架无关的 handler 方法，每个方法返回 `HandlerResponse` 结构体。你只需要将 `HandlerResponse` 包装到你框架的 response 对象中。

### 示例：伪代码集成

```cpp
#include "profiler_manager.h"
#include "profiler/http_handlers.h"
#include "your_web_framework.h"  // 你的 Web 框架头文件

int main() {
    profiler::ProfilerManager profiler;
    profiler::ProfilerHttpHandlers handlers(profiler);

    YourWebServer server;

    // 注册 CPU 分析端点
    server.route("GET", "/api/cpu/analyze", [&](const Request& req) {
        int duration = req.get_param_int("duration", 10);
        std::string output_type = req.get_param("output_type", "pprof");

        // 调用 handler 获取框架无关的响应
        profiler::HandlerResponse resp = handlers.handleCpuAnalyze(duration, output_type);

        // 用你的框架包装响应
        return YourResponse()
            .status(resp.status)
            .header("Content-Type", resp.content_type)
            .body(resp.body);
    });

    // 注册 pprof 兼容端点
    server.route("GET", "/pprof/profile", [&](const Request& req) {
        int seconds = req.get_param_int("seconds", 30);
        profiler::HandlerResponse resp = handlers.handlePprofProfile(seconds);
        return YourResponse()
            .status(resp.status)
            .header("Content-Type", resp.content_type)
            .body(resp.body);
    });

    // 注册状态端点
    server.route("GET", "/api/status", [&]() {
        profiler::HandlerResponse resp = handlers.handleStatus();
        return YourResponse()
            .status(resp.status)
            .header("Content-Type", resp.content_type)
            .body(resp.body);
    });

    // ... 注册其他端点

    server.listen(8080);
}
```

### 示例：与 oat++ 集成

> ⚠️ 下面只是**思路示意**，不是可直接编译的代码。真实接入请用 oat++ 的 `ENDPOINT`
> 宏注册路由，并用 `request->getPathTail()` / `getQueryParameter()` 取路径与参数——
> HTTP 请求里**没有**名为 `PATH` 的请求头。

核心做法是给每个端点写一个薄适配函数：

```cpp
#include "profiler_manager.h"
#include "profiler/http_handlers.h"
#include "oatpp/web/server/api/ApiController.hpp"

class ProfilerController : public oatpp::web::server::api::ApiController {
    profiler::ProfilerManager profiler_;
    profiler::ProfilerHttpHandlers handlers_;

public:
    ProfilerController(const std::shared_ptr<ObjectMapper>& objectMapper)
        : ApiController(objectMapper), handlers_(profiler_) {}

    // 把框架无关的 HandlerResponse 转成 oat++ 的响应
    std::shared_ptr<OutgoingResponse> toOat(const profiler::HandlerResponse& resp) {
        auto response = createResponse(Status(resp.status), resp.body);
        response->putHeader("Content-Type", resp.content_type.c_str());
        for (const auto& [k, v] : resp.headers) {
            response->putHeader(k.c_str(), v.c_str());
        }
        return response;
    }

    ENDPOINT("GET", "/api/status", status) {
        return toOat(handlers_.handleStatus());
    }

    ENDPOINT("GET", "/api/cpu/analyze", cpuAnalyze,
             QUERY(Int32, duration, "duration", "10"),
             QUERY(String, output_type, "output_type", "flamegraph")) {
        return toOat(handlers_.handleCpuAnalyze(duration, output_type));
    }

    ENDPOINT("GET", "/pprof/profile", pprofProfile,
             QUERY(Int32, seconds, "seconds", "30")) {
        return toOat(handlers_.handlePprofProfile(seconds));
    }

    // 也可以用 dispatch() 统一按路径 + 查询参数分发，避免逐个写端点
    // auto resp = handlers_.dispatch(method, path, params, body);
};
```

### 关键点

- `ProfilerHttpHandlers` 不依赖任何 Web 框架
- 只需链接 `profiler_core`（不需要 Drogon）
- 每个 handler 方法返回 `HandlerResponse`（`status`, `content_type`, `body`, `headers`）
- 你负责从请求中提取参数、调用 handler、包装响应；`resp.headers` 也要一并转发（下载类接口依赖 `Content-Disposition`）
- `dispatch(method, path, params, body)` 可按路径一次性分发，认不出的路径返回 404

---

## 场景 4: 定时 profiling

**适用场景**: 长期运行的服务，定期采集性能数据

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
        : interval_seconds_(interval_seconds), running_(false) {}

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
        profiler::ProfilerManager profiler;
        int profile_count = 0;

        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(interval_seconds_));

            profile_count++;

            time_t now = time(nullptr);
            char filename[256];
            strftime(filename, sizeof(filename),
                    "profile_%Y%m%d_%H%M%S.prof",
                    localtime(&now));

            std::cout << "[" << profile_count << "] 开始 profiling..." << std::endl;

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
    ScheduledProfiler profiler(300); // 每 5 分钟
    profiler.start();

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    profiler.stop();
    return 0;
}
```

---

## 场景 5: 条件触发 profiling

**适用场景**: 检测到性能问题时自动启用 profiling

```cpp
// conditional_profiling_app.cpp
#include "profiler_manager.h"
#include <iostream>
#include <thread>
#include <chrono>

class PerformanceMonitor {
public:
    void checkPerformance() {
        profiler::ProfilerManager profiler;

        while (running_) {
            auto start = std::chrono::high_resolution_clock::now();
            doTask();
            auto end = std::chrono::high_resolution_clock::now();

            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

            if (duration.count() > threshold_ms_) {
                std::cout << "检测到性能问题: " << duration.count() << "ms" << std::endl;

                if (!profiler.isProfilerRunning(profiler::ProfilerType::CPU)) {
                    profiler.startCPUProfiler("auto_profile.prof");
                    std::this_thread::sleep_for(std::chrono::seconds(10));
                    profiler.stopCPUProfiler();
                    std::cout << "Profiling 完成" << std::endl;
                }
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    void start() { running_ = true; }
    void stop() { running_ = false; }

private:
    void doTask() {
        double result = 0;
        for (int i = 0; i < 1000000; i++) {
            result += std::sqrt(i);
        }
    }

    int threshold_ms_ = 100;
    std::atomic<bool> running_{false};
};

int main() {
    PerformanceMonitor monitor;
    monitor.start();
    std::this_thread::sleep_for(std::chrono::minutes(5));
    monitor.stop();
    return 0;
}
```

---

## 最佳实践

### 1. RAII 管理 profiler

```cpp
class ScopedProfiler {
public:
    ScopedProfiler(profiler::ProfilerManager& profiler, const std::string& name)
        : profiler_(profiler), name_(name) {
        profiler_.startCPUProfiler(name_);
    }

    ~ScopedProfiler() {
        profiler_.stopCPUProfiler();
    }

private:
    profiler::ProfilerManager& profiler_;
    std::string name_;
};

// 使用
profiler::ProfilerManager profiler;
{
    ScopedProfiler guard(profiler, "critical_section.prof");
    // 这里的代码会被 profiling
} // 自动停止
```

### 2. 环境隔离

```cpp
// 通过编译期宏控制：在 CMake 里用 target_compile_definitions(app PRIVATE ENABLE_PROFILING)
#ifdef ENABLE_PROFILING
    profiler::ProfilerManager profiler;
    profiler.startCPUProfiler("debug.prof");
#endif
```

> 宏名由你自己定义，本库不提供 `DEBUG_MODE` 之类的开关。

### 3. 并发调用：注意 CPU 采样是互斥的

```cpp
profiler::ProfilerManager profiler;

void thread1() {
    if (!profiler.isProfilerRunning(profiler::ProfilerType::CPU)) {
        profiler.startCPUProfiler("thread1.prof");
    }
}

void thread2() {
    // ❌ 不要与 thread1 的采样并发：analyzeCPUProfile() 会先停掉正在运行的
    //    CPU profiler 再启动自己的采样，两者互相破坏结果
    profiler.analyzeCPUProfile(10);
}
```

所有公共 API 都可在任意线程调用，也都有锁保护，但 **CPU 采样会话在语义上是独占的**：
`analyzeCPUProfile()` 会先停止已有会话（`src/profiler_manager.cpp:410-429`），而 gperftools 的
`ProfilerStart()` 在采样进行中会失败。要先判断状态（`isProfilerRunning()` 或 `/api/status`），
或者干脆把 profiling 操作串行化到一个专用线程里。

Heap 与 CPU 之间没有这层互斥，可以并存。

---

## 更多信息

- [API 参考手册](02_api_reference.md)
- [故障排除指南](04_troubleshooting.md)
- [返回主文档](../../README.md)
