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

#### 1.3 Heap Growth Profiling ✅ (新增)
- [x] 集成 gperftools heap growth profiler
- [x] 使用 `GetHeapGrowthStacks()` API
- [x] 无需采样时长（即时获取）
- [x] 自动生成 profile 文件和 SVG 火焰图
- [x] 不需要 `TCMALLOC_SAMPLE_PARAMETER` 环境变量

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
- [x] `GET /api/growth/analyze` - 获取 heap growth 并返回 SVG（新增）

**实现细节**:
- CPU: 使用 `analyzeCPUProfile()` 进行采样和 SVG 生成
- Heap: 使用 `getRawHeapSample()` 获取 heap sample，然后用 pprof 生成 SVG
- SVG 后处理：添加 viewBox 属性修复负坐标问题，使浏览器正确显示

#### 2.3 原始 SVG 下载接口 ✅
- [x] `GET /api/cpu/svg_raw?duration=N` - 返回 pprof 生成的原始 CPU SVG（下载）
- [x] `GET /api/heap/svg_raw` - 返回 pprof 生成的原始 Heap SVG（下载）
- [x] `GET /api/growth/svg_raw` - 返回 pprof 生成的原始 Growth SVG（下载）（新增）

**设计目的**:
解决 `/api/cpu/analyze` 和 `/api/heap/analyze` 的 SVG 在浏览器中显示可能存在兼容性问题。用户可以下载原始 SVG，使用其他工具（如浏览器、Inkscape 等）打开查看。

**与一键分析接口的区别**:
| 特性 | `/api/cpu/analyze` | `/api/cpu/svg_raw` |
|------|-------------------|-------------------|
| SVG 处理 | 添加 viewBox 修复负坐标 | **不修改，返回原始 pprof 输出** |
| Content-Disposition | inline（浏览器显示） | attachment（触发下载） |
| pprof 输出处理 | 去除 pprof 信息输出 | **去除 pprof 信息输出** |
| 使用场景 | 快速浏览器查看 | 下载后用专业工具分析 |

**实现细节**:
```cpp
// CPU 原始 SVG
std::string profile_data = profiler.getRawCPUProfile(duration);
// 保存到临时文件
std::string temp_file = "/tmp/cpu_raw.prof";
// 使用 pprof 生成 SVG
std::string cmd = "./pprof --svg " + exe_path + " " + temp_file + " 2>/dev/null";
// 关键：提取纯 SVG 内容，去除 pprof 的信息输出（如 "Using local file..."）
size_t svg_start = svg_content.find("<?xml");
if (svg_start == std::string::npos) {
    svg_start = svg_content.find("<svg");
}
if (svg_start != std::string::npos && svg_start > 0) {
    svg_content = svg_content.substr(svg_start);
}
// 设置下载响应头
resp->addHeader("Content-Disposition", "attachment; filename=cpu_profile.svg");
```

**关键技术点**:
1. 使用 `2>/dev/null` 而非 `2>&1`，避免 pprof 的 stderr 输出混入 SVG
2. 提取 SVG 开始位置（`<?xml` 或 `<svg>`），去除前导信息
3. 设置 `Content-Disposition: attachment` 触发浏览器下载
4. 不添加 viewBox 或任何其他修改，保持 pprof 原始输出

#### 2.3.1 主页面下载按钮 ✅
**功能描述**:
在主页面 (`web/index.html`) 添加两个按钮，用于直接下载原始 SVG 文件，无需打开新标签页。

**实现细节**:
1. **UI 设计**:
   - 橙色下载按钮 (`#FF9800`)，区别于紫色分析按钮
   - 下载中显示 `⏳ 下载中...` 并禁用按钮
   - 完成后恢复按钮并显示成功/失败提示
   - CPU 和 Heap 可同时下载（各自独立管理状态）

2. **JavaScript 实现**:
   ```javascript
   function downloadCPURawSVG() {
       const duration = document.getElementById('cpu-duration').value;
       const btn = document.getElementById('cpu-download-btn');

       // 1. 禁用按钮，显示下载中状态
       btn.disabled = true;
       btn.textContent = '⏳ 下载中...';
       log(`📥 正在下载 CPU 原始 SVG (采样时长: ${duration}秒)...`);

       // 2. 使用 fetch 下载文件
       fetch(`/api/cpu/svg_raw?duration=${duration}`)
           .then(response => {
               if (!response.ok) throw new Error('下载失败');
               return response.blob();
           })
           .then(blob => {
               // 3. 创建下载链接并触发下载
               const url = URL.createObjectURL(blob);
               const a = document.createElement('a');
               a.href = url;
               a.download = `cpu_profile_${duration}s.svg`;
               a.click();
               URL.revokeObjectURL(url);

               // 4. 恢复按钮，显示成功
               btn.disabled = false;
               btn.textContent = '📥 下载 CPU 原始 SVG';
               log('✅ CPU 原始 SVG 下载完成');
           })
           .catch(error => {
               // 5. 错误处理
               btn.disabled = false;
               btn.textContent = '📥 下载 CPU 原始 SVG';
               log(`❌ CPU 原始 SVG 下载失败: ${error.message}`);
           });
   }

   // Heap 同理
   function downloadHeapRawSVG() { ... }
   ```

3. **CSS 样式**:
   ```css
   .download-btn {
       background-color: #FF9800;
   }
   .download-btn:hover {
       background-color: #F57C00;
   }
   button:disabled {
       background-color: #cccccc;
       cursor: not-allowed;
       opacity: 0.6;
   }
   ```

4. **环境变量配置**:
   - 设置 `TCMALLOC_SAMPLE_PARAMETER=524288` (512KB)
   - 在启动 profiler_example 时设置此环境变量
   - 确保 Heap Profiling 功能正常工作

**测试验证**:
- ✅ CPU 原始 SVG 下载成功（HTTP 200，文件格式正确）
- ✅ Heap 原始 SVG 下载成功（HTTP 200，文件格式正确）
- ✅ 按钮状态切换正常（禁用/恢复）
- ✅ 独立状态管理（CPU 和 Heap 可同时下载）

#### 2.4 辅助接口 ✅
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
- [x] Growth 火焰图页面（新增）
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
  ├── Heap Growth Profiler 分析（即时获取 + 显示 SVG）（新增）
  └── 状态显示

show_svg.html (CPU 火焰图)
  ├── SVG 容器
  ├── 控制按钮（重新加载、下载）
  └── 提示信息

show_heap_svg.html (Heap 火焰图)
  ├── SVG 容器
  ├── 控制按钮（重新加载、下载）
  └── 提示信息

show_growth_svg.html (Heap Growth 火焰图)（新增）
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

## 作为可复用库的完善计划

### 概述
本章节记录了将 cpp-remote-profiler 从"工具"转变为"可复用库"需要完善的功能。

### API 稳定性策略

#### 版本阶段划分
**当前版本**: v0.1.0 (开发阶段)

| 阶段 | 版本范围 | API 稳定性 | 生产环境就绪 |
|------|---------|-----------|------------|
| **开发阶段** | v0.x.x | ❌ API 可能随时变化 | ❌ 不推荐 |
| **稳定版本** | v1.0.0+ | ✅ 保证向后兼容 | ✅ 推荐 |

#### 语义化版本规则
```
MAJOR.MINOR.PATCH
  │     │     │
  │     │     └─ PATCH: 向后兼容的问题修复
  │     └─────── MINOR: 向后兼容的功能新增
  └───────────── MAJOR: 不兼容的 API 变更
```

#### 版本变更示例
- **v0.1.0 → v0.2.0**: 新增功能，API 可能调整
- **v0.1.0 → v0.1.1**: Bug 修复，保持 API 兼容
- **v0.x.x → v1.0.0**: API 稳定，承诺向后兼容

#### API 兼容性承诺（v1.0.0 起）
1. **PATCH 版本升级**:
   - ✅ 完全向后兼容
   - ✅ 只修复 bug
   - ✅ 不改变 API

2. **MINOR 版本升级**:
   - ✅ 向后兼容
   - ✅ 新增功能和 API
   - ✅ 已弃用功能保留至少一个 MINOR 版本

3. **MAJOR 版本升级**:
   - ⚠️ 可能包含不兼容的 API 变更
   - ⚠️ 需要升级指南
   - ⚠️ 提供迁移工具或文档

#### 版本检查宏
库用户可以使用宏检查版本：
```cpp
#include "version.h"

#if REMOTE_PROFILER_VERSION_AT_LEAST(0, 2, 0)
    // 使用 0.2.0 引入的新 API
#else
    // 使用旧 API
#endif
```

### P0 优先级（必须完成）

#### 1. 版本管理和发布机制 🔴 ✅ 已完成
**当前状态**:
- ❌ 缺少版本号（如 v1.0.0）
- ❌ 没有发布流程（release/tags）
- ❌ 没有 ABI/API 稳定性保证

**实施计划**:
1. 添加版本头文件 `include/version.h`
2. 定义语义化版本号（Major.Minor.Patch）
3. 制定 API 稳定性策略
4. 创建 release 流程文档

#### 2. 面向库用户的文档 🔴 ✅ 已完成
**当前状态**:
- ❌ 缺少"库用户"视角的文档（当前主要是"开发者"文档）
- ❌ 没有 API 参考手册（只有代码注释）
- ❌ 缺少集成示例（如何集成到现有项目）
- ❌ 没有故障排除指南

**实施计划**:
1. 创建 `docs/` 目录，分离用户文档和开发者文档
2. 编写 API 参考手册
3. 提供多种集成场景示例
4. 创建故障排除指南

#### 3. 构建配置灵活性 🔴 ✅ 已完成
**当前状态**:
- ❌ 只能构建为静态库
- ❌ 无法禁用不需要的功能（如 Web 服务器）
- ❌ 强制依赖 Drogon（即使只使用核心功能）

**实施计划**:
1. 添加 CMake 选项：
   - `BUILD_SHARED_LIBS` - 支持动态库
   - `REMOTE_PROFILER_ENABLE_WEB` - 启用/禁用 Web 服务器
   - `REMOTE_PROFILER_ENABLE_SYMBOLIZE` - 启用/禁用符号化
   - `REMOTE_PROFILER_INSTALL` - 启用安装目标
2. 优化依赖链，支持最小依赖编译

### P1 优先级（重要）

#### 4. 包管理支持 ✅ 已完成
**实施内容**:
1. ✅ vcpkg 支持（完善端口配置）
2. ✅ Conan 支持（conanfile.py + 测试包）
3. ✅ FetchContent 支持（配置示例）
4. ✅ find_package 支持（CMake config 文件）
5. ✅ 安装和使用文档

**生成文件**:
- `vcpkg.json` - 更新版本和描述
- `vcpkg-configuration.json` - vcpkg 配置
- `ports/cpp-remote-profiler/` - vcpkg 端口
- `conanfile.py` - Conan 包配置
- `conan/test_package/` - Conan 测试包
- `cmake/cpp-remote-profiler-config.cmake.in` - CMake config 模板
- `cmake/examples/` - FetchContent 示例
- `docs/user_guide/05_installation.md` - 安装指南
- `docs/user_guide/06_using_find_package.md` - find_package 使用指南

#### 5. CI/CD 流水线 ⏳ 待完成
**实施计划**:
1. 添加 GitHub Actions 工作流
2. 自动化测试（单元测试、集成测试）
3. 代码覆盖率检查
4. 内存泄漏检测（valgrind、ASan）
5. 多平台构建测试

#### 6. 错误处理规范
**实施计划**:
1. 定义错误码规范
2. 统一异常处理策略
3. 支持外部日志回调
4. 文档化所有可能的错误情况

### P2 优先级（增强）

#### 7. 性能基准测试
**实施计划**:
1. 创建性能测试 suite
2. 记录基准数据
3. 性能回归检测

#### 8. 多平台支持
**实施计划**:
1. 验证 Windows 支持
2. 验证 macOS 支持
3. 平台特定功能文档

#### 9. 预编译包
**实施计划**:
1. 提供 Linux 预编译包
2. 提供 macOS 预编译包（如果支持）
3. 提供包安装脚本

---

## 未来规划（原有功能）

### 短期目标 (1-2 周)
1. [ ] **完善库的可复用性** (最高优先级)
   - 完成 P0 任务：版本管理、文档、构建配置
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

### 2026-02-05 - Major Release: 库可复用性完善 ⭐
**版本**: v0.1.0

**重大更新**:

#### P0: 版本管理和文档 ✅
1. **版本管理机制**:
   - 创建 `include/version.h`，定义版本号 v0.1.0
   - 添加语义化版本支持
   - 制定 API 稳定性策略（v0.x 开发阶段，v1.0.0 后稳定）
   - 更新 `CMakeLists.txt` 和 `README.md` 版本信息

2. **面向库用户的完整文档** (新增 4 篇文档):
   - `docs/user_guide/01_quick_start.md` - 快速开始指南（5分钟集成）
   - `docs/user_guide/02_api_reference.md` - API 参考手册（完整 API 文档）
   - `docs/user_guide/03_integration_examples.md` - 集成示例（6 种场景）
   - `docs/user_guide/04_troubleshooting.md` - 故障排除指南
   - `docs/README.md` - 文档索引

3. **构建配置灵活性**:
   - 添加 `BUILD_SHARED_LIBS` 选项（默认动态库）
   - 添加 `REMOTE_PROFILER_INSTALL` 选项
   - 添加 `REMOTE_PROFILER_BUILD_EXAMPLES` 选项
   - 添加 `REMOTE_PROFILER_BUILD_TESTS` 选项
   - 支持 4 种编译配置组合

#### P1: 包管理支持 ✅
1. **vcpkg 支持**:
   - 更新 `vcpkg.json`（版本、描述、许可）
   - 创建 `vcpkg-configuration.json`
   - 创建 vcpkg 端口 `ports/cpp-remote-profiler/`

2. **Conan 支持**:
   - 创建 `conanfile.py`（Conan 2.0 配置）
   - 创建测试包 `conan/test_package/`
   - 支持多种选项（shared、fPIC、with_web、with_symbolize）

3. **FetchContent 支持**:
   - 创建 `cmake/examples/FetchContent_example.cmake`
   - 创建 `cmake/examples/CMakeLists.txt`
   - 创建 `cmake/examples/FetchContent_integration_example.cpp`

4. **find_package 支持** ⭐ 重点:
   - 创建 `cmake/cpp-remote-profiler-config.cmake.in`
   - 修改 `CMakeLists.txt` 使用 `configure_package_config_file()`
   - 正确导出 PUBLIC/PRIVATE 依赖
   - 支持依赖查找（gperftools、Backward、absl）
   - 验证 find_package 功能成功

5. **文档更新** (新增 2 篇文档):
   - `docs/user_guide/05_installation.md` - 详细安装指南（5 种方法）
   - `docs/user_guide/06_using_find_package.md` - find_package 使用指南

**技术改进**:
- CMake 安装导出优化
- PUBLIC/PRIVATE 依赖分离
- 跨项目依赖查找
- 版本兼容性检查

**测试验证**:
- ✅ 默认配置（动态库 + examples + tests）
- ✅ 静态库配置（禁用 examples 和 tests）
- ✅ find_package 功能验证成功
- ✅ 安装到临时目录验证成功

**新增文件** (共 20+ 个):
```
include/version.h
cmake/cpp-remote-profiler-config.cmake.in
cmake/examples/
vcpkg-configuration.json
ports/cpp-remote-profiler/
conanfile.py
conan/test_package/
docs/user_guide/01-06.md
docs/README.md
```

**文档完善度**:
- 6 篇用户指南（快速开始、安装、API、集成、故障排除、find_package）
- 总计 20,000+ 字的详细文档
- 覆盖所有主要使用场景

**下一步计划**:
- P1.2: CI/CD 流水线（GitHub Actions）
- P1.3: 错误处理规范
- P2: 性能基准测试、多平台支持

---

### 2026-01-14
- ✅ **实现 Heap Growth Profiler**
  - **核心 API**: 使用 `MallocExtension::GetHeapGrowthStacks()` 获取堆增长数据
  - **新增接口**:
    - `/pprof/growth` - 标准 pprof 接口，返回 heap growth stacks
    - `/api/growth/analyze` - 一键分析并生成 SVG 火焰图
    - `/api/growth/svg_raw` - 下载原始 SVG 文件
  - **前端支持**:
    - 主页面新增 "Heap Growth Profiler" section
    - 新增 `show_growth_svg.html` 查看器页面
    - 实现下载按钮和查看器功能
  - **关键特性**:
    - 无需 `TCMALLOC_SAMPLE_PARAMETER` 环境变量
    - 无需采样时长（即时获取当前堆增长数据）
    - 输出格式与 heap profile 相同，兼容 pprof
  - **测试验证**:
    - ✅ `/pprof/growth` 返回正确的 heap growth stacks 数据
    - ✅ `/api/growth/analyze` 成功生成 SVG 火焰图
    - ✅ `/api/growth/svg_raw` 成功下载原始 SVG
    - ✅ `/show_growth_svg.html` 查看器正常工作
    - ✅ `/api/status` 包含 growth 状态信息

### 2026-01-13
- 🔄 **重大重构**: 代码组织优化，支持作为库使用
  - **目录结构重构**:
    - 创建 `src/web_resources.cpp` - 嵌入所有 HTML 页面到 C++ 代码
    - 创建 `src/web_server.cpp` - 提取所有 HTTP 路由处理
    - 创建 `include/web_resources.h` 和 `include/web_server.h` - 库接口
    - 创建 `example/workload.cpp` 和 `example/workload.h` - 示例工作负载代码
    - 简化 `example/main.cpp` - 从 772 行减少到 45 行，只保留主入口
  - **Web 资源嵌入**:
    - 所有 HTML 页面（index.html, show_svg.html, show_heap_svg.html）嵌入到 C++ 代码
    - 删除 `web/` 目录，用户无需拷贝静态文件
    - 通过 `WebResources::getIndexPage()` 等函数访问嵌入的内容
  - **库的使用方式**:
    - **方式 1（Web 界面）**: 链接库，调用 `profiler::registerHttpHandlers()` 启动 Web 服务
    - **方式 2（核心库）**: 只链接 `profiler_lib`，直接调用 `ProfilerManager` API
  - **编译系统更新**:
    - 更新 `CMakeLists.txt`，添加新源文件到库
    - 移除复制 web 目录的命令
  - **测试验证**:
    - ✅ 编译成功（23MB 可执行文件）
    - ✅ 服务器启动成功，所有页面正常访问
    - ✅ API 接口正常工作
    - ✅ HTML 内容已正确嵌入到代码中
- ✅ 添加主页面下载按钮功能
  - 在 CPU 和 Heap section 各添加橙色下载按钮
  - 实现下载状态管理和用户提示
  - CPU 和 Heap 可同时下载（独立状态管理）

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

**文档版本**: 1.1
**最后更新**: 2026-01-13
**维护者**: Claude Code
