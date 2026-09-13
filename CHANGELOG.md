# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
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
- **Breaking:** `ProfilerManager` is no longer a singleton — construct it directly and manage its lifetime
- **Breaking:** removed the spdlog dependency; use `setLogSink()` / `setLogLevel()` instead
- **Breaking:** core library decoupled from the web layer — `profiler_core` (no Drogon) and `profiler_web` (optional Drogon adapter) are now separate targets
- **Breaking:** `analyzeHeapProfile()` takes `(duration, output_type)` again, and `handleHeapAnalyze` / `handleHeapSvgRaw` / `handleHeapFlamegraphRaw` all take a duration. This reverses an earlier removal that was right about the *sampling rate* but wrong to conclude the window is meaningless: gperftools fixes the rate via `TCMALLOC_SAMPLE_PARAMETER` at process start, so `duration` cannot improve precision, but it does decide **how long allocations are collected**. A process that allocates sparsely needs a longer window to produce anything, and a hard-coded 1s gave it no recourse. Default 1s, clamped 1-300s.
- `/api/heap/svg_raw` and `/api/heap/flamegraph_raw` now run their own sampling window (`getRawHeapProfileSample()`) instead of pulling `GetHeapSample()`. Besides honouring the duration, this makes them work in a process started without `TCMALLOC_SAMPLE_PARAMETER`, which previously made `/api/heap/svg_raw` fail with 500 while its sibling `/api/heap/flamegraph_raw` succeeded on the same input. The three window-based endpoints now behave identically.
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
- `~ProfilerManager()` did not stop the heap profiler: it called `IsHeapProfilerRunning()`, a query whose result was discarded, leaving the process-global profiler recording after the object was destroyed and still dumping profiles during process exit
- `/pprof/symbol` now implements the Go pprof symbolz protocol correctly
- CMake package config exports the correct targets and dependency lookup
- `BUILD_DOCS=ON` was dead code — `CMakeLists.txt` tested `DOxygen_FOUND` while FindDoxygen sets `DOXYGEN_FOUND`
- UBSan integer-overflow errors; strict mode enabled
- clang-tidy job could not locate dependency headers
- Suppressed a known tcmalloc leak so ASan builds fail only on real errors

### Fixed
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

### Fixed
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

### Removed
- `logger.h` (unused after the logging rework)
- The synthetic allocation thread inside `analyzeHeapProfile()`

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
