#pragma once

// ============================================================================
// Version Information
// ============================================================================
// Current version: 0.1.0 (Development Phase)
//
// Version format: MAJOR.MINOR.PATCH
//   - MAJOR: Incompatible API changes
//   - MINOR: Backwards-compatible functionality additions
//   - PATCH: Backwards-compatible bug fixes
//
// Phase: 0.x.x - Development (API may change)
//         1.0.0 - Stable release (API backwards compatibility guaranteed)
// ============================================================================

#define REMOTE_PROFILER_VERSION_MAJOR 0
#define REMOTE_PROFILER_VERSION_MINOR 1
#define REMOTE_PROFILER_VERSION_PATCH 0

#define REMOTE_PROFILER_VERSION "0.1.0"

// Version as integer for comparison (e.g., 00100 for 0.1.0)
#define REMOTE_PROFILER_VERSION_INT                                                                                    \
    ((REMOTE_PROFILER_VERSION_MAJOR * 10000) + (REMOTE_PROFILER_VERSION_MINOR * 100) + (REMOTE_PROFILER_VERSION_PATCH))

// API Stability Notice
// ---------------------
// Current phase: DEVELOPMENT (v0.x.x)
// - API is subject to change without notice
// - Not recommended for production use
// - Feedback and suggestions welcome
//
// Future phase: STABLE (v1.0.0+)
// - API backwards compatibility will be maintained
// - Deprecated features will be marked for at least one minor version
// - Breaking changes will require major version bump

#define REMOTE_PROFILER_API_STABILITY "Development phase - API subject to change"

// Version checking helper macros
#define REMOTE_PROFILER_VERSION_AT_LEAST(major, minor, patch)                                                          \
    ((REMOTE_PROFILER_VERSION_MAJOR) > (major) ||                                                                      \
     ((REMOTE_PROFILER_VERSION_MAJOR) == (major) &&                                                                    \
      ((REMOTE_PROFILER_VERSION_MINOR) > (minor) ||                                                                    \
       ((REMOTE_PROFILER_VERSION_MINOR) == (minor) && (REMOTE_PROFILER_VERSION_PATCH) >= (patch)))))

// Example usage:
// #if REMOTE_PROFILER_VERSION_AT_LEAST(0, 2, 0)
//     // Use new API introduced in 0.2.0
// #else
//     // Use old API
// #endif
