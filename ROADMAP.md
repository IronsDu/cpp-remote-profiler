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
<summary>1. `/api/heap/analyze` 的 duration 语义（✅ 已修复；该路径现已并入 `/api/pprof/heap`）</summary>

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

### 已修复（本轮，用 go tool pprof 验证）

- `/pprof/heap` 在采样关闭时返回 200 + `%warn` 文本：`GetHeapSample()` 不返回空串，导致空值检查失效。现按 `@ heap_v2/0` / 前导 `%warn` 识别，返回 500。
- `/api/heap/svg_raw` 与 `flamegraph_raw` 的行为不一致：根因同上，两者现在都返回同样的明确错误（后经 API 重构合并为 `/api/pprof/heap`）。
- 分析类接口的错误响应**双重包裹**成非法 JSON（`{"error":"{"error": "..."}`）：现将内部错误消息解出后再包一层。
- `/pprof/heap` 与 `/pprof/growth` 的错误改用 Go `serveError()` 的形状（`text/plain` + `X-Go-Pprof: 1`），`go tool pprof` 能直接显示原因。
- `flamegraph.pl` 对空输入返回"合法 SVG + ERROR 文本"，被当作 200 成功：现识别并转为带原因的 500。

### 已完成（本轮 API 重构）

- HTTP API 收敛：9 个端点 → 4 个（`/api/pprof/{cpu,heap,growth}` + `/api/pprof/heap/snapshot`），
  渲染器与交付方式改为参数（`renderer` / `output`），消除了 `analyze` 与 `*_raw` 各采样一次的重复。
- `duration` 默认值 1 秒 → 10 秒（实测 3 秒失败率 35–45%、10 秒 0%，见 CHANGELOG）。
- 移除 3 个查看器页及其内联 SVG 分支——其页内缩放从未生效（生成的 SVG 内嵌了从未被初始化的
  pan/zoom 库）。面板改为「下载 + 实时日志」。
- 新增 `scripts/verify-web-ui.mjs`（CDP 驱动真实浏览器回归，23 项检查）与
  `scripts/check-sanitizers.sh`（本地复刻 CI 的 ASan/UBSan/TSan 配置）。

### 已完成（CI 稳定性）

- 测试可在并行下运行（`ctest -j`）。此前 `pprof_cpu_temp.prof`、`cpu_collapsed.prof` 等
  中间产物用**固定文件名**放在共享目录，两个测试进程会互相截断文件——CI 的
  `FullFlowTest` 读到 0 字节 profile 即源于此。现按 PID 命名。
- `FullFlowTest.GetRawCPUProfile` 在采样窗口内**真的跑 CPU 负载**，而不是 sleep。
  gperftools 只采样正在执行的线程，窗口内空闲时返回空 profile 是**设计如此**，
  原断言把这种合法结果当成失败。
- `scripts/check-sanitizers.sh` 支持 `CTEST_JOBS=<n>` 以复现并行场景。

### 待修

- **`GetHeapProfile()` 的返回缓冲区没有可移植的释放方式**：头文件说调用方应 `free()`，
  上游测试也这么做，但静态链接 tcmalloc 时该缓冲区来自库自身的分配器，进程的 malloc
  拦截器（ASan）不认识它，`free()` 会 abort；`tc_free()` 仅 gperftools ≥ 2.16.90 存在；
  2.18 还把中间 chunk 走内部 arena，其 `Free` 在私有头文件里。当前选择是**不释放**
  （每次约 11KB，已在 `lsan.supp` 登记）。若将来 gperftools 提供公开释放函数，应改回释放。

- **生成的 SVG 无法缩放**：FlameGraph 的 `zoom()` 找不到它要操作的 `#viewport`（其产物只有
  `#frames`），pprof 的 SVGPan 库则从未被初始化（产物缺少 `onload` 挂钩）。两条路径的交互脚本
  都是死代码。可选方案：改为在服务端生成自己的交互式 SVG，或在文档中明确"下载后用桌面工具看"。

- **火焰图里的函数名带地址后缀**：内置 pprof 的 `--collapsed` 输出把函数名写成
  `cpuIntensiveTask()<0000000000409360>`，经 `flamegraph.pl` 渲染后直接显示在图上。除了噪音，还有一个
  理论隐患：同一函数出现在不同地址时（模板实例化）会被当成**不同栈帧**，本该合并的被拆开。
  可在送入 `flamegraph.pl` 之前剥掉 `<十六进制>` 后缀。
- **部分符号未 demangle**：实测火焰图里残留 `_ZSt12construct_at...` 之类的 mangled 名，且这类帧常显示为
  无上下文的 `operator()[inline]`（lambda 的 `operator()`）。可对未解开的符号做二次 `__cxa_demangle`。
- **两个 pprof 实现的差异未写进文档**：内置 Perl pprof（`renderer=callgraph` 的渲染器）符号化更完整——
  实测同一份 profile 未解析帧为 0、且能标出 `(inline)`；而 `go tool pprof` 强在对比与交互
  （`-diff_base`、`-peek`、`-traces`）。README 目前只说了"两种渲染方式"，没说各自适合什么场景。
- **`pprof --svg` 的失败信息不透出**：内置 pprof 脚本用 `dot`(graphviz) 渲染，采样点过少时 `dot` 失败，代码只回一句 `pprof did not generate valid SVG. Output: `（且 `svg_output` 为空），无法定位原因。应把 `dot` 的 stderr 一并返回。**这是异步改造期间实测复现的既有缺陷**，与请求调度无关。
- **`stopHeapProfiler()` 的 `output_path` 语义**：它把 `GetHeapProfile()` 的返回值写进 `output_path` 文件，而 `.heap` 是 gperftools 自己按 prefix 写的，两套产物并存容易混淆。
- **窗口式 heap 端点缺少"采样率"调节**：`duration` 控制的是**采集多久**（覆盖度），采样率仍由
  `TCMALLOC_SAMPLE_PARAMETER` 在启动时固定。若调用方希望在运行期调整**精度**，目前没有途径
  （`HEAP_PROFILE_ALLOCATION_INTERVAL` 只在 `HeapProfilerStart` 前设环境变量才生效）。

### 已完成的调度改造（供参考）

所有会产生图表的 handler 都是秒级阻塞（CPU 采样最长 300 秒；渲染要 fork `pprof`/`flamegraph.pl`），原先在 **Drogon 事件循环线程**上同步执行。现在的结构：

- `include/profiler/async_executor.h` — 单工作线程执行器，把阻塞任务搬离事件循环，完成后经 `queueInLoop()` 回送响应
- 阻塞接口全部投递到该工作线程；`/api/status`、`/` 等快速接口保持在事件循环上
- **CPU 采样独占且 fail-fast**：`cpu_profiling_in_progress_` 是所有 CPU 采样入口（`getRawCPUProfile` / `analyzeCPUProfile`）共用的进程级标记。占用期间新请求**立即被拒绝**，不会排队、也不会打断正在进行的采样
- 拒绝语义与 Go 的 `net/http/pprof` 对齐：`/pprof/profile` 返回 500 + `text/plain` + `X-Go-Pprof: 1`；`/api/pprof/cpu` 返回 409 JSON
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
