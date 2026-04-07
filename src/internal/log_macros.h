#pragma once

#include "log_manager.h"
#include <format>
#include <string>

PROFILER_NAMESPACE_BEGIN

namespace internal {

template <typename... Args> std::string formatMessage(std::format_string<Args...> fmt, Args&&... args) {
    return std::format(fmt, std::forward<Args>(args)...);
}

inline void logToManager(LogManager& mgr, LogLevel level, const char* file, int line, const char* function, std::string&& message) {
    mgr.sink()->log(level, file, line, function, message.c_str());
}

} // namespace internal

PROFILER_NAMESPACE_END

// The PROFILER_LOG_IMPL macro uses logManager() method which returns a reference to the internal LogManager.
// This method must be available in the calling context (i.e., ProfilerManager methods).
#define PROFILER_LOG_IMPL(level, fmt_str, ...)                                                                         \
    do {                                                                                                               \
        auto& _log_mgr = logManager();                                                                                 \
        if (_log_mgr.shouldLog(level)) {                                                                               \
            ::profiler::internal::logToManager(_log_mgr, level, __FILE__, __LINE__, __FUNCTION__,                      \
                                               ::profiler::internal::formatMessage(fmt_str, ##__VA_ARGS__));            \
        }                                                                                                              \
    } while (0)

#define PROFILER_TRACE(fmt_str, ...) PROFILER_LOG_IMPL(::profiler::LogLevel::Trace, fmt_str, ##__VA_ARGS__)
#define PROFILER_DEBUG(fmt_str, ...) PROFILER_LOG_IMPL(::profiler::LogLevel::Debug, fmt_str, ##__VA_ARGS__)
#define PROFILER_INFO(fmt_str, ...) PROFILER_LOG_IMPL(::profiler::LogLevel::Info, fmt_str, ##__VA_ARGS__)
#define PROFILER_WARNING(fmt_str, ...) PROFILER_LOG_IMPL(::profiler::LogLevel::Warning, fmt_str, ##__VA_ARGS__)
#define PROFILER_ERROR(fmt_str, ...) PROFILER_LOG_IMPL(::profiler::LogLevel::Error, fmt_str, ##__VA_ARGS__)
#define PROFILER_FATAL(fmt_str, ...) PROFILER_LOG_IMPL(::profiler::LogLevel::Fatal, fmt_str, ##__VA_ARGS__)
