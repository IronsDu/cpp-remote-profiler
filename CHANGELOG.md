# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]
### Added


- `/api/thread/stacks` (and `getThreadCallStacks()`) print each thread's **name** next to its tid, read from `/proc/<tid>/comm`. A bare tid is not identifiable when reading the output afterwards; the name is what tells you that thread 1234 is `DrogonIoLoop` and 1235 is a worker parked on a futex.
- CMake options documentation and reproducible `CMakePresets.json` presets (`debug`, `release`, `relwithdebinfo`, `coverage`, clang variants)
- `find_package` integration test under `cmake/examples/test_find_package/`
- Doxygen API documentation support (`-DBUILD_DOCS=ON`)
- Abseil-style versioned inline namespace (`profiler::v0_1_0`) with `PROFILER_NAMESPACE_BEGIN/END`
- Configurable logging: `LogSink` interface, `LogManager`, per-instance logger
- Code coverage reporting (Codecov) and clang-format check in CI
- `tests/test_http_handlers.cpp` — first test coverage for `ProfilerHttpHandlers`
  and the heap-profiler lifecycle (runs without Drogon)
- `tests/test_async_executor.cpp` — serialization, queue bounds and shutdown semantics
- `profiler/async_executor.h` — bounded single-worker executor for long-running jobs

### Changed


- The Heap Snapshot section is one **output** dropdown (raw profile text / flame graph / call graph) with a single 打开 and 下载 pair, instead of separate buttons per product. Three options, one endpoint: `?format=profile`, `?format=svg&renderer=flamegraph`, `?format=svg&renderer=callgraph`.
- Each chart section owns its renderer selector. The snapshot section previously read the Heap Profiler section's dropdown, so which picture you got from the snapshot depended on a control in a different block.
- Every section offers 打开 (`output=inline`) alongside 下载 (`output=attachment`). The inline mode was reachable only by hand-assembling a URL before, since the buttons hard-coded attachment -- a feature nothing in the UI exercised.
- **Breaking:** the HTTP API was consolidated. Nine endpoints across three
  profiler types became four:

  | before | after |
  |---|---|
  | `/api/{cpu,heap,growth}/analyze` | `/api/pprof/{cpu,heap,growth}` |
  | `/api/{cpu,heap,growth}/svg_raw` | `/api/pprof/{cpu,heap,growth}?renderer=callgraph` |
  | `/api/{cpu,heap,growth}/flamegraph_raw` | `/api/pprof/{cpu,heap,growth}?renderer=flamegraph` |
  | `/api/heap/*?source=state` | `/api/pprof/heap/snapshot` |

  `analyze` and `svg_raw` were the *same operation* — sample for N seconds, then
  render — differing only in the renderer and in whether the response forced a
  download, so each pair sampled the process twice for one picture. The renderer
  is now a parameter (`renderer=flamegraph|callgraph`) and delivery is another
  (`output=inline|attachment`). The old paths are gone, not aliased.
- **Breaking:** the heap snapshot is its own endpoint,
  `/api/pprof/heap/snapshot`, with `format=profile` (default, raw text for
  `go tool pprof`) or `format=svg`. It is state-based ("what is in the heap now")
  as opposed to `/api/pprof/heap`, which is window-based ("what was allocated
  during the window"). Keeping it on the same resource behind a `source=` flag
  would have put two different questions at one URL.
- **Breaking:** `renderer=callgraph` replaces `output_type=pprof`. The old value
  named a tool rather than the result: the pprof script draws a node/edge *call
  graph*, while FlameGraph draws a *flame graph*. They are different pictures, not
  two styles of one, and the parameter should say which picture you get.
- The `duration` default is now 10s everywhere (was 1s for heap, 30s for
  `/pprof/profile`). Measured on the bundled example workload, a 3s window failed
  to collect enough samples to render 35-45% of the time; 10s never failed. A 1s
  default was close to useless.
- Responses are `inline` by default, so a plain link shows the SVG in the browser
  instead of downloading it; `output=attachment` restores a forced download.
  Inline display is purely the absence of `Content-Disposition` — verified that
  the same bytes navigate as a document without it and abort with it.
- The three `/show_*_svg.html` viewer pages are removed, along with the inline-SVG
  branch they consumed. Their in-page pan/zoom never worked: the generated SVGs
  embed a pan/zoom library that is never initialised (FlameGraph's `zoom()` looks
  for a `#viewport` element that its own output does not contain, and the pprof
  SVGPan library has no `onload` hook). Downloading the SVG and opening it in a
  desktop tool is what actually works, so the panel now offers downloads and a
  live log rather than a viewer.
- `ProfilerHttpHandlers` exposes one entry point per profiler —
  `handleCpuChart` / `handleHeapChart` / `handleGrowthChart` / `handleHeapSnapshot` —
  each taking a `ChartOptions` (renderer, duration, delivery) instead of the
  per-endpoint duration/output_type argument lists. Shared rendering moved into
  `renderChart`/`renderFlameGraph`/`renderCallGraph`, replacing six copies of the
  same shell-out logic.
- **Breaking:** `ProfilerManager` is no longer a singleton — construct it directly and manage its lifetime
- **Breaking:** removed the spdlog dependency; use `setLogSink()` / `setLogLevel()` instead
- **Breaking:** core library decoupled from the web layer — `profiler_core` (no Drogon) and `profiler_web` (optional Drogon adapter) are now separate targets
- **Breaking:** `analyzeHeapProfile()` takes `(duration, output_type)` again, and `handleHeapAnalyze` / `handleHeapSvgRaw` / `handleHeapFlamegraphRaw` all take a duration. This reverses an earlier removal that was right about the *sampling rate* but wrong to conclude the window is meaningless: gperftools fixes the rate via `TCMALLOC_SAMPLE_PARAMETER` at process start, so `duration` cannot improve precision, but it does decide **how long allocations are collected**. A process that allocates sparsely needs a longer window to produce anything, and a hard-coded 1s gave it no recourse. Default 1s, clamped 1-300s.
- `/api/heap/svg_raw` and `/api/heap/flamegraph_raw` now run their own sampling window (`getRawHeapProfileSample()`) instead of pulling `GetHeapSample()`. Besides honouring the duration, this makes them work in a process started without `TCMALLOC_SAMPLE_PARAMETER`, which previously made `/api/heap/svg_raw` fail with 500 while its sibling `/api/heap/flamegraph_raw` succeeded on the same input. The three window-based endpoints now behave identically.
- Both heap renderers accept `?source=state` to draw the state-based snapshot instead, so the cumulative heap view can be rendered as a chart rather than only exported as text. Previously the graph endpoints could only draw a collection window, which meant the panel had no way to show the live heap visually even though the rendering path existed.
- `handleHeapSvgRaw` renders with `pprof --alloc_space` rather than the default `--inuse_space`. pprof's default plots what is live *now*, so a short collection window -- whose allocations have usually already been freed -- produced a profile with an empty in-use column and the render failed with "No nodes to print" even though the window recorded real allocations. Measured: `?duration=1` went from 500 to a 23KB graph.
- The flame-graph handlers' `pprof --collapsed` step also uses `--alloc_space` now, and no longer counts pprof's own "Using local file ..." chatter as stack data. pprof's default reports what is live *now*, so a short window often produced a collapsed file with zero stack lines -- which still passed the "has data" check because the two chatter lines do not start with `#` -- and flamegraph.pl then rejected it. Measured: `?duration=2` failed 2 times out of 5 before, 0 out of 6 after, and the same sample already rendered fine through the svg path, which is what pointed at the mode rather than the data.
- All four flame-graph handlers share one validator (`flameGraphResponse`). flamegraph.pl answers unusable input with a *valid* SVG whose only content is an "ERROR: ..." message, so the structural `<?xml`/`<svg` check passed and a 200 carrying an error text reached callers as if it were a graph. An earlier fix had added the check to `generateFlameGraph()` only; the handlers that inline the command still had the weak check.
- The panel's chart download now requests the same data as the chart view. It built its URL from `duration` alone and dropped `source=state`, so with the state source selected it silently downloaded a window collection instead. Both actions now share `fetchHeapChart()`, and viewing opens a blob URL so the SVG is displayed rather than downloaded (`Content-Disposition` does not apply to blobs).
- `downloadHeapChart()` routes its call through `Promise.resolve().then(...)` so a synchronous throw is caught too. A throw raised before the promise chain existed left the countdown running and the button disabled forever, which is what made the button look inert; the `.catch()` could not see it.

Verified in a real browser (headless Chrome driven over CDP) rather than by inspection: all four heap actions issue the expected request -- including `source=state` where it matters -- the view action opens a blob URL and reports its size, and the download action leaves a file on disk (94580 bytes for the window source, 18810 for the state source). No page errors.
- The empty-snapshot case names itself instead of reading as a render fault. tcmalloc reports nothing until its first sampling event, so the state-based view is legitimately empty for a freshly started process -- and pprof's "No nodes to print" was surfacing as a generic rendering error. It now explains the cold start and points at the window source as an alternative; the collection-window case gets its own wording about a short window.
- All four download actions insert their `<a>` into the DOM before clicking it; the heap/CPU/growth chart downloads never did, which leaves the click unreliable in browsers that require attachment.
- **Breaking:** removed the never-implemented `ProfilerHttpHandlers::dispatch()` declaration; route registration stays with the host framework
- **The Drogon adapter no longer runs profiling on the event-loop thread.** Every handler that can block — CPU/heap/growth analysis, the `_raw` renderers, and `/pprof/profile` — is dispatched to a background worker and its response is handed back with `queueInLoop()`. Profiling takes seconds (up to 300s for CPU), so this previously stalled all other requests, including `/api/status`, for the whole sampling window.
- Blocking jobs share one worker thread. This is required for correctness, not just throughput: gperftools keeps its profiling session in process-global state, so concurrent analyses would corrupt each other's results.
- **Concurrent CPU profile requests are now rejected instead of queued**, matching Go's `net/http/pprof`. A second request arriving while a session is sampling fails immediately rather than silently waiting behind up to 300 seconds of someone else's window, and it no longer stops that in-progress session (which previously corrupted both results). Responses: `/pprof/profile` returns `500` + `text/plain` + `Could not enable CPU profiling: cpu profiling already in use` with an `X-Go-Pprof: 1` marker; the custom `/api/cpu/*` endpoints return `409 Conflict`.
- **Concurrent heap analysis is likewise rejected.** `HeapProfilerStart()` has no failure mode -- it silently replaces the output prefix rather than refusing -- so two callers that both observed "not running" would proceed together and end up sharing one snapshot. `analyzeHeapProfile()` now claims the profiler atomically before touching it and rejects with "heap profiling already in use" (HTTP 409 on `/api/heap/analyze`). Snapshot prefixes also gained a counter: two analyses starting in the same millisecond previously produced identical names. `/api/growth/analyze` is unaffected because it reads `GetHeapGrowthStacks()` and claims no session.
- `ProfilerManager::cpu_profiling_in_progress_` is now `static`: the gperftools session it guards is process-global, so a per-instance flag would let two `ProfilerManager` objects each claim it.
- `analyzeCPUProfile()` honours that same guard (it previously had no concurrency control at all and would stop whatever session was running).
- `analyzeHeapProfile()` no longer synthesizes its own allocations to force samples. Fabricating allocations polluted the profile with the profiler's own stack frames and leaked memory into the process under test; it now snapshots the application's real allocations
- Framework-agnostic `ProfilerHttpHandlers` introduced; Drogon is now only one possible adapter
- Internal headers moved to `src/internal/`, no longer part of the public include path
- Release build type now defaults to `RelWithDebInfo`; all build types carry `-g`
- Installation layout made consistent across `lib`/`lib64` hosts so `find_package()` works (GNUInstallDirs is now included before the install rules)

### Fixed
- `stopHeapProfiler()` and `getRawHeapProfileSample()` no longer call `GetHeapProfile()`. They write the window's snapshot with `HeapProfilerDump()` and read that file back, so the profile memory belongs to us: nothing needs freeing, and the `leak:GetHeapProfile` suppression is gone. This replaces the previous workaround of retaining the buffer, which existed only because `GetHeapProfile()`'s return value has no portable deallocator (see ROADMAP). Note the dump lands beside the requested output path under a derived name -- gperftools appends `.<sequence>.heap` to the prefix -- so `stopHeapProfiler()` still writes the caller's path, from the dump's contents.
- Reverted the `free()` added to `stopHeapProfiler()` and `getRawHeapProfileSample()`: it aborted ASan's job with "attempting free on address which was not malloc()-ed". `GetHeapProfile()`'s header says the caller should free the buffer, and upstream's own test does exactly that, but the buffer has no portable deallocator in practice -- with a statically linked tcmalloc it comes from the library's own allocator, which the process's malloc interposer never saw (so `free()` aborts), `tc_free()` only exists from gperftools 2.16.90, and gperftools 2.18 additionally routes its intermediate chunks through an internal arena whose `Free` lives in a private header. The buffer is roughly 11KB per stop, so it is copied out and released to the OS instead; `lsan.supp` carries a matching suppression. The earlier "fix" traded a benign leak for a crash.
- The ASan job now also covers a statically linked tcmalloc build, which is what vcpkg produces and what the crash above required; the shared-library configuration used locally cannot reproduce it.
- Downloaded charts no longer overwrite each other. Filenames were built from a second-precision timestamp, so two downloads completing within the same second produced the same name and the browser silently replaced the first file with the second -- picking 火焰图 then 调用图 for the heap snapshot lost the flame graph, and the panel still reported both as saved. The timestamp now carries milliseconds. Found because the browser regression's download check counted files instead of comparing names, so an overwrite looked like a success; it now tracks the filename set and asserts that each of the seven downloads produced its own file.


- The test suite is safe to run in parallel. The profiler's intermediate artifacts
  (`pprof_cpu_temp.prof`, `cpu_analyze.prof`, `cpu_collapsed.prof`,
  `heap_collapsed.prof`, and the per-endpoint render inputs) had fixed names in a
  shared directory, so two test binaries running at once truncated each other's
  files. `ctest -j4` reproduced the CI failure locally: FullFlowTest read back a
  0-byte CPU profile because a concurrently running binary had recreated the
  shared file. The names now carry the process id.
- `FullFlowTest.GetRawCPUProfile` (and the gperftools profile test) generate real
  CPU work across the sampling window instead of sleeping through it. gperftools
  samples executing threads, so a window spent idle yields an empty profile by
  design -- the test treated that legitimate result as a failure whenever the
  machine had nothing to sample. It now fails only for a real fault.
- An out-of-range `duration` now clamps to 1..300 for every analysis endpoint. The two backends disagreed: `getRawCPUProfile()` rejected out-of-range values (surfacing as "Failed to collect a CPU profile in the requested window") while `getRawHeapProfileSample()` clamped them, so the same query succeeded or failed depending on which profiler served it. The clamping happens once at the HTTP boundary. Measured after: `duration=-99` and `duration=0` both run a 1s window, `duration=9999` runs 300s rather than hanging for 9999.
- `stopHeapProfiler()` freed nothing for the profile it generates:
  `GetHeapProfile()` returns a malloc'd string the caller must `free()`, and
  letting it convert directly into a `std::string` discarded the pointer, leaking
  the entire profile on every stop (LeakSanitizer: ~11.6KB per call). The path
  only became reachable once `analyzeHeapProfile()` and the new
  `getRawHeapProfileSample()` started routing through `stopHeapProfiler()`, but
  the leak itself is older: that function previously had no callers at all.
  Surfaced by the new HttpHandlersTest cases, which are the first to drive
  start/stop directly.
- `~ProfilerManager()` did not stop the heap profiler: it called `IsHeapProfilerRunning()`, a query whose result was discarded, leaving the process-global profiler recording after the object was destroyed and still dumping profiles during process exit
- `/pprof/symbol` now implements the Go pprof symbolz protocol correctly
- CMake package config exports the correct targets and dependency lookup
- `BUILD_DOCS=ON` was dead code — `CMakeLists.txt` tested `DOxygen_FOUND` while FindDoxygen sets `DOXYGEN_FOUND`
- UBSan integer-overflow errors; strict mode enabled
- clang-tidy job could not locate dependency headers
- Suppressed a known tcmalloc leak so ASan builds fail only on real errors

- A session opened by the host through `startCPUProfiler()` / `startHeapProfiler()`
  is no longer preempted. `getRawCPUProfile()` and `analyzeCPUProfile()` used to
  stop whatever session was running and start their own; they now refuse and
  report "already in use", leaving the host's session untouched. `startCPUProfiler()`
  exists so the host controls sampling, so a later request must not pull the rug
  out from under it. The same rule now applies to `analyzeHeapProfile()` versus
  `startHeapProfiler()`.
- `getRawCPUProfile()` read and mutated `profiler_states_[CPU]` outside the mutex
  while stopping an existing session; the stop and the state update now happen
  under the lock, as does the corresponding teardown in `getRawCPUProfile()`.

- `/pprof/heap` reported success when heap sampling was off. `GetHeapSample()` does
  not return an empty string in that case -- it returns pprof's `%warn` advisory
  followed by a valid-looking but empty profile (`@ heap_v2/0`) -- so the
  emptiness check never fired and clients received HTTP 200 carrying the warning
  text. The zero sampling rate (or a leading `%warn`) is now recognised and
  reported as an error. This also removes an inconsistency where
  `/api/heap/svg_raw` failed with 500 while `/api/heap/flamegraph_raw` rendered a
  13KB graph from the identical input.
- Analyze endpoints emitted invalid JSON on failure: the core returns a
  `{"error": ...}` string and the handler wrapped it again without escaping,
  producing `{"error":"{"error": "..."}`. The inner message is now extracted
  first; `handleHeapAnalyze` also no longer discards the reason in favour of a
  generic string.
- `/pprof/heap` and `/pprof/growth` errors now follow Go's `serveError()` shape
  (`text/plain` plus `X-Go-Pprof: 1`), so `go tool pprof` prints the reason
  instead of the content type misleading it.
- `flamegraph.pl` answers empty input with a *valid* SVG whose entire content is
  an error message, so the structural SVG check passed and callers got HTTP 200
  plus a graph reading "ERROR: No valid input provided to flamegraph.pl." That is
  now detected and returned as a 500 explaining that the sample window was too
  short or the process idle.

### Removed


- **Breaking:** `ProfilerManager::getThreadStacks()` is gone. It was 122 lines that nothing called: `/api/thread/stacks` has always used `getThreadCallStacks()`, which answers the same question ("where is each thread stuck") with a full call stack instead of a `/proc` field. Its one unique contribution, the thread name, is now part of the endpoint output.
- `logger.h` (unused after the logging rework)
- The synthetic allocation thread inside `analyzeHeapProfile()`

### Documentation


- README, the API reference and the troubleshooting guide now spell out that the
  two heap views are **opposite in kind**, not two renderings of one dataset:
  `/pprof/heap` is state-based (cumulative snapshot of the current heap, needs
  `TCMALLOC_SAMPLE_PARAMETER`) while `/api/heap/analyze` is window-based (only
  allocations made during a fixed 1s sampling window, and it does not need that
  variable). A process holding a large heap but no longer allocating yields a
  full graph from the former and an empty result from the latter. Measured
  bounds included: allocations made before `HeapProfilerStart()` never appear,
  allocations freed inside the window still do, and consecutive sessions do not
  accumulate.
- The heap Web panel says which of the two it performs, and no longer labels a
  window-rendered SVG download as a "heap profile". It also gained the missing
  entry point for the state-based view: the panel previously offered only the
  window-based endpoints, so a process holding a large heap but no longer
  allocating could not be inspected from the UI at all. The Heap section is now
  two labelled cards, the second driving `GET /pprof/heap` with view and
  download actions.

### Notes


- The repository currently has **no git tags**; `v0.1.0` below refers to commit `a503106`. Tagging is tracked in [ROADMAP.md](ROADMAP.md).

## [0.1.0] - 2026-02-05

First release as a reusable library, transforming cpp-remote-profiler from a
standalone tool into an embeddable library.

### Added

#### Core features
- CPU profiling via gperftools
- Heap profiling with leak detection (tcmalloc sampling)
- Heap growth stack analysis (`GetHeapGrowthStacks()`), no sampling env var required
- Thread stack capture across all threads, dynamic thread count
- Standard Go pprof interface: `/pprof/profile`, `/pprof/heap`, `/pprof/growth`, `/pprof/symbol`
- One-click analysis endpoints returning SVG: `/api/{cpu,heap,growth}/analyze`
- Raw SVG download: `/api/{cpu,heap,growth}/{svg_raw,flamegraph_raw}`
- Web control panel with flame-graph viewer pages (`/show_svg.html`, `/show_heap_svg.html`, `/show_growth_svg.html`)
- Signal-handler safety: the previous handler is saved and restored

#### Library integration
- Modern CMake configuration with install targets
- `find_package(cpp-remote-profiler)` support via `cpp-remote-profiler-config.cmake`
- `FetchContent` and `add_subdirectory` integration
- vcpkg support with an in-repo port (`ports/cpp-remote-profiler/`)
- Conan 2 recipe (`conanfile.py`)
- Shared/static build selection (`BUILD_SHARED_LIBS`)

#### Documentation
- User guide: quick start, API reference, integration examples, installation, troubleshooting, `find_package`
- `include/version.h` with semantic-version macros and an API stability policy

#### Examples
- `example/main.cpp`, `example/workload.cpp`, `example/custom_signal.cpp`

### Technical details

#### Endpoints registered by `registerDrogonHandlers()`
- `/` — web control panel
- `/pprof/profile`, `/pprof/heap`, `/pprof/growth` (GET), `/pprof/symbol` (POST)
- `/api/cpu/analyze` (GET/POST), `/api/heap/analyze`, `/api/growth/analyze` (GET)
- `/api/{cpu,heap,growth}/svg_raw` (GET)
- `/api/{cpu,heap,growth}/flamegraph_raw` (GET)
- `/api/thread/stacks`, `/api/status` (GET)
- `/show_svg.html`, `/show_heap_svg.html`, `/show_growth_svg.html` (GET)

#### Dependencies
- gperftools — CPU/heap profiling core (found via pkg-config)
- Drogon — optional web layer
- backward-cpp, Abseil — stack symbolization
- nlohmann-json, OpenSSL, zlib, protobuf — supporting libraries

---

## Pre-release history

Development before the library restructuring: initial CPU profiling with
gperftools, a browser-based flame-graph UI, protobuf-based profile parsing,
Abseil symbolization, one-click CPU/heap analysis, vcpkg-based dependency
management, and embedding of web resources into the binary.

[Unreleased]: https://github.com/IronsDu/cpp-remote-profiler/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/IronsDu/cpp-remote-profiler/releases/tag/v0.1.0
