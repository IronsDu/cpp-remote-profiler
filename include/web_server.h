#pragma once

#include "profiler_manager.h"
#include "profiler_version.h"
#include <drogon/drogon.h>

PROFILER_NAMESPACE_BEGIN

// 注册所有 HTTP 路由处理器
// 这个函数会注册所有与 profiling 相关的 API 端点和 Web 页面
void registerHttpHandlers(profiler::ProfilerManager& profiler);

PROFILER_NAMESPACE_END
