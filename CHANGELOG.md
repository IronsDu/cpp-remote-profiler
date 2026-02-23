# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Add MIT LICENSE file
- Add graphviz runtime dependency documentation

## [0.1.0] - 2026-02-05

First official release - transforming cpp-remote-profiler from a tool to a reusable library.

### Added

#### Core Features
- CPU Profiling using gperftools
- Heap Profiling with memory leak detection (tcmalloc sample)
- Thread stack capture for all threads with dynamic thread count support
- Standard Go pprof interface compatibility
- Web interface with one-click flame graph analysis
- RESTful HTTP API endpoints
- Raw SVG download for CPU/Heap profiles
- Signal handler safety (save and restore user signal handlers)

#### Library Support
- Modern CMake 3.15+ configuration with installation targets
- `find_package()` support with `cpp-remote-profiler-config.cmake`
- `FetchContent` integration support
- vcpkg package manager support with custom port
- Conan 2.0 package support
- Shared/static library build options (`BUILD_SHARED_LIBS`)
- Optional components (`with_web`, `with_symbolize`)

#### Documentation
- Complete API reference (`docs/user_guide/02_api_reference.md`)
- Quick start guide (`docs/user_guide/01_quick_start.md`)
- Integration examples for 6 scenarios (`docs/user_guide/03_integration_examples.md`)
- Installation guide with 5 methods (`docs/user_guide/05_installation.md`)
- Troubleshooting guide (`docs/user_guide/04_troubleshooting.md`)
- `find_package` usage guide (`docs/user_guide/06_using_find_package.md`)

#### CI/CD
- GitHub Actions workflow for build and test
- Code quality checks (AddressSanitizer, UBSan, Clang-Tidy)
- Support for GCC and Clang compilers
- vcpkg dependency caching

#### Examples
- Basic usage example (`example/main.cpp`)
- Workload example for profiling (`example/workload.cpp`)
- Custom signal handling example (`example/custom_signal.cpp`)

#### Version Management
- Semantic versioning support (`include/version.h`)
- Version check macro (`REMOTE_PROFILER_VERSION_AT_LEAST`)
- API stability policy documentation

### Technical Details

#### API Endpoints
- `/pprof/profile` - CPU profile (Go pprof compatible)
- `/pprof/heap` - Heap profile (Go pprof compatible)
- `/pprof/symbol` - Symbol information (Go pprof compatible)
- `/api/cpu/flamegraph` - One-click CPU flame graph
- `/api/heap/flamegraph` - One-click heap flame graph
- `/api/cpu/svg_raw` - Raw CPU flame graph SVG download
- `/api/heap/svg_raw` - Raw heap flame graph SVG download
- `/api/thread/stacks` - Thread stack capture
- `/api/status` - Service status query

#### Dependencies
- gperftools - CPU/Heap profiling core
- Drogon - Web framework
- backward-cpp - Stack symbolization
- nlohmann-json - JSON support
- OpenSSL, zlib, Protobuf, Abseil - Supporting libraries

---

## Development History (Pre-release)

### Initial Implementation
- Basic CPU profiling with gperftools
- Web interface for flame graph visualization
- Browser-based interactive flame graph rendering
- Protobuf-based profile parsing
- absl symbolization support
- One-click CPU/Heap analysis

### Refactoring
- Removed pprof script dependency
- Unified dependency management with vcpkg
- Cleaned up build scripts
- Code organization for library usage
- Web resource embedding

---

[Unreleased]: https://github.com/IronsDu/cpp-remote-profiler/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/IronsDu/cpp-remote-profiler/releases/tag/v0.1.0
