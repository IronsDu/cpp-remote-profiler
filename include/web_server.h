#pragma once

#include <drogon/drogon.h>
#include "profiler_manager.h"

namespace profiler {

// 注册所有 HTTP 路由处理器
// 这个函数会注册所有与 profiling 相关的 API 端点和 Web 页面
void registerHttpHandlers(profiler::ProfilerManager& profiler);

} // namespace profiler
