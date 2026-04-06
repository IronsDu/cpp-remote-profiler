#include "log_manager.h"
#include "default_log_sink.h"
#include <utility>

PROFILER_NAMESPACE_BEGIN

namespace internal {

LogManager::LogManager() : sink_(std::make_shared<DefaultLogSink>()) {}

void LogManager::setSink(std::shared_ptr<LogSink> sink) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sink) {
        sink_ = std::move(sink);
    } else {
        sink_ = std::make_shared<DefaultLogSink>();
    }
}

LogSink* LogManager::sink() {
    std::lock_guard<std::mutex> lock(mutex_);
    return sink_.get();
}

void LogManager::setLogLevel(LogLevel level) {
    level_.store(level, std::memory_order_relaxed);
}

LogLevel LogManager::logLevel() const {
    return level_.load(std::memory_order_relaxed);
}

bool LogManager::shouldLog(LogLevel level) const {
    return static_cast<int>(level) >= static_cast<int>(level_.load(std::memory_order_relaxed));
}

} // namespace internal

PROFILER_NAMESPACE_END
