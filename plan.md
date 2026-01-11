# C++ Remote Profiler 整体规划

## 项目概述

**项目名称**: C++ Remote Profiler
**目标**: 类似 Go pprof 和 brpc pprof service 的 C++ 远程性能分析工具
**技术栈**: gperftools + Drogon + vcpkg
**开发语言**: C++20
**设计理念**:
- 参考 brpc pprof service 设计，提供标准 pprof 接口
- 支持两种使用场景：pprof 工具访问 + 浏览器直接查看
- 简化 API，去掉显式的启动/停止操作

---

## 核心架构

### 1. 两种使用场景

#### 场景 1: pprof 工具访问（标准模式）
```
pprof 工具 → GET /pprof/profile?seconds=10
             → 服务器内部采样（启动profiler → 采样 → 停止）
             → 返回原始 profile 文件（二进制）
             → pprof 工具本地生成 SVG/火焰图
```

**适用场景**:
- 开发者本地分析
- 需要使用 pprof 的高级功能
- 需要多种可视化格式（pdf、svg、text等）

#### 场景 2: 浏览器直接查看（一键分析）
```
浏览器 → GET /api/cpu/analyze?duration=10
       → 服务器内部采样 + 生成 SVG
       → 返回 SVG 图像
       → 浏览器直接显示火焰图
```

**适用场景**:
- 快速查看性能概况
- 远程服务器监控
- 不想安装 pprof 工具

**接口命名规则**:
- `/pprof/*` - 标准 pprof 接口（兼容 Go pprof 工具）
- `/api/*` - 自定义分析接口（用于浏览器直接查看）

### 2. 整体架构图

```
┌─────────────────────────────────────────────────────────────┐
│                     C++ Remote Profiler                     │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌──────────────┐      ┌─────────────┐      ┌─────────┐ │
│  │ HTTP Server │ ←──→ │ Drogon      │ ←──→ │ gperftools│ │
│  │   (Drogon)   │      │ Framework   │      │  Profiler  │ │
│  └──────────────┘      └─────────────┘      └─────────┘ │
│         ↓                      ↓                      ↓      │
│  ┌──────────────┐      ┌─────────────┐      ┌─────────┐ │
│  │   Web UI     │      │ REST API    │      │Profile  │ │
│  │  (HTML/JS)   │ ←──→ │  Handlers   │ ←──→ │  Files  │ │
│  └──────────────┘      └─────────────┘      └─────────┘ │
│                                                               │
└─────────────────────────────────────────────────────────────┘
```

### 2. 模块划分

#### 后端模块 (C++)
- `profiler_manager.{h,cpp}` - Profiler 管理核心
- `symbolize.{h,cpp}` - 符号化引擎
- `stack_collector.{h,cpp}` - 调用栈收集
- `profile_parser.{h,cpp}` - Profile 数据解析
- `example/main.cpp` - 示例程序

#### 前端模块 (Web)
- `index.html` - 主控制面板
- `show_svg.html` - CPU 火焰图显示
- `show_heap_svg.html` - Heap 火焰图显示
- `flamegraph.html` - 火焰图组件

#### 工具模块
- `embed_pprof.h` - 嵌入的 gperftools pprof 脚本
- FlameGraph 工具 - 火焰图生成

---

## 已完成功能 ✅

### 1. 核心功能

#### 1.1 CPU Profiling ✅
- [x] 集成 gperftools CPU profiler
- [x] 启动/停止 CPU profiling
- [x] 配置采样时长
- [x] 自动生成 profile 文件

#### 1.2 Heap Profiling ✅
- [x] 集成 gperftools heap profiler
- [x] 启动/停止 heap profiling
- [x] 配置采样时长
- [x] 自动生成 profile 文件

#### 1.3 Web 服务器 ✅
- [x] 基于 Drogon 框架的 HTTP 服务器
- [x] RESTful API 接口
- [x] 静态文件服务
- [x] CORS 支持

### 2. API 接口

#### 2.1 标准 pprof 接口（兼容 Go pprof）⏳
- [ ] `GET /pprof/profile?seconds=N` - CPU profile，返回原始 profile 文件
- [ ] `GET /pprof/heap` - Heap profile，调用 tcmalloc sample，返回原始 profile 文件
- [ ] `GET /pprof/growth` - Heap growth profile（可选）
- [ ] `GET /pprof/contention` - Contention profile（可选）

**工作流程**:
```
请求: GET /pprof/profile?seconds=10
  ↓
1. 启动 CPU profiler
2. 采样 10 秒
3. 停止 CPU profiler
4. 返回原始 profile 文件（二进制）
```

**Heap Sampling 配置**:
需要设置环境变量以启用 heap profiling：
```bash
export TCMALLOC_SAMPLE_PARAMETER=524288  # 采样间隔（字节）
# 或者
env TCMALLOC_SAMPLE_PARAMETER=524288 ./profiler_example
```

#### 2.2 一键分析接口（浏览器直接显示）✅
- [x] `GET /api/cpu/analyze?duration=N` - 采样并返回 SVG
- [x] `GET /api/heap/analyze?duration=N` - 采样并返回 SVG

#### 2.3 辅助接口 ✅
- [x] `GET /api/status` - 全局状态
- [x] `GET /api/list` - 列出所有 profile 文件

### 3. 符号化系统

#### 3.1 多层符号化策略 ✅
- [x] **Layer 1**: backward-cpp (C++ stack trace)
- [x] **Layer 2**: dladdr (动态链接信息)
- [x] **Layer 3**: addr2line (符号表查询)
- [x] **Layer 4**: 降级显示原始地址

**实现代码**:
```cpp
// src/symbolize.cpp
std::string ProfilerManager::resolveSymbolWithBackward(void* address)
```

#### 3.2 PIE 可执行文件支持 ✅
- [x] 计算相对地址 (address - dli_fbase)
- [x] 使用正确的可执行文件路径
- [x] 支持动态库符号查询

### 4. 前端界面

#### 4.1 主控制面板 ✅
- [x] 状态显示
- [x] 一键分析按钮（直接触发采样并显示 SVG）
- [x] 配置选项（采样时长）
- [x] 响应式设计

**说明**: 不再有"启动"、"停止"按钮，所有采样都是一次性操作

#### 4.2 火焰图显示 ✅
- [x] CPU 火焰图页面
- [x] Heap 火焰图页面
- [x] SVG 直接嵌入显示
- [x] 移除缩放功能（使用浏览器原生缩放）

**关键修复**:
- pprof SVG 使用负坐标 `viewBox="0 -1000 2000 1000"`
- 转换为正坐标 `viewBox="0 0 2000 1000"` 使内容可见
- 移除固定高度限制，支持浏览器无限缩放

### 5. Profile 数据处理

#### 5.1 数据格式支持 ✅
- [x] gperftools 原始 profile 格式
- [x] Collapsed stack trace 格式
- [x] SVG 格式输出

#### 5.2 数据解析 ✅
- [x] 解析 gperftools binary profile
- [x] 提取 PC 指针和调用栈
- [x] 符号化 PC 地址
- [x] 生成 collapsed 格式

### 6. 构建系统

#### 6.1 依赖管理 ✅
- [x] 使用 vcpkg 管理依赖
- [x] 自动下载和配置依赖库
- [x] 支持离线构建

**主要依赖**:
- drogon (C++ Web 框架)
- jsoncpp (JSON 解析)
- gperftools (性能分析库)
- tracy (可选的跟踪工具)
- backward-cpp (符号化)

#### 6.2 构建脚本 ✅
- [x] `CMakeLists.txt` 配置
- [x] `full_build.sh` 一键构建脚本
- [x] 嵌入式 pprof 脚本
- [x] 自动生成 pprof 脚本文件

---

## 待完成功能 ⏳

### 1. 修复 SVG 符号化问题 🔴

#### 问题描述
**接口**: `/api/cpu/analyze` 返回的 SVG 火焰图中显示的是地址（0x...）而非函数名

**原因分析**:
- `analyzeCPUProfile()` 方法调用 `pprof --svg` 生成 SVG
- 对于 PIE 可执行文件，pprof 无法正确符号化运行时地址
- `/pprof/profile` 接口返回的原始 prof 文件是正确的

**解决方案**:
- 修改 `analyzeCPUProfile()` 使用 FlameGraph 工具（已有符号化支持）
- 或者：在生成 SVG 前先对 profile 进行符号化预处理

**状态**: 待实现

### 2. 删除开始/停止接口 🔴

#### 需要删除的接口
- `DELETE /api/cpu/start`
- `DELETE /api/cpu/stop`
- `DELETE /api/heap/start`
- `DELETE /api/heap/stop`

#### 需要修改的文件
- `example/main.cpp` - 删除路由处理器
- `web/index.html` - 删除开始/停止按钮
- `README.md` - 更新文档

**保留的接口**:
- `GET /pprof/profile` ✅
- `GET /pprof/heap` ✅
- `GET /api/cpu/analyze` ✅
- `GET /api/heap/analyze` ✅

**状态**: ✅ 已完成（分支: remove-start-stop-interfaces）

### 3. 修复 Heap 分析接口 ✅

#### 问题描述
**接口**: `/api/heap/analyze` 无法生成火焰图
- 原实现使用 `HeapProfilerStart()`，需要在程序启动时设置 HEAPPROFILE 环境变量
- 不适合运行时分析

#### 解决方案
参考 brpc 实现，使用 `MallocExtension::GetHeapSample()` 直接获取 heap 采样：
- 移除 duration 参数（heap profiling 不需要采样时长）
- 直接调用 `getRawHeapSample()` 获取当前 heap 采样
- 使用 pprof 生成 SVG 火焰图

#### 变更内容
1. **修改 `/api/heap/analyze` 接口** (`example/main.cpp`)
   - 移除 duration 参数
   - 直接调用 `getRawHeapSample()` 获取 heap sample
   - 使用 pprof 绝对路径生成 SVG

2. **修改 ProfilerManager** (`include/profiler_manager.h`)
   - 将 `executeCommand()` 和 `getExecutablePath()` 改为 public
   - 供 HTTP 处理器使用

3. **修改 web 界面**
   - `web/index.html`: 移除 heap 分析的 duration 输入框
   - `web/show_heap_svg.html`: 直接调用 `/api/heap/analyze`（不带参数）
   - `example/main.cpp`: 修改 `analyzeHeap()` 函数

4. **修改 start.sh**
   - 添加 `TCMALLOC_SAMPLE_PARAMETER=524288` 环境变量
   - 启用 tcmalloc heap sampling 功能

**状态**: ✅ 已完成（分支: remove-start-stop-interfaces）

### 4. 实现 Go pprof 兼容接口 ✅

#### 1.1 需要实现的接口
- [ ] `GET /pprof/profile?seconds=N` - CPU profile
  - 内部启动 CPU profiler
  - 采样指定秒数
  - 停止 profiler
  - 返回原始 profile 文件（二进制）

- [ ] `GET /pprof/heap` - Heap profile
  - 调用 `MallocExtension::GetHeapSample()` 获取 tcmalloc 采样数据
  - 返回原始 profile 文件（二进制）
  - **注意**: 需要设置 `TCMALLOC_SAMPLE_PARAMETER` 环境变量

- [ ] 可选接口：
  - `GET /pprof/growth` - Heap growth profile
  - `GET /pprof/contention` - Contention profile

#### 1.2 环境变量配置

**TCMALLOC_SAMPLE_PARAMETER**:
- 作用：设置 tcmalloc heap sampling 的采样间隔
- 单位：字节
- 默认值：524288 (512KB)
- 推荐值：
  - 开发环境：524288 (512KB)
  - 生产环境：2097152 (2MB) 或更大，减少开销
- 设置方式：
  ```bash
  export TCMALLOC_SAMPLE_PARAMETER=524288
  ./profiler_example
  ```

#### 1.3 实现细节
```cpp
// 示例：CPU profile handler
void getPprofProfile(const HttpRequestPtr& req,
                     std::function<void(const HttpResponsePtr&)>&& callback) {
    int seconds = 30;
    auto secondsStr = req->getParameter("seconds");
    if (!secondsStr.empty()) {
        seconds = std::stoi(secondsStr);
    }

    // 1. 启动 CPU profiler
    ProfilerStart("/tmp/cpp_profiler/temp.prof");

    // 2. 等待指定秒数（需要在后台线程中执行）
    sleep(seconds);

    // 3. 停止 profiler
    ProfilerStop();

    // 4. 读取 profile 文件并返回
    std::string profile_data = readFile("/tmp/cpp_profiler/temp.prof");

    auto resp = HttpResponse::newHttpResponse();
    resp->setContentTypeCode(CT_APPLICATION_OCTET_STREAM);
    resp->setBody(profile_data);
    callback(resp);
}

// 示例：Heap profile handler
void getPprofHeap(const HttpRequestPtr& req,
                  std::function<void(const HttpResponsePtr&)>&& callback) {
    // 调用 tcmalloc sample
    char* buffer = nullptr;
    int length = 0;

    // 使用 MallocExtension::GetHeapSample() 获取 heap sample
    MallocExtension::instance()->GetHeapSample(&buffer, &length);

    auto resp = HttpResponse::newHttpResponse();
    resp->setContentTypeCode(CT_APPLICATION_OCTET_STREAM);
    resp->setBody(std::string(buffer, length));
    callback(resp);
}
```

**注意事项**:
- 需要异步处理，避免阻塞主线程
- 需要处理并发请求（同时多个 /pprof/profile 请求）
- 需要设置合理的超时时间
- Heap profile 需要程序启动时设置 `TCMALLOC_SAMPLE_PARAMETER` 环境变量

### 2. 符号化优化 🔴

#### 问题描述
**当前状态**: pprof 生成的 SVG 显示地址而非函数名

**原因**:
- pprof Perl 脚本无法正确符号化 PIE 可执行文件的运行时地址
- gperftools 采样的是运行时地址（0x000061...），需要转换为链接时地址

**测试结果**:
```
# 使用 /proc/self/exe:
Total: 6 samples
       1  16.7%  16.7%        1  16.7% ?? ??:0
       1  16.7%  33.3%        1  16.7% ?? ??:0

# 使用绝对路径:
       1  16.7%  16.7%        1  16.7% 0x0000610a7fc6280b
```

**解决方案**:

**方案 A**: 使用 FlameGraph 生成 SVG ✅ 已实现
- 使用服务器端符号化 (`getCollapsedStacks`)
- 使用 FlameGraph 工具生成 SVG
- 优点: 符号化正确，显示函数名
- 缺点: 不是 pprof 的调用图格式

**方案 B**: 修复 pprof 符号化 (未实现)
- 修改 pprof Perl 脚本的符号化逻辑
- 或者使用 nm/addr2line 预处理
- 优点: 保持 pprof 原生格式
- 缺点: 工作量大，需要深入 Perl 脚本

**当前选择**: 方案 A (使用 FlameGraph)

**实现代码**:
```cpp
// src/profiler_manager.cpp::analyzeCPUProfile()
std::string collapsed_data = getCollapsedStacks("cpu");
std::ostringstream cmd;
cmd << "perl /tmp/FlameGraph/flamegraph.pl " << collapsed_file;
```

### 2. SVG 渲染优化 🟡

#### 2.1 浏览器缩放限制
- [x] 移除固定容器高度
- [x] SVG height 设置为 auto
- [x] 支持浏览器无限缩放
- [ ] 潜在问题: SVG 过大时性能优化

#### 2.2 负坐标处理
- [x] 修改 viewBox 解决 pprof 负坐标问题
- [ ] 考虑: 动态计算实际内容的边界框
- [ ] 考虑: 添加平移/缩放手势支持

### 3. 错误处理和健壮性

#### 3.1 异常情况处理 ⏳
- [ ] Profiler 启动失败处理
- [ ] Profile 文件损坏检测
- [ ] 符号化失败降级策略
- [ ] 网络错误处理

#### 3.2 日志和调试 ⏳
- [ ] 结构化日志输出
- [ ] 调试模式开关
- [ ] 性能指标收集

### 4. 高级特性

#### 4.1 数据导出 🟡
- [x] Profile 文件下载
- [ ] Speedscope 格式支持
- [ ] pprof protobuf 格式支持
- [ ] CSV/JSON 格式导出

#### 4.2 实时监控 🟡
- [ ] WebSocket 实时更新
- [ ] 流式数据传输
- [ ] 实时火焰图更新

#### 4.3 多进程支持 🔴
- [ ] 进程列表显示
- [ ] 选择特定进程进行 profiling
- [ ] 远程机器 profiling

### 5. 性能优化

#### 5.1 后端优化 ⏳
- [ ] 异步 profiling 启动
- [ ] Profile 数据流式处理
- [ ] 符号化缓存
- [ ] 减少内存拷贝

#### 5.2 前端优化 🟡
- [ ] 虚拟滚动（大 SVG）
- [ ] 懒加载
- [ ] Web Worker 解析
- [ ] 压缩传输

### 6. 文档和测试

#### 6.1 文档 ⏳
- [ ] API 文档生成
- [ ] 用户手册
- [ ] 开发者指南
- [ ] 架构设计文档

#### 6.2 测试 ⏳
- [ ] 单元测试
- [ ] 集成测试
- [ ] 压力测试
- [ ] 端到端测试

---

## 设计文档

### 1. Profiler Manager 设计

**职责**:
- 管理 profiler 生命周期（启动/停止）
- 协调 profiling 流程
- 提供 API 接口

**关键方法**:
```cpp
class ProfilerManager {
public:
    // CPU Profiling
    bool startCPUProfiler(const std::string& output_path = "");
    bool stopCPUProfiler();
    std::string analyzeCPUProfile(int duration, const std::string& output_type);

    // Heap Profiling
    bool startHeapProfiler(const std::string& output_path = "");
    bool stopHeapProfiler();
    std::string analyzeHeapProfile(int duration, const std::string& output_type);

    // 数据查询
    std::string getStatus();
    std::string getProfileSamples(const std::string& profile_type);
    std::string getCollapsedStacks(const std::string& profile_type);
};
```

### 2. 符号化引擎设计

**架构**:
```
┌─────────────────────────────────────────┐
│         Symbolization Pipeline          │
├─────────────────────────────────────────┤
│ 1. backward-cpp (优先)                  │
│    - C++ stack trace                    │
│    - 源码级信息                        │
│         ↓                                │
│ 2. dladdr (动态链接)                    │
│    - 共享库符号                         │
│    - 函数名映射                         │
│         ↓                                │
│ 3. addr2line (符号表)                   │
│    - 精确文件和行号                     │
│    - 相对地址计算                       │
│         ↓                                │
│ 4. 原始地址 (降级)                      │
│    - 0x... 格式                         │
└─────────────────────────────────────────┘
```

**PIE 可执行文件处理**:
```cpp
// 计算相对地址
Dl_info info;
dladdr(address, &info);
uintptr_t relative_addr = addr - reinterpret_cast<uintptr_t>(info.dli_fbase);

// 使用 addr2line 符号化
std::ostringstream cmd;
cmd << "addr2line -e " << info.dli_fname << " -f -C 0x"
    << std::hex << relative_addr;
```

### 3. Web API 设计

#### 3.1 API 端点规范

| 端点 | 方法 | 说明 | 状态 |
|------|------|------|------|
| **标准 pprof 接口** ||||
| `/pprof/profile` | GET | CPU profile（兼容 Go pprof） | ⏳ |
| `/pprof/heap` | GET | Heap profile（兼容 Go pprof） | ⏳ |
| `/pprof/growth` | GET | Heap growth profile | 📋 |
| `/pprof/contention` | GET | Contention profile | 📋 |
| **一键分析接口** ||||
| `/api/cpu/analyze` | GET | 采样并返回 CPU 火焰图 SVG | ✅ |
| `/api/heap/analyze` | GET | 采样并返回 Heap 火焰图 SVG | ✅ |
| **辅助接口** ||||
| `/api/status` | GET | 获取全局状态 | ✅ |
| `/api/list` | GET | 列出所有 profile 文件 | ✅ |

#### 3.2 使用示例

**场景 1: 使用 pprof 工具访问**
```bash
# 采样 10 秒 CPU profile
go tool pprof http://localhost:8080/pprof/profile?seconds=10

# 或者先下载再用 pprof 打开
curl http://localhost:8080/pprof/profile?seconds=10 > cpu.prof
go tool pprof -http=:8081 cpu.prof

# Heap profile（需要先设置 TCMALLOC_SAMPLE_PARAMETER）
curl http://localhost:8080/pprof/heap > heap.prof
go tool pprof -http=:8081 heap.prof
```

**场景 2: 浏览器直接查看**
```bash
# 在浏览器中打开
http://localhost:8080/api/cpu/analyze?duration=10

# 或者在主页面点击"分析 CPU"按钮
http://localhost:8080/
```

#### 3.3 响应格式

**成功响应**:
```json
{
    "cpu_profiler": {
        "is_running": true,
        "output_path": "/tmp/cpp_profiler/cpu.prof",
        "duration": 5
    }
}
```

**错误响应**:
```json
{
    "error": "Failed to start CPU profiler"
}
```

### 4. 前端架构

#### 4.1 页面结构
```
index.html (主控制面板)
  ├── CPU Profiler 分析（一键触发采样 + 显示 SVG）
  ├── Heap Profiler 分析（一键触发采样 + 显示 SVG）
  └── 状态显示

show_svg.html (CPU 火焰图)
  ├── SVG 容器
  ├── 控制按钮（重新加载、下载）
  └── 提示信息

show_heap_svg.html (Heap 火焰图)
  ├── SVG 容器
  ├── 控制按钮（重新加载、下载）
  └── 提示信息
```

**关键变化**:
- 去掉了"启动"、"停止"按钮
- 保留"分析"按钮（一次性采样并显示）

#### 4.2 SVG 显示方案

**pprof SVG**:
- 格式: 调用图 (Call Graph)
- 坐标系统: 负坐标 (viewBox="0 -1000 2000 1000")
- 问题: 显示地址而非函数名

**FlameGraph SVG** (当前方案):
- 格式: 火焰图 (Flame Graph)
- 坐标系统: 正坐标 (viewBox="0 0 1200 214")
- 优点: 正确显示函数名
- 实现: 服务器端符号化 + FlameGraph 工具

### 5. 部署架构

```
┌───────────────────────────────────────────┐
│              Client Browser                │
└───────────────────────────────────────────┘
                    ↓ HTTP
┌───────────────────────────────────────────┐
│           Nginx / Reverse Proxy          │
└───────────────────────────────────────────┘
                    ↓
┌───────────────────────────────────────────┐
│      C++ Remote Profiler (Drogon)         │
│  - HTTP Server (port 8080)                │
│  - Static Files (/web/*)                   │
│  - API Handlers (/api/*)                   │
└───────────────────────────────────────────┘
```

---

## 技术决策记录

### 1. 为什么使用 gperftools 而非其他 profiler？
- **优势**: 成熟稳定，Google 生产环境验证
- **生态**: 与 pprof 工具链兼容
- **开销低**: 采样开销小，适合生产环境
- **功能**: 同时支持 CPU 和 Heap profiling

### 2. 为什么选择 Drogon 框架？
- **高性能**: 异步 I/O，低延迟
- **C++ 原生**: 与 profiler 无缝集成
- **易用**: 类似 Express.js 的路由语法
- **功能丰富**: ORM、WebSocket、Session 等

### 3. 为什么使用 vcpkg 管理依赖？
- **跨平台**: 支持 Linux/Windows/macOS
- **版本控制**: 锁定特定版本
- **自动化**: 一键安装和构建
- **社区支持**: Microsoft 官方支持

### 4. 为什么使用 FlameGraph 而非 pprof SVG？
- **符号化**: FlameGraph 方案能正确显示函数名
- **兼容性**: pprof 对 PIE 可执行文件支持不佳
- **控制权**: FlameGraph 更容易自定义和扩展
- **性能**: 服务器端符号化，浏览器直接显示

---

## 已知问题和限制

### 1. pprof 符号化问题 🔴
**问题**: pprof Perl 脚本无法符号化 PIE 可执行文件
**影响**: pprof SVG 显示地址而非函数名
**解决**: 使用 FlameGraph 替代 pprof
**状态**: ✅ 已解决

### 2. SVG 浏览器缩放限制 🟡
**问题**: 容器固定高度限制浏览器缩放
**影响**: 用户无法无限放大查看细节
**解决**: 移除固定高度，使用 auto
**状态**: ✅ 已解决

### 3. 性能开销 🟢
**问题**: Profiling 会增加程序开销
**影响**: CPU 1-5%，内存额外分配
**缓解**: 低频采样，可配置采样频率

### 4. 符号化不完整 🟡
**问题**: 部分系统调用无法符号化
**影响**: 显示为原始地址或共享库名
**缓解**: 多层符号化策略，尽力解析

---

## 未来规划

### 短期目标 (1-2 周)
1. [ ] **实现 Go pprof 兼容接口** (最高优先级)
   - 实现 `/pprof/profile` 和 `/pprof/heap` 接口
   - 返回原始 profile 文件
   - 测试与 Go pprof 工具的兼容性
   - 添加 `TCMALLOC_SAMPLE_PARAMETER` 环境变量支持
2. [ ] 完善错误处理和日志
3. [ ] 优化大 SVG 的渲染性能
4. [ ] 添加单元测试

### 中期目标 (1-2 月)
1. [ ] WebSocket 实时更新
2. [ ] 多进程支持
3. [ ] 性能优化（异步、缓存）
4. [ ] 完善文档和测试

### 长期目标 (3-6 月)
1. [ ] 分布式 profiling
2. [ ] 可视化性能对比
3. [ ] 自动化性能分析报告
4. [ ] Docker 部署方案

---

## 开发规范

### 分支管理
- ✅ 永远不要直接提交到 main 分支
- ✅ 在进行修改之前，先创建新分支
- ✅ 使用 `git checkout -b <branch-name>` 创建新分支

### 代码规范
- 使用 C++20 标准
- 遵循 Google C++ Style Guide
- 添加必要的注释和文档

### 测试规范
- 编译前运行 `cmake --build .`
- 测试关键功能路径
- 验证符号化是否正确

---

## 参考资源

- [gperftools Documentation](https://github.com/gperftools/gperftools)
- [FlameGraph Tool](https://github.com/brendangregg/FlameGraph)
- [Drogon Framework](https://github.com/drogonframework/drogon)
- [vcpkg Package Manager](https://github.com/microsoft/vcpkg)
- [backward-cpp](https://github.com/bombela/backward-cpp)

---

## 更新日志

### 2026-01-11
- 🔄 **架构调整**: 采用 Go pprof 标准接口设计
  - 计划实现 `/pprof/profile` 和 `/pprof/heap` 标准接口
  - 保留 `/api/cpu/analyze` 等一键分析接口
  - 去掉显式的启动/停止接口
  - 添加 `TCMALLOC_SAMPLE_PARAMETER` 环境变量支持
- ✅ 移除火焰图缩放功能
- ✅ 修复 SVG 负坐标显示问题
- ✅ 支持浏览器无限缩放
- 📝 创建 plan.md 文档
- 📝 更新开发规则（notes.md）

### 2026-01-10
- ✅ 实现一键式 CPU/Heap 分析
- ✅ 改进符号化系统
- ✅ 添加 collapsed 格式支持

---

**文档版本**: 1.0
**最后更新**: 2026-01-11
**维护者**: Claude Code
