/// @file log_manager.cpp
/// @brief Internal log manager implementation

#include "log_manager.h"
#include "default_log_sink.h"
#include <utility>

PROFILER_NAMESPACE_BEGIN

namespace internal {

LogManager& LogManager::instance() {
    static LogManager instance;
    return instance;
}

LogManager::LogManager() : sink_(std::make_shared<DefaultLogSink>()) {}

void LogManager::setSink(std::shared_ptr<LogSink> sink) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sink) {
        sink_ = std::move(sink);
    } else {
        // Revert to default sink
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

void logMessage(LogLevel level, const char* file, int line, const char* function, std::string&& message) {
    LogManager::instance().sink()->log(level, file, line, function, message.c_str());
}

} // namespace internal

// Implementation of public API functions (in profiler namespace)
void setSink(std::shared_ptr<LogSink> sink) {
    internal::LogManager::instance().setSink(std::move(sink));
}

void setLogLevel(LogLevel level) {
    internal::LogManager::instance().setLogLevel(level);
}

PROFILER_NAMESPACE_END
