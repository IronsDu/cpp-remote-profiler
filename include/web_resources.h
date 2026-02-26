#pragma once

#include <string>

namespace profiler {

// Web 资源管理类 - 提供嵌入的 HTML 页面内容
class WebResources {
  public:
    // 获取主页面 HTML
    static std::string getIndexPage();

    // 获取 CPU 火焰图查看器 HTML
    static std::string getCpuSvgViewerPage();

    // 获取 Heap 火焰图查看器 HTML
    static std::string getHeapSvgViewerPage();

    // 获取 Growth 火焰图查看器 HTML
    static std::string getGrowthSvgViewerPage();
};

} // namespace profiler
