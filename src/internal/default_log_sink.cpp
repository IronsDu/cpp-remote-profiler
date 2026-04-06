/// @file default_log_sink.cpp
/// @brief Default log sink implementation using std::cout/std::cerr

#include "default_log_sink.h"
#include <chrono>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <string>

PROFILER_NAMESPACE_BEGIN

namespace internal {

static const char* levelToString(LogLevel level) {
    switch (level) {
    case LogLevel::Trace:
        return "TRACE";
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warning:
        return "WARN";
    case LogLevel::Error:
        return "ERROR";
    case LogLevel::Fatal:
        return "FATAL";
    default:
        return "UNKNOWN";
    }
}

static std::string currentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tm_buf{};
    localtime_r(&time_t_now, &tm_buf);

    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);

    return std::string(buf) + "." + std::to_string(ms.count());
}

DefaultLogSink::DefaultLogSink() = default;

DefaultLogSink::~DefaultLogSink() = default;

void DefaultLogSink::log(LogLevel level, const char* file, int line, const char* function, const char* message) {
    if (static_cast<int>(level) < static_cast<int>(min_level_)) {
        return;
    }

    // Extract filename from path
    const char* filename = file;
    if (const char* last_slash = strrchr(file, '/')) {
        filename = last_slash + 1;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Format: [timestamp] [LEVEL] [file:line] message
    std::string timestamp = currentTimestamp();

    // Use stderr for warnings and above, stdout for the rest
    auto& stream = (static_cast<int>(level) >= static_cast<int>(LogLevel::Warning)) ? std::cerr : std::cout;

    stream << "[" << timestamp << "] [" << levelToString(level) << "] [" << filename << ":" << line << "] " << message
           << std::endl;

    if (level == LogLevel::Fatal) {
        stream.flush();
    }
}

void DefaultLogSink::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout.flush();
    std::cerr.flush();
}

void DefaultLogSink::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    min_level_ = level;
}

} // namespace internal

PROFILER_NAMESPACE_END
