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

### Changed
- **Breaking:** `ProfilerManager` is no longer a singleton — construct it directly and manage its lifetime
- **Breaking:** removed the spdlog dependency; use `setLogSink()` / `setLogLevel()` instead
- **Breaking:** core library decoupled from the web layer — `profiler_core` (no Drogon) and `profiler_web` (optional Drogon adapter) are now separate targets
- Framework-agnostic `ProfilerHttpHandlers` introduced; Drogon is now only one possible adapter
- Internal headers moved to `src/internal/`, no longer part of the public include path
- Release build type now defaults to `RelWithDebInfo`; all build types carry `-g`
- Installation layout made consistent across `lib`/`lib64` hosts so `find_package()` works (GNUInstallDirs is now included before the install rules)

### Fixed
- `/pprof/symbol` now implements the Go pprof symbolz protocol correctly
- CMake package config exports the correct targets and dependency lookup
- UBSan integer-overflow errors; strict mode enabled
- clang-tidy job could not locate dependency headers
- Suppressed a known tcmalloc leak so ASan builds fail only on real errors

### Removed
- `logger.h` (unused after the logging rework)

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
