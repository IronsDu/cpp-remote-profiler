#pragma once

// ============================================================================
// Version Information
// ============================================================================
// This file provides backward compatibility for version checking.
// The actual version information is auto-generated in profiler_version.h
// ============================================================================

#include "profiler_version.h"

// Backward compatibility - redirect to new macros
#define REMOTE_PROFILER_VERSION_MAJOR PROFILER_VERSION_MAJOR
#define REMOTE_PROFILER_VERSION_MINOR PROFILER_VERSION_MINOR
#define REMOTE_PROFILER_VERSION_PATCH PROFILER_VERSION_PATCH
#define REMOTE_PROFILER_VERSION PROFILER_VERSION
#define REMOTE_PROFILER_VERSION_INT PROFILER_VERSION_INT
#define REMOTE_PROFILER_VERSION_AT_LEAST PROFILER_VERSION_AT_LEAST

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
