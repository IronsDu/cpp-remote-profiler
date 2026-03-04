#include "web_resources.h"
#include <string>

PROFILER_NAMESPACE_BEGIN

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
        select {
            padding: 8px 12px;
            border: 1px solid #ddd;
            border-radius: 4px;
            min-width: 200px;
            font-size: 14px;
            background-color: white;
            cursor: pointer;
            font-family: Arial, sans-serif;
        }
        select:hover {
            border-color: #4CAF50;
        }
        select:focus {
            outline: none;
            border-color: #4CAF50;
            box-shadow: 0 0 5px rgba(76, 175, 80, 0.3);
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
                <div class="input-group">
                    <label for="cpu-chart-type">图表类型:</label>
                    <select id="cpu-chart-type">
                        <option value="pprof">pprof SVG</option>
                        <option value="flamegraph">FlameGraph (Brendan Gregg)</option>
                    </select>
                </div>
                <button class="analyze-btn" onclick="analyzeCPU()">⚡ 一键分析并生成火焰图</button>
                <button class="download-btn" id="cpu-download-btn" onclick="downloadCPUChart()">📥 下载 CPU 图表</button>
            </div>
        </div>

        <div class="section">
            <h2>Heap Profiler</h2>
            <div>
                <div class="input-group">
                    <label for="heap-chart-type">图表类型:</label>
                    <select id="heap-chart-type">
                        <option value="pprof">pprof SVG</option>
                        <option value="flamegraph">FlameGraph (Brendan Gregg)</option>
                    </select>
                </div>
                <button class="analyze-btn" onclick="analyzeHeap()">⚡ 一键分析并生成Heap火焰图</button>
                <button class="download-btn" id="heap-download-btn" onclick="downloadHeapChart()">📥 下载 Heap 图表</button>
            </div>
        </div>

        <div class="section">
            <h2>Heap Growth Profiler</h2>
            <div>
                <div class="input-group">
                    <label for="growth-chart-type">图表类型:</label>
                    <select id="growth-chart-type">
                        <option value="pprof">pprof SVG</option>
                        <option value="flamegraph">FlameGraph (Brendan Gregg)</option>
                    </select>
                </div>
                <button class="analyze-btn" onclick="analyzeGrowth()">⚡ 一键分析并生成Growth火焰图</button>
                <button class="download-btn" id="growth-download-btn" onclick="downloadGrowthChart()">📥 下载 Growth 图表</button>
            </div>
        </div>

        <div class="section">
            <h2>Thread Stacks</h2>
            <div>
                <button class="view-btn" onclick="getThreadStacks()">🧵 获取所有线程堆栈</button>
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
            const chartType = document.getElementById('cpu-chart-type').value;
            log(`🚀 正在进行CPU分析，采样时长: ${duration}秒, 图表类型: ${chartType}...\n(这可能需要一些时间，请耐心等待)`);
            document.getElementById('cpu-duration').disabled = true;

            // 打开独立的SVG查看器页面，传递 output_type 参数
            window.open(`/show_svg.html?duration=${duration}&output_type=${chartType}`, '_blank');

            log('✅ 火焰图查看器已在新标签页打开');
            log(`💡 提示：当前使用 ${chartType === 'flamegraph' ? 'Brendan Gregg FlameGraph' : 'pprof SVG'}`);

            document.getElementById('cpu-duration').disabled = false;
        }

        function analyzeHeap() {
            const chartType = document.getElementById('heap-chart-type').value;
            log(`🚀 正在获取Heap火焰图 (图表类型: ${chartType})...`);
            // 打开独立的SVG查看器页面，传递 output_type 参数
            window.open(`/show_heap_svg.html?output_type=${chartType}`, '_blank');
            log('✅ Heap火焰图查看器已在新标签页打开');
            log(`💡 提示：当前使用 ${chartType === 'flamegraph' ? 'Brendan Gregg FlameGraph' : 'pprof SVG'}`);
        }

        function analyzeGrowth() {
            const chartType = document.getElementById('growth-chart-type').value;
            log(`🚀 正在获取Heap Growth火焰图 (图表类型: ${chartType})...`);
            // 打开独立的SVG查看器页面，传递 output_type 参数
            window.open(`/show_growth_svg.html?output_type=${chartType}`, '_blank');
            log('✅ Heap Growth火焰图查看器已在新标签页打开');
            log(`💡 提示：当前使用 ${chartType === 'flamegraph' ? 'Brendan Gregg FlameGraph' : 'pprof SVG'}`);
        }

        function log(message) {
            const output = document.getElementById('output');
            output.textContent = message;
        }

        function downloadCPUChart() {
            const duration = document.getElementById('cpu-duration').value;
            const chartType = document.getElementById('cpu-chart-type').value;
            const btn = document.getElementById('cpu-download-btn');

            // 禁用按钮，显示下载中状态
            btn.disabled = true;
            const originalText = btn.textContent;
            btn.textContent = '⏳ 准备下载...';

            // 根据图表类型选择端点和文件名
            const endpoint = chartType === 'flamegraph' ? '/api/cpu/flamegraph_raw' : '/api/cpu/svg_raw';
            const chartTypeName = chartType === 'flamegraph' ? 'FlameGraph' : 'pprof SVG';
            const filenamePrefix = chartType === 'flamegraph' ? 'cpu_flamegraph' : 'cpu_profile';

            log(`📥 开始下载 CPU ${chartTypeName} (采样时长: ${duration}秒)...`);

            // 估计下载时间（基于采样时长）
            const estimatedTime = Math.max(3, parseInt(duration) + 2); // 至少3秒
            let countdown = estimatedTime;
            let progressInterval;

            // 启动倒计时
            progressInterval = setInterval(() => {
                countdown--;
                const progress = Math.round(((estimatedTime - countdown) / estimatedTime) * 100);
                btn.textContent = `⏳ 生成中 ${progress}% (${countdown}s)`;
            }, 1000);

            // 使用 fetch 下载文件
            fetch(`${endpoint}?duration=${duration}`)
                .then(response => {
                    if (!response.ok) {
                        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
                    }
                    return response.blob();
                })
                .then(blob => {
                    // 清除倒计时
                    clearInterval(progressInterval);
                    btn.textContent = '⏳ 保存文件...';

                    // 创建下载链接
                    const url = URL.createObjectURL(blob);
                    const a = document.createElement('a');
                    a.href = url;
                    a.download = `${filenamePrefix}_${duration}s.svg`;
                    a.click();
                    URL.revokeObjectURL(url);

                    // 恢复按钮，显示成功
                    btn.disabled = false;
                    btn.textContent = originalText;
                    log(`✅ CPU ${chartTypeName} 下载完成 (大小: ${(blob.size / 1024).toFixed(1)} KB)`);
                })
                .catch(error => {
                    // 清除倒计时
                    clearInterval(progressInterval);
                    // 错误处理
                    btn.disabled = false;
                    btn.textContent = originalText;
                    log(`❌ CPU ${chartTypeName} 下载失败: ${error.message}`);
                });
        }

        function downloadHeapChart() {
            const chartType = document.getElementById('heap-chart-type').value;
            const btn = document.getElementById('heap-download-btn');

            // 禁用按钮，显示下载中状态
            btn.disabled = true;
            const originalText = btn.textContent;
            btn.textContent = '⏳ 准备下载...';

            // 根据图表类型选择端点和文件名
            const endpoint = chartType === 'flamegraph' ? '/api/heap/flamegraph_raw' : '/api/heap/svg_raw';
            const chartTypeName = chartType === 'flamegraph' ? 'FlameGraph' : 'pprof SVG';
            const filenamePrefix = chartType === 'flamegraph' ? 'heap_flamegraph' : 'heap_profile';

            log(`📥 开始下载 Heap ${chartTypeName}...`);

            // 估计下载时间（Heap profile 通常较快）
            const estimatedTime = 5; // 固定5秒
            let countdown = estimatedTime;
            let progressInterval;

            // 启动倒计时
            progressInterval = setInterval(() => {
                countdown--;
                const progress = Math.round(((estimatedTime - countdown) / estimatedTime) * 100);
                btn.textContent = `⏳ 生成中 ${progress}% (${countdown}s)`;
            }, 1000);

            // 使用 fetch 下载文件
            fetch(endpoint)
                .then(response => {
                    if (!response.ok) {
                        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
                    }
                    return response.blob();
                })
                .then(blob => {
                    // 清除倒计时
                    clearInterval(progressInterval);
                    btn.textContent = '⏳ 保存文件...';

                    // 创建下载链接
                    const url = URL.createObjectURL(blob);
                    const a = document.createElement('a');
                    a.href = url;
                    const timestamp = new Date().toISOString().replace(/[:.]/g, '-').slice(0, -5);
                    a.download = `${filenamePrefix}_${timestamp}.svg`;
                    a.click();
                    URL.revokeObjectURL(url);

                    // 恢复按钮，显示成功
                    btn.disabled = false;
                    btn.textContent = originalText;
                    log(`✅ Heap ${chartTypeName} 下载完成 (大小: ${(blob.size / 1024).toFixed(1)} KB)`);
                })
                .catch(error => {
                    // 清除倒计时
                    clearInterval(progressInterval);
                    // 错误处理
                    btn.disabled = false;
                    btn.textContent = originalText;
                    log(`❌ Heap ${chartTypeName} 下载失败: ${error.message}`);
                });
        }

        function downloadGrowthChart() {
            const chartType = document.getElementById('growth-chart-type').value;
            const btn = document.getElementById('growth-download-btn');

            // 禁用按钮，显示下载中状态
            btn.disabled = true;
            const originalText = btn.textContent;
            btn.textContent = '⏳ 准备下载...';

            // 根据图表类型选择端点和文件名
            const endpoint = chartType === 'flamegraph' ? '/api/growth/flamegraph_raw' : '/api/growth/svg_raw';
            const chartTypeName = chartType === 'flamegraph' ? 'FlameGraph' : 'pprof SVG';
            const filenamePrefix = chartType === 'flamegraph' ? 'growth_flamegraph' : 'growth_profile';

            log(`📥 开始下载 Growth ${chartTypeName}...`);

            // 估计下载时间（Growth profile 通常较快）
            const estimatedTime = 5; // 固定5秒
            let countdown = estimatedTime;
            let progressInterval;

            // 启动倒计时
            progressInterval = setInterval(() => {
                countdown--;
                const progress = Math.round(((estimatedTime - countdown) / estimatedTime) * 100);
                btn.textContent = `⏳ 生成中 ${progress}% (${countdown}s)`;
            }, 1000);

            // 使用 fetch 下载文件
            fetch(endpoint)
                .then(response => {
                    if (!response.ok) {
                        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
                    }
                    return response.blob();
                })
                .then(blob => {
                    // 清除倒计时
                    clearInterval(progressInterval);
                    btn.textContent = '⏳ 保存文件...';

                    // 创建下载链接
                    const url = URL.createObjectURL(blob);
                    const a = document.createElement('a');
                    a.href = url;
                    const timestamp = new Date().toISOString().replace(/[:.]/g, '-').slice(0, -5);
                    a.download = `${filenamePrefix}_${timestamp}.svg`;
                    a.click();
                    URL.revokeObjectURL(url);

                    // 恢复按钮，显示成功
                    btn.disabled = false;
                    btn.textContent = originalText;
                    log(`✅ Growth ${chartTypeName} 下载完成 (大小: ${(blob.size / 1024).toFixed(1)} KB)`);
                })
                .catch(error => {
                    // 清除倒计时
                    clearInterval(progressInterval);
                    // 错误处理
                    btn.disabled = false;
                    btn.textContent = originalText;
                    log(`❌ Growth ${chartTypeName} 下载失败: ${error.message}`);
                });
        }

        function getThreadStacks() {
            log('🚀 正在获取所有线程堆栈...');

            fetch('/api/thread/stacks')
                .then(response => {
                    if (!response.ok) {
                        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
                    }
                    return response.text();
                })
                .then(text => {
                    log('✅ 线程堆栈获取成功:\n\n' + text);
                })
                .catch(error => {
                    log(`❌ 获取线程堆栈失败: ${error.message}`);
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
        const outputType = urlParams.get('output_type') || 'pprof';

        function loadSVG() {
            const container = document.getElementById('svg-container');
            container.innerHTML = '正在加载火焰图...';

            fetch(`/api/cpu/analyze?duration=${duration}&output_type=${outputType}`)
                .then(response => response.text())
                .then(svgText => {
                    // Only fix viewBox for pprof SVG (not FlameGraph)
                    if (outputType === 'pprof') {
                        svgText = svgText.replace(/viewBox="0 -1000 2000 1000"/g, 'viewBox="0 0 2000 1000"');
                    }

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

        const urlParams = new URLSearchParams(window.location.search);
        const outputType = urlParams.get('output_type') || 'pprof';

        function loadSVG() {
            document.getElementById('svg-container').innerHTML = '正在加载Heap火焰图...';

            // Heap分析传递output_type参数
            fetch(`/api/heap/analyze?output_type=${outputType}`)
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

static const char GROWTH_SVG_VIEWER_PAGE[] = R"HTML(
<!DOCTYPE html>
<html>
<head>
    <title>Growth Flame Graph Viewer</title>
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
    <h1>🔥 Heap Growth 堆增长火焰图</h1>
    <div class="info">
        <strong>💡 查看提示：</strong>
        <ul>
            <li><strong>滚动：</strong> 图表可能很大，请滚动查看所有函数</li>
            <li><strong>缩放：</strong> 使用鼠标滚轮可以缩放视图</li>
            <li><strong>拖拽：</strong> 按住鼠标左键可以拖动图表</li>
            <li><strong>搜索：</strong> 使用 Ctrl+F 搜索特定函数</li>
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

        const urlParams = new URLSearchParams(window.location.search);
        const outputType = urlParams.get('output_type') || 'pprof';

        function loadSVG() {
            document.getElementById('svg-container').innerHTML = '正在加载Growth火焰图...';

            // Growth分析传递output_type参数
            fetch(`/api/growth/analyze?output_type=${outputType}`)
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
                        }
                    }, 100);
                })
                .catch(error => {
                    document.getElementById('svg-container').innerHTML =
                        `<div style="color: red; padding: 20px;">❌ 加载失败: ${error.message}</div>`;
                    console.error('Error loading SVG:', error);
                });
        }

        function zoomIn() {
            currentZoom *= 1.2;
            applyZoom();
        }

        function zoomOut() {
            currentZoom /= 1.2;
            applyZoom();
        }

        function resetZoom() {
            currentZoom = 1.0;
            applyZoom();
        }

        function fitToWidth() {
            const container = document.getElementById('svg-container');
            const svg = container.querySelector('svg');
            if (!svg) return;

            const containerWidth = container.clientWidth - 40;
            const svgWidth = svg.getBBox().width;

            currentZoom = containerWidth / svgWidth;
            applyZoom();
        }

        function applyZoom() {
            if (!viewportElement) return;

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
            a.download = `growth_flamegraph_${timestamp}.svg`;
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

std::string WebResources::getGrowthSvgViewerPage() {
    return std::string(GROWTH_SVG_VIEWER_PAGE);
}

PROFILER_NAMESPACE_END
