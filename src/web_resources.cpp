#include "web_resources.h"
#include <string>

namespace profiler {

static const char INDEX_PAGE[] = R"HTML(
<!DOCTYPE html>
<html>
<head>
    <title>C++ Remote Profiler</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            max-width: 1200px;
            margin: 0 auto;
            padding: 20px;
            background-color: #f5f5f5;
        }
        .container {
            background-color: white;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }
        h1 {
            color: #333;
            border-bottom: 2px solid #4CAF50;
            padding-bottom: 10px;
        }
        .section {
            margin: 20px 0;
            padding: 15px;
            background-color: #f9f9f9;
            border-radius: 4px;
        }
        .section h2 {
            color: #555;
            margin-top: 0;
        }
        button {
            background-color: #4CAF50;
            color: white;
            padding: 10px 20px;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            margin: 5px;
            font-size: 14px;
        }
        button:hover {
            background-color: #45a049;
        }
        button:disabled {
            background-color: #cccccc;
            cursor: not-allowed;
            opacity: 0.6;
        }
        .stop-btn {
            background-color: #f44336;
        }
        .stop-btn:hover {
            background-color: #da190b;
        }
        .status {
            display: inline-block;
            padding: 5px 10px;
            border-radius: 4px;
            margin-left: 10px;
            font-size: 12px;
        }
        .status.running {
            background-color: #ffebee;
            color: #c62828;
        }
        .status.stopped {
            background-color: #e8f5e9;
            color: #2e7d32;
        }
        .output {
            margin-top: 10px;
            padding: 10px;
            background-color: #263238;
            color: #aed581;
            border-radius: 4px;
            font-family: monospace;
            white-space: pre-wrap;
            max-height: 400px;
            overflow-y: auto;
            font-size: 12px;
        }
        .view-btn {
            background-color: #2196F3;
        }
        .view-btn:hover {
            background-color: #0b7dda;
        }
        .info-box {
            background-color: #e3f2fd;
            border-left: 4px solid #2196F3;
            padding: 15px;
            margin: 20px 0;
            border-radius: 4px;
        }
        .info-box h3 {
            margin-top: 0;
            color: #1976D2;
        }
        .info-box code {
            background-color: #f5f5f5;
            padding: 2px 6px;
            border-radius: 3px;
            font-family: monospace;
        }
        .analyze-btn {
            background-color: #9C27B0;
        }
        .analyze-btn:hover {
            background-color: #7B1FA2;
        }
        .download-btn {
            background-color: #FF9800;
        }
        .download-btn:hover {
            background-color: #F57C00;
        }
        .input-group {
            display: inline-block;
            margin: 5px;
        }
        .input-group label {
            margin-right: 5px;
        }
        .input-group input {
            padding: 5px;
            border: 1px solid #ddd;
            border-radius: 4px;
            width: 60px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>C++ Remote Profiler</h1>
        <p>类似 Go pprof 的 C++ 性能分析工具 - 支持 CPU 和 Heap Profiling，自动生成SVG火焰图</p>

        <div class="section">
            <h2>CPU Profiler</h2>
            <div>
                <div class="input-group">
                    <label for="cpu-duration">采样时长(秒):</label>
                    <input type="number" id="cpu-duration" value="10" min="1" max="300">
                </div>
                <button class="analyze-btn" onclick="analyzeCPU()">⚡ 一键分析并生成SVG火焰图</button>
                <button class="download-btn" id="cpu-download-btn" onclick="downloadCPURawSVG()">📥 下载 CPU 原始 SVG</button>
            </div>
        </div>

        <div class="section">
            <h2>Heap Profiler</h2>
            <div>
                <button class="analyze-btn" onclick="analyzeHeap()">⚡ 一键分析并生成Heap火焰图</button>
                <button class="download-btn" id="heap-download-btn" onclick="downloadHeapRawSVG()">📥 下载 Heap 原始 SVG</button>
            </div>
        </div>

        <div class="section">
            <h2>输出</h2>
            <div id="output" class="output">等待操作...</div>
        </div>
    </div>

    <script>
        function analyzeCPU() {
            const duration = document.getElementById('cpu-duration').value;
            log(`🚀 正在进行CPU分析，采样时长: ${duration}秒...\n(这可能需要一些时间，请耐心等待)`);
            document.getElementById('cpu-duration').disabled = true;

            // 打开独立的SVG查看器页面
            window.open(`/show_svg.html?duration=${duration}`, '_blank');

            log('✅ 火焰图查看器已在新标签页打开');
            log('💡 提示：图表中包含 cpuIntensiveTask、FibonacciCalculator、memoryIntensiveTask 等函数');

            document.getElementById('cpu-duration').disabled = false;
        }

        function analyzeHeap() {
            log('🚀 正在获取Heap火焰图...');
            // 打开独立的SVG查看器页面（不需要duration参数）
            window.open('/show_heap_svg.html', '_blank');
            log('✅ Heap火焰图查看器已在新标签页打开');
            log('💡 提示：图表中显示内存分配情况');
        }

        function log(message) {
            const output = document.getElementById('output');
            output.textContent = message;
        }

        function downloadCPURawSVG() {
            const duration = document.getElementById('cpu-duration').value;
            const btn = document.getElementById('cpu-download-btn');

            // 禁用按钮，显示下载中状态
            btn.disabled = true;
            btn.textContent = '⏳ 下载中...';
            log(`📥 正在下载 CPU 原始 SVG (采样时长: ${duration}秒)...`);

            // 使用 fetch 下载文件
            fetch(`/api/cpu/svg_raw?duration=${duration}`)
                .then(response => {
                    if (!response.ok) {
                        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
                    }
                    return response.blob();
                })
                .then(blob => {
                    // 创建下载链接
                    const url = URL.createObjectURL(blob);
                    const a = document.createElement('a');
                    a.href = url;
                    a.download = `cpu_profile_${duration}s.svg`;
                    a.click();
                    URL.revokeObjectURL(url);

                    // 恢复按钮，显示成功
                    btn.disabled = false;
                    btn.textContent = '📥 下载 CPU 原始 SVG';
                    log('✅ CPU 原始 SVG 下载完成');
                })
                .catch(error => {
                    // 错误处理
                    btn.disabled = false;
                    btn.textContent = '📥 下载 CPU 原始 SVG';
                    log(`❌ CPU 原始 SVG 下载失败: ${error.message}`);
                });
        }

        function downloadHeapRawSVG() {
            const btn = document.getElementById('heap-download-btn');

            // 禁用按钮，显示下载中状态
            btn.disabled = true;
            btn.textContent = '⏳ 下载中...';
            log('📥 正在下载 Heap 原始 SVG...');

            // 使用 fetch 下载文件
            fetch('/api/heap/svg_raw')
                .then(response => {
                    if (!response.ok) {
                        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
                    }
                    return response.blob();
                })
                .then(blob => {
                    // 创建下载链接
                    const url = URL.createObjectURL(blob);
                    const a = document.createElement('a');
                    a.href = url;
                    const timestamp = new Date().toISOString().replace(/[:.]/g, '-').slice(0, -5);
                    a.download = `heap_profile_${timestamp}.svg`;
                    a.click();
                    URL.revokeObjectURL(url);

                    // 恢复按钮，显示成功
                    btn.disabled = false;
                    btn.textContent = '📥 下载 Heap 原始 SVG';
                    log('✅ Heap 原始 SVG 下载完成');
                })
                .catch(error => {
                    // 错误处理
                    btn.disabled = false;
                    btn.textContent = '📥 下载 Heap 原始 SVG';
                    log(`❌ Heap 原始 SVG 下载失败: ${error.message}`);
                });
        }
    </script>
</body>
</html>
)HTML";

static const char CPU_SVG_VIEWER_PAGE[] = R"HTML(
<!DOCTYPE html>
<html>
<head>
    <title>CPU Flame Graph Viewer</title>
    <style>
        body { margin: 0; padding: 20px; font-family: Arial, sans-serif; background: #f5f5f5; }
        h1 { color: #333; margin-bottom: 10px; }
        .info { background: #e3f2fd; padding: 15px; border-radius: 5px; margin-bottom: 20px; }
        .info ul { margin: 10px 0; padding-left: 20px; }
        .info li { margin: 5px 0; }
        .toolbar { margin-bottom: 20px; }
        button { padding: 10px 20px; margin-right: 10px; cursor: pointer; font-size: 14px; }
        button:hover { background: #f0f0f0; }
        #svg-container {
            background: white;
            border: 1px solid #ddd;
            border-radius: 5px;
            padding: 0;
            width: 100%;
            overflow: auto;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }
        #svg-container svg {
            display: block;
        }
    </style>
</head>
<body>
    <h1>🔥 CPU 火焰图</h1>
    <div class="info">
        <strong>💡 查看提示：</strong>
        <ul>
            <li><strong>滚动：</strong> 图表可能很大，请滚动查看所有函数</li>
            <li><strong>缩放：</strong> 使用浏览器缩放功能（Ctrl + / Ctrl -）或鼠标滚轮（按住 Ctrl）</li>
            <li><strong>拖拽：</strong> SVG 支持鼠标拖拽</li>
            <li><strong>搜索：</strong> 使用 Ctrl+F 搜索特定函数</li>
        </ul>
    </div>
    <div class="toolbar">
        <button onclick="loadSVG()">🔄 重新加载</button>
        <button onclick="window.print()">🖨️ 打印</button>
        <button onclick="downloadSVG()">⬇️ 下载 SVG</button>
    </div>
    <div id="svg-container">加载中...</div>

    <script>
        const urlParams = new URLSearchParams(window.location.search);
        const duration = urlParams.get('duration') || '10';

        function loadSVG() {
            const container = document.getElementById('svg-container');
            container.innerHTML = '正在加载火焰图...';

            fetch(`/api/cpu/analyze?duration=${duration}`)
                .then(response => response.text())
                .then(svgText => {
                    // 修复 pprof SVG 的负坐标 viewBox
                    svgText = svgText.replace(/viewBox="0 -1000 2000 1000"/g, 'viewBox="0 0 2000 1000"');

                    container.innerHTML = svgText;

                    const svg = container.querySelector('svg');
                    if (svg) {
                        svg.removeAttribute('width');
                        svg.removeAttribute('height');
                        svg.style.width = '100%';
                        svg.style.height = 'auto';
                        svg.style.display = 'block';
                        console.log('SVG loaded and viewBox fixed');
                    }
                })
                .catch(error => {
                    container.innerHTML = `<div style="color: red; padding: 20px;">❌ 加载失败: ${error.message}</div>`;
                });
        }

        function downloadSVG() {
            const svg = document.querySelector('#svg-container svg');
            if (!svg) {
                alert('没有找到 SVG 图表');
                return;
            }
            const serializer = new XMLSerializer();
            const svgStr = serializer.serializeToString(svg);
            const blob = new Blob([svgStr], {type: 'image/svg+xml'});
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = `cpu_flamegraph_${duration}s.svg`;
            a.click();
            URL.revokeObjectURL(url);
        }

        // 页面加载时自动加载 SVG
        window.addEventListener('load', loadSVG);
    </script>
</body>
</html>
)HTML";

static const char HEAP_SVG_VIEWER_PAGE[] = R"HTML(
<!DOCTYPE html>
<html>
<head>
    <title>Heap Flame Graph Viewer</title>
    <style>
        body { margin: 0; padding: 20px; font-family: Arial, sans-serif; background: #f5f5f5; }
        h1 { color: #333; margin-bottom: 10px; }
        .info { background: #fff3e0; padding: 15px; border-radius: 5px; margin-bottom: 20px; }
        .info ul { margin: 10px 0; padding-left: 20px; }
        .info li { margin: 5px 0; }
        .toolbar { margin-bottom: 20px; }
        button { padding: 10px 20px; margin-right: 10px; cursor: pointer; font-size: 14px; }
        button:hover { background: #f0f0f0; }
        #svg-container {
            background: white;
            border: 1px solid #ddd;
            border-radius: 5px;
            padding: 20px;
            overflow: auto;
            max-width: 100%;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
            min-height: 600px;
            max-height: 80vh;
        }
        svg {
            display: block;
            margin: 0 auto;
            min-width: 100%;
            min-height: 100%;
        }
    </style>
</head>
<body>
    <h1>🔥 Heap 内存火焰图</h1>
    <div class="info">
        <strong>💡 查看提示：</strong>
        <ul>
            <li><strong>滚动：</strong> 图表可能很大，请滚动查看所有函数</li>
            <li><strong>缩放：</strong> 使用鼠标滚轮可以缩放视图</li>
            <li><strong>拖拽：</strong> 按住鼠标左键可以拖动图表</li>
            <li><strong>搜索：</strong> 使用 Ctrl+F 搜索特定函数（如 cpuIntensiveTask、FibonacciCalculator）</li>
            <li><strong>点击：</strong> 点击节点可以高亮显示相关调用</li>
        </ul>
    </div>
    <div class="toolbar">
        <button onclick="loadSVG()">🔄 重新加载</button>
        <button onclick="zoomIn()">🔍+ 放大</button>
        <button onclick="zoomOut()">🔍- 缩小</button>
        <button onclick="resetZoom()">1:1 原始大小</button>
        <button onclick="fitToWidth()">↔️ 适应宽度</button>
        <button onclick="window.print()">🖨️ 打印</button>
        <button onclick="downloadSVG()">⬇️ 下载 SVG</button>
    </div>
    <div id="svg-container">加载中...</div>

    <script>
        let currentZoom = 1.0;
        let svgElement = null;
        let viewportElement = null;

        function loadSVG() {
            document.getElementById('svg-container').innerHTML = '正在加载Heap火焰图...';

            // Heap分析不需要duration参数，直接调用接口
            fetch('/api/heap/analyze')
                .then(response => {
                    const contentType = response.headers.get('Content-Type');
                    if (contentType && contentType.includes('json')) {
                        return response.json().then(data => {
                            throw new Error(data.error || '未知错误');
                        });
                    }
                    return response.text();
                })
                .then(svgText => {
                    const container = document.getElementById('svg-container');
                    container.innerHTML = svgText;

                    // 调整 SVG 的显示，确保完整渲染
                    setTimeout(() => {
                        const svg = container.querySelector('svg');
                        svgElement = svg;
                        if (svg) {
                            // 移除固定的宽高，让SVG自适应
                            svg.removeAttribute('width');
                            svg.removeAttribute('height');

                            // 设置一个合理的最小尺寸
                            svg.style.minWidth = '100%';
                            svg.style.minHeight = '600px';

                            // 查找并调整 viewport transform
                            const viewport = svg.querySelector('#viewport');
                            viewportElement = viewport;
                            if (viewport) {
                                // 获取当前的 transform
                                const transform = viewport.getAttribute('transform');
                                console.log('Original transform:', transform);

                                // 不修改 transform，保持 pprof 的原始布局
                                // 但添加一些样式让显示更好
                                viewport.style.transformBox = 'fill-box';
                                viewport.style.transformOrigin = 'top left';
                            }

                            // 自动滚动到包含我们函数的区域
                            const titles = svg.querySelectorAll('title');
                            let found = false;
                            for (let title of titles) {
                                const text = title.textContent;
                                if (text.includes('cpuIntensiveTask') ||
                                    text.includes('FibonacciCalculator') ||
                                    text.includes('memoryIntensiveTask') ||
                                    text.includes('DataProcessor') ||
                                    text.includes('MatrixOperations')) {
                                    const node = title.closest('g');
                                    if (node && !found) {
                                        node.scrollIntoView({behavior: 'smooth', block: 'center'});
                                        found = true;

                                        // 高亮显示找到的节点
                                        node.style.outline = '3px solid red';
                                        node.style.outlineOffset = '2px';
                                    }
                                }
                            }

                            if (found) {
                                console.log('Found and scrolled to target function');
                            } else {
                                console.log('Target function not found, showing all nodes');
                                // 如果没找到目标函数，滚动到中间
                                const viewport = svg.querySelector('#viewport');
                                if (viewport) {
                                    viewport.scrollIntoView({behavior: 'smooth', block: 'center'});
                                }
                            }
                        }
                    }, 300);
                })
                .catch(error => {
                    document.getElementById('svg-container').innerHTML =
                        `<div style="color: red; padding: 20px;">❌ 加载失败: ${error.message}</div>`;
                });
        }

        function zoomIn() {
            if (!svgElement) return;
            currentZoom *= 1.2;
            applyZoom();
        }

        function zoomOut() {
            if (!svgElement) return;
            currentZoom /= 1.2;
            applyZoom();
        }

        function resetZoom() {
            if (!svgElement) return;
            currentZoom = 1.0;
            applyZoom();
        }

        function fitToWidth() {
            if (!svgElement || !viewportElement) return;
            const container = document.getElementById('svg-container');
            const containerWidth = container.clientWidth - 40; // padding

            // 获取 SVG 的实际宽度
            const bbox = viewportElement.getBBox();
            const svgWidth = bbox.width;

            currentZoom = containerWidth / svgWidth;
            applyZoom();
        }

        function applyZoom() {
            if (!viewportElement) return;

            // 获取原始的 transform
            const originalTransform = viewportElement.getAttribute('transform') || '';

            // 解析原始的 scale
            const scaleMatch = originalTransform.match(/scale\(([^)]+)\)/);
            const baseScale = scaleMatch ? parseFloat(scaleMatch[1]) : 1.0;

            // 解析原始的 translate
            const translateMatch = originalTransform.match(/translate\(([^)]+)\)/);
            const baseTranslate = translateMatch ? translateMatch[1] : '0,0';

            // 应用新的缩放
            const newScale = baseScale * currentZoom;
            viewportElement.setAttribute('transform',
                `scale(${newScale},${newScale}) translate(${baseTranslate})`);

            console.log(`Applied zoom: ${currentZoom}x (${newScale})`);
        }

        function downloadSVG() {
            const svg = document.querySelector('svg');
            if (!svg) {
                alert('没有找到 SVG 图表');
                return;
            }
            const serializer = new XMLSerializer();
            const svgStr = serializer.serializeToString(svg);
            const blob = new Blob([svgStr], {type: 'image/svg+xml'});
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            // 使用时间戳生成文件名
            const timestamp = new Date().toISOString().replace(/[:.]/g, '-').slice(0, -5);
            a.download = `heap_flamegraph_${timestamp}.svg`;
            a.click();
            URL.revokeObjectURL(url);
        }

        // 页面加载时自动加载 SVG
        window.addEventListener('load', loadSVG);
    </script>
</body>
</html>
)HTML";


std::string WebResources::getIndexPage() {
    return std::string(INDEX_PAGE);
}

std::string WebResources::getCpuSvgViewerPage() {
    return std::string(CPU_SVG_VIEWER_PAGE);
}

std::string WebResources::getHeapSvgViewerPage() {
    return std::string(HEAP_SVG_VIEWER_PAGE);
}

} // namespace profiler
