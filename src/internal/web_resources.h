#pragma once

#include "profiler_version.h"
#include <string>

PROFILER_NAMESPACE_BEGIN

// Web 资源管理类 - 提供嵌入的 HTML 页面内容
class WebResources {
public:
    /// The single control panel page. Charts are delivered as downloadable or
    /// inline SVG by the analysis endpoints; there are no separate viewer pages.
    static std::string getIndexPage();
};

PROFILER_NAMESPACE_END
