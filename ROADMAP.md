# 路线图

> 本文件合并了原 `ROADMAP.md`（现代化改进清单）与 `TODO.md`（待办列表）。
> 只收录**尚未完成**的事项；已完成项见 [CHANGELOG.md](CHANGELOG.md) 与 `git log`。

---

## 一、正确性问题（最高优先级）

这些是阅读源码时发现的实现与文档/预期不一致之处，会直接影响使用者。

### 1. `/api/heap/analyze` 忽略采样时长 🔴

- 路由只解析 `output_type`（`src/drogon_adapter.cpp:171-179`），`handleHeapAnalyze()` 也没有 duration 形参（`include/profiler/http_handlers.h:69`），最终硬编码 `analyzeHeapProfile(1, ...)`（`src/http_handlers.cpp:171`）
- 传 `?duration=10` 会被静默忽略，用户以为采样了 10 秒
- 方案：把 duration 打通到 handler，或明确标注该接口固定 1 秒

### 2. `~ProfilerManager()` 未停止 heap profiler 🔴

```cpp
if (profiler_states_[ProfilerType::HEAP].is_running) {
    IsHeapProfilerRunning();   // src/profiler_manager.cpp:94-96
}
```

- 调用的是查询函数而非 `HeapProfilerStop()`，析构后 heap profiler 仍在进程内运行
- 方案：改为 `HeapProfilerStop()`，并补测试

### 3. `include/profiler_manager.h:40` 注释与实现不符 🟡

- `ProfilerState::duration` 注释为"Configured duration in seconds"，实际存的是毫秒（`src/profiler_manager.cpp:189-191`、`214`），HTTP 层 JSON 键名也是 `duration_ms`（`src/http_handlers.cpp:48`）
- 方案：改注释为 milliseconds

### 4. `include/profiler_manager.h:129,135` 注释提及不存在的输出类型 🟡

- 注释写 `"iciclegraph", etc.`，但 `validateOutputType()` 只接受 `flamegraph` 与 `pprof`（`src/http_handlers.cpp:20-22`）
- 方案：删除该措辞

### 5. `start.sh` 打印不存在的查看页地址 🟡

- 脚本输出 `http://localhost:8080/flamegraph`，但该路由不存在；真实查看页为 `/show_svg.html`、`/show_heap_svg.html`、`/show_growth_svg.html`

---

## 二、测试覆盖

- **为 `http_handlers` 补单元测试**：`ProfilerHttpHandlers` 的各 handler 目前完全没有测试覆盖（现有 3 个测试只覆盖 gperftools 文件格式、完整流程与日志系统）
- **测试 `dispatch()`**：`include/profiler/http_handlers.h:81` 声明了按路径分发的 `dispatch()`，但 `src/http_handlers.cpp` 中**没有定义**——要么实现它，要么从公共头文件移除
- **补充 Web 资源嵌入测试**：验证内嵌 HTML 页面可正常返回
- 按模块拆分测试文件；引入 GTest 测试标签；启用 `ctest` 并行执行
- 引入 **Fuzz Testing** 覆盖 profile 解析路径

---

## 三、发布与分发

- **创建 git tag**：仓库当前**没有任何 tag**，但 `CHANGELOG.md` 与用户文档中的 `v0.1.0` 均指向它；应在 v0.1.0 对应提交上打 tag
- **自动化发布流程**：GitHub Releases + 打包（tar.gz / 各发行版包）
- **重新评估 Conan / vcpkg 分发**：
  - `conanfile.py` 使用的是 Conan 2 API，但 `self.cpp_info.libs` 仍写着已不存在的 `profiler_lib`，且 requirements 缺少 openssl/zlib/gtest/absl
  - `ports/cpp-remote-profiler/` 未在 `vcpkg-configuration.json` 中注册（无 `overlay-ports`），实际无法通过 vcpkg 安装
  - 两条路径都未被文档正式支持，建议要么修好并在 README 中说明，要么删除
- 生成 **SBOM**（SPDX / CycloneDX）
- 考虑 **CPM.cmake** 支持

---

## 四、CI/CD 增强

- **修复并重新启用 Thread Sanitizer**（`.github/workflows/code-quality.yml` 中该 job 被 `if: false` 禁用）
- **ccache / sccache** 加速 CI 编译
- **依赖更新自动化**（Dependabot 或 Renovate）
- **安全扫描**（CodeQL 或 Snyk）
- **macOS / Windows 构建矩阵**：需要先移除对 `/proc/self/exe`、`/proc/self/task`、`sigaction`/`ucontext` 的依赖，属较大改造

---

## 五、代码质量

- 引入 **cppcheck**、**include-what-you-use (IWYU)**
- 集成 **Valgrind** 做持续内存泄漏检测
- 引入 **Google Benchmark** 或 **nanobench** 做性能基准
- 符号可见性控制：`-fvisibility=hidden` + 显式导出宏（`CMakeLists.txt` 中已有 TODO 注释）
- **libabigail** 检查 ABI 变化
- 启用 PCH / Unity Build 缩短编译时间

---

## 六、文档与项目治理

- 添加 **`SECURITY.md`**（漏洞报告流程）
- 添加 **`CODE_OF_CONDUCT.md`**（`CONTRIBUTING.md:17` 已声称采用贡献者公约，但文件不存在）
- 添加 **Issue / PR 模板**（`CONTRIBUTING.md:26,225` 已引用，但 `.github/` 下只有 workflows）
- 部署在线文档（GitHub Pages / Read the Docs）
- README 状态徽章
- 添加 `.gitattributes`（统一换行符）、`.editorconfig`
- API 兼容性承诺目前只在文档中声明，缺少自动化校验

---

## 七、功能增强（长期）

- 支持更多输出格式（PDF、PNG）
- profile 数据对比（两次采样 diff）
- 历史 profile 数据查看与趋势面板
- 多进程 profiling 支持
- 降低对工作目录可写性的依赖（当前必须向 CWD 写入 `pprof` / `flamegraph.pl`）
