/// @file log_macros.h
/// @brief Internal logging macros

#pragma once

#include "log_manager.h"
#include <spdlog/fmt/fmt.h>

PROFILER_NAMESPACE_BEGIN

namespace internal {

// Helper template to format message using fmt
template <typename... Args>
std::string formatMessage(fmt::format_string<Args...> fmt, Args&&... args) {
    return fmt::format(fmt, std::forward<Args>(args)...);
}

}  // namespace internal

PROFILER_NAMESPACE_END

// Internal logging macro using fmt-style formatting
#define PROFILER_LOG_IMPL(level, fmt_str, ...)                                     \
    do {                                                                           \
        if (::profiler::internal::LogManager::instance().shouldLog(level)) {       \
            ::profiler::internal::logMessage(level, __FILE__, __LINE__,            \
                __FUNCTION__,                                                      \
                ::profiler::internal::formatMessage(fmt_str, ##__VA_ARGS__));      \
        }                                                                          \
    } while (0)

// User-facing logging macros (fmt-style: PROFILER_INFO("Message: {}", value))
#define PROFILER_TRACE(fmt_str, ...)                                               \
    PROFILER_LOG_IMPL(::profiler::LogLevel::Trace, fmt_str, ##__VA_ARGS__)

#define PROFILER_DEBUG(fmt_str, ...)                                               \
    PROFILER_LOG_IMPL(::profiler::LogLevel::Debug, fmt_str, ##__VA_ARGS__)

#define PROFILER_INFO(fmt_str, ...)                                                \
    PROFILER_LOG_IMPL(::profiler::LogLevel::Info, fmt_str, ##__VA_ARGS__)

#define PROFILER_WARNING(fmt_str, ...)                                             \
    PROFILER_LOG_IMPL(::profiler::LogLevel::Warning, fmt_str, ##__VA_ARGS__)

#define PROFILER_ERROR(fmt_str, ...)                                               \
    PROFILER_LOG_IMPL(::profiler::LogLevel::Error, fmt_str, ##__VA_ARGS__)

#define PROFILER_FATAL(fmt_str, ...)                                               \
    PROFILER_LOG_IMPL(::profiler::LogLevel::Fatal, fmt_str, ##__VA_ARGS__)
