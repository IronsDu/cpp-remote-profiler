/// @file default_log_sink.cpp
/// @brief Default log sink implementation using spdlog

#include "default_log_sink.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/pattern_formatter.h>
#include <mutex>

PROFILER_NAMESPACE_BEGIN

namespace internal {

// Helper to convert profiler LogLevel to spdlog level
static spdlog::level::level_enum toSpdlogLevel(LogLevel level) {
    switch (level) {
        case LogLevel::Trace:
            return spdlog::level::trace;
        case LogLevel::Debug:
            return spdlog::level::debug;
        case LogLevel::Info:
            return spdlog::level::info;
        case LogLevel::Warning:
            return spdlog::level::warn;
        case LogLevel::Error:
            return spdlog::level::err;
        case LogLevel::Fatal:
            return spdlog::level::critical;
        default:
            return spdlog::level::info;
    }
}

// Helper to convert LogLevel to string
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

class DefaultLogSink::Impl {
public:
    Impl() {
        // Create stderr sink with color
        auto stderr_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();

        // Create logger with custom pattern
        // Format: [timestamp] [level] [source_location] message
        logger_ = std::make_shared<spdlog::logger>("profiler", stderr_sink);
        logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%-7l%$] [%s:%#] %v");
        logger_->set_level(spdlog::level::trace);  // Let LogManager handle filtering

        // Register as default logger (optional, for spdlog internal use)
        spdlog::register_logger(logger_);
    }

    ~Impl() {
        spdlog::drop("profiler");
    }

    void log(LogLevel level,
             const char* file,
             int line,
             const char* function,
             const char* message) {
        // Extract just the filename from the full path
        const char* filename = file;
        if (const char* last_slash = strrchr(file, '/')) {
            filename = last_slash + 1;
        }

        // Create source location for spdlog
        spdlog::source_loc source_loc{filename, line, function};

        // Log with spdlog
        logger_->log(source_loc, toSpdlogLevel(level), "{}", message);

        // For fatal errors, flush immediately
        if (level == LogLevel::Fatal) {
            logger_->flush();
        }
    }

    void flush() {
        logger_->flush();
    }

    void setLogLevel(LogLevel level) {
        logger_->set_level(toSpdlogLevel(level));
    }

private:
    std::shared_ptr<spdlog::logger> logger_;
};

DefaultLogSink::DefaultLogSink()
    : impl_(std::make_unique<Impl>()) {
}

DefaultLogSink::~DefaultLogSink() = default;

void DefaultLogSink::log(LogLevel level,
                         const char* file,
                         int line,
                         const char* function,
                         const char* message) {
    std::lock_guard<std::mutex> lock(mutex_);
    impl_->log(level, file, line, function, message);
}

void DefaultLogSink::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    impl_->flush();
}

void DefaultLogSink::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    impl_->setLogLevel(level);
}

}  // namespace internal

PROFILER_NAMESPACE_END
