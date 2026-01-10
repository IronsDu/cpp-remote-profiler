# SVG 火焰图实现方案

## 实施时间
2026-01-10

## 当前方案

### 1. 符号化方案
**问题**: 原先使用 `/proc/self/exe` 作为可执行文件路径调用 pprof，导致生成的 SVG 只显示内存地址而非函数名。

**解决方案**: 使用 `readlink("/proc/self/exe")` 获取可执行文件的绝对路径，再传给 pprof 进行符号化。

**代码变更**:
- 在 `profiler_manager.h` 中添加 `getExecutablePath()` 方法声明
- 在 `profiler_manager.cpp` 中实现该方法：
  ```cpp
  std::string ProfilerManager::getExecutablePath() {
      char exe_path[PATH_MAX];
      ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
      if (len == -1) {
          return "/proc/self/exe";  // fallback
      }
      exe_path[len] = '\0';
      return std::string(exe_path);  // 返回绝对路径
  }
  ```

**修改位置**:
1. `analyzeCPUProfile()` - CPU分析函数
2. `analyzeHeapProfile()` - Heap分析函数
3. `getProfileSamples()` - 获取样本数据函数

### 2. 抑制 pprof 调试输出
**问题**: pprof 会输出 "Using local file..." 等调试信息到 stderr，导致 SVG 响应不是纯净的 XML，浏览器无法解析。

**解决方案**: 在 pprof 命令中重定向 stderr 到 /dev/null

**代码变更**:
```cpp
// 之前:
cmd << "./pprof --svg " << exe_path << " " << profile_path << " 2>&1";

// 之后:
cmd << "./pprof --svg " << exe_path << " " << profile_path << " 2>/dev/null";
```

### 3. 官方 gperftools pprof 脚本
使用官方 gperftools-2.16 版本的 pprof Perl 脚本（5580行），通过 C++ raw string literal 嵌入到程序中。

**优点**:
- 支持多种输出格式（SVG、文本、PDF等）
- 完整的符号化功能
- 支持调用栈可视化

## 已知问题

### 问题1: SVG 缩放功能异常
**现象**:
- 点击放大按钮有时会缩小
- 默认视图没有显示完整的火焰图
- 缩放行为不一致

**可能原因**:
- SVG 内置的 JavaScript 缩放库（SVGPan）配置问题
- SVG viewBox 设置不正确
- 需要调整 SVG 的初始视口和缩放参数

**状态**: 待解决

### 问题2: SVG 火焰图是黑白的
**现象**:
- 当前生成的 SVG 火焰图是黑白线条
- 没有根据函数占比（采样次数）使用不同颜色

**期望效果**:
- 根据函数热度（CPU占比）使用不同颜色
- 热度高的函数用暖色（红、橙、黄）
- 热度低的函数用冷色（蓝、绿）
- 方框粗细根据占比自动调整

**可能方案**:
1. pprof 参数调整:
   - `--colors` 参数可以设置着色模式
   - 尝试不同的着色方案（热力图、按包名等）
2. 后处理 SVG:
   - 解析生成的 SVG
   - 根据样本数量计算颜色
   - 重新着色后返回

**状态**: 待解决

## 当前 API 端点

- `GET /api/cpu/analyze?duration=10&output_type=flamegraph` - 生成 CPU 火焰图 SVG
- `GET /api/heap/analyze?duration=10&output_type=flamegraph` - 生成 Heap 火焰图 SVG
- `GET /show_svg.html?duration=10` - 前端展示页面（带缩放功能）

## 技术栈

- **C++ 框架**: Drogon (HTTP server)
- **Profiling 工具**: gperftools (libprofiler, libtcmalloc)
- **可视化工具**: gperftools pprof (Perl script)
- **输出格式**: SVG (with embedded JavaScript for zoom/pan)

## 代码变更总结

**文件**: `src/profiler_manager.cpp`, `include/profiler_manager.h`

**关键修改**:
1. 添加 `getExecutablePath()` 辅助函数
2. 三个 pprof 调用位置改为使用绝对路径
3. stderr 重定向到 /dev/null

**效果**:
- ✅ 函数名正确显示（不再是内存地址）
- ✅ SVG 可以在浏览器中解析（不再是 XML 解析错误）
- ⚠️ 缩放功能仍有问题
- ⚠️ 颜色显示单调（黑白）
