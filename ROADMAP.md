# 路线图

> 本文件合并了原 `ROADMAP.md`（现代化改进清单）与 `TODO.md`（待办列表）。
> 只收录**尚未完成**的事项；已完成项见 [CHANGELOG.md](CHANGELOG.md) 与 `git log`。

---

## 一、正确性问题

这些是阅读源码时发现的实现与文档/预期不一致之处，会直接影响使用者。

> 第 1–5 项**已修复**（见 CHANGELOG.md 的 Unreleased 段与 `git log`）。
> 保留记录是为了说明当时的判断依据；新增问题请追加到本节的「待修」小节。

### 已修复

<details>
<summary>1. `/api/heap/analyze` 的 duration 语义（✅ 已修复）</summary>

原问题：路由只解析 `output_type`，`handleHeapAnalyze()` 也没有 duration 形参，最终硬编码
`analyzeHeapProfile(1, ...)`。

**根因不止是"参数没打通"**：gperftools 的 heap profiling 是**按分配驱动**的，
`HeapProfilerStart()` 开始记录、`HeapProfilerDump()` 写出快照，采样率由进程启动时的
`TCMALLOC_SAMPLE_PARAMETER` 决定，**与经过的时间无关**。所以 duration 对 heap 本就没有意义。

附带发现一个更严重的问题：原实现为了让 profile "有内容"，起了一个线程**伪造分配**
（每次迭代泄漏约 400KB 的假数据，且硬上限 100 次迭代 ⇒ 10 秒后停止，`duration > 10` 时后段纯空转）。
这把测试脚手架泄漏进了生产库，并污染了被分析进程的堆。

修复：去掉 `analyzeHeapProfile()` 的 duration 形参，删除伪造分配线程，改用
`HeapProfilerDump()` 做确定性快照。实测确认 `HeapProfilerStart/Dump/Stop` 这条路径
**不依赖** `TCMALLOC_SAMPLE_PARAMETER`（该变量只影响 `GetHeapSample()` 的采样精度）。

</details>

<details>
<summary>2. `~ProfilerManager()` 未停止 heap profiler（✅ 已修复）</summary>

原代码在析构里调用 `IsHeapProfilerRunning()`——一个查询函数，返回值被丢弃，heap profiler 继续运行。
已改为 `HeapProfilerStop()` 并同步状态。

已补回归测试 `ProfilerLifecycleTest.HeapProfilerStopsOnDestruction`：断言 gperftools 的
**全局**状态（而非 manager 自己的标志位），已验证把 bug 重新引入后该测试会失败。

</details>

<details>
<summary>3. `ProfilerState` 注释与实现不符（✅ 已修复）</summary>

`duration` 注释写 "seconds"，实际存毫秒；HTTP 层 JSON 键名是 `duration_ms`。注释已更正。

</details>

<details>
<summary>4. 注释提及不存在的输出类型（✅ 已修复）</summary>

`analyzeCPUProfile` / `analyzeHeapProfile` 的注释写了 `"iciclegraph", etc.`，
但只接受 `flamegraph` 与 `pprof`。措辞已删除。

</details>

<details>
<summary>5. `start.sh` 打印不存在的查看页地址（✅ 已修复）</summary>

原来输出 `http://localhost:8080/flamegraph`（无此路由），已改为真实的
`/show_svg.html`、`/show_heap_svg.html`、`/show_growth_svg.html`。

</details>

### 待修

- **`pprof --svg` 的失败信息不透出**：内置 pprof 脚本用 `dot`(graphviz) 渲染，采样点过少时 `dot` 失败，代码只回一句 `pprof did not generate valid SVG. Output: `（且 `svg_output` 为空），无法定位原因。应把 `dot` 的 stderr 一并返回。**这是异步改造期间实测复现的既有缺陷**，与请求调度无关。
- **`stopHeapProfiler()` 的 `output_path` 语义**：它把 `GetHeapProfile()` 的返回值写进 `output_path` 文件，而 `.heap` 是 gperftools 自己按 prefix 写的，两套产物并存容易混淆。
- **`/pprof/heap` 在采样关闭时返回 200，响应体却不是 profile**：`GetHeapSample()` 在 `TCMALLOC_SAMPLE_PARAMETER` 未设置（默认 0）时**不返回空串**，而是返回 pprof 的 `%warn` 警告文本加一行零样本统计（`heap profile: 0: 0 [0: 0] @ heap_v2/0`）。因此 `if (heap_sample.empty())` 这个错误分支进不去，客户端收到 200 + 看起来像 profile 的东西。应识别 `@ heap_v2/0` 或首行 `%warn` 并返回明确错误。
- **`/api/heap/svg_raw` 与 `/api/heap/flamegraph_raw` 在无 `TCMALLOC_SAMPLE_PARAMETER` 时表现不一致**：两者**都**先调 `getRawHeapSample()`（源码已核对），拿到的都是 `%warn` + 零样本文本，但因为上面那条缺陷没被拦下，后续命令的容错差异决定成败——`pprof --svg`（`svg_raw`）对零样本 profile 失败并超出 3 秒超时，返回 500 `Failed to generate SVG`；`pprof --collapsed` + `flamegraph.pl`（`flamegraph_raw`）却渲染出 13KB 的 SVG，返回 200。实测复现。修掉上面那条（识别零样本）即可同时解决。
- **`/tmp/cpp_profiler` 下的 `.heap` 快照不再清理**：每次 `analyzeHeapProfile` 用唯一前缀（时间戳 + 序号）产生一个新文件，长期运行会累积。
- **`analyzeHeapProfile` 的采样窗口固定 1 秒且不可配置**：分配稀疏的进程在窗口内可能一次分配都没有，只能得到空结果。可考虑加一个可选的窗口参数，或复用 `startHeapProfiler()`/`stopHeapProfiler()` 让调用方掌控。

### 已完成的调度改造（供参考）

所有会产生图表的 handler 都是秒级阻塞（CPU 采样最长 300 秒；渲染要 fork `pprof`/`flamegraph.pl`），原先在 **Drogon 事件循环线程**上同步执行。现在的结构：

- `include/profiler/async_executor.h` — 单工作线程执行器，把阻塞任务搬离事件循环，完成后经 `queueInLoop()` 回送响应
- 阻塞接口全部投递到该工作线程；`/api/status`、`/` 等快速接口保持在事件循环上
- **CPU 采样独占且 fail-fast**：`cpu_profiling_in_progress_` 是所有 CPU 采样入口（`getRawCPUProfile` / `analyzeCPUProfile`）共用的进程级标记。占用期间新请求**立即被拒绝**，不会排队、也不会打断正在进行的采样
- 拒绝语义与 Go 的 `net/http/pprof` 对齐：`/pprof/profile` 返回 500 + `text/plain` + `X-Go-Pprof: 1`；`/api/cpu/*` 返回 409 JSON
- 拒绝判定发生在**事件循环线程上、任务入队之前**（`runAsync` 的 precheck）。这是必须的：若放在任务内部判断，只能在前一个任务结束后才执行，永远观察不到它正在运行

实测：20 秒采样期间 `/api/status` 响应 0.0004s；并发请求第二个 0.0005s 内被拒且第一个不受影响。

---

## 二、测试覆盖

- **为 `http_handlers` 补单元测试** ✅ 已新增 `tests/test_http_handlers.cpp`（15 个用例，不依赖 Drogon）：`HandlerResponse` 工厂方法、`output_type` 校验、`/api/status` 契约、profiler 生命周期与 heap 快照唯一性
- **`dispatch()`** ✅ 已处理：声明存在于 `include/profiler/http_handlers.h` 但从未定义，已**从公共头文件移除**（不提供按路径自动分发，路由由使用者框架注册）
- **补充 Web 资源嵌入测试**：验证内嵌 HTML 页面可正常返回
- 其余 handler 的端到端覆盖（需要 `./pprof` 与 `./flamegraph.pl`，依赖 CWD 可写）
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
