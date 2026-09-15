#include "internal/web_resources.h"
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
        .card {
            border: 1px solid #e0e0e0;
            border-radius: 4px;
            padding: 12px;
            margin: 10px 0;
            background-color: #fff;
        }
        .card h3 {
            margin: 0 0 6px;
            font-size: 15px;
            color: #333;
        }
        .hint {
            display: block;
            margin: 6px 0 10px;
            color: #666;
            font-size: 12px;
            line-height: 1.6;
        }
        .hint code {
            background: #eee;
            padding: 1px 4px;
            border-radius: 3px;
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
                    <label for="cpu-renderer">图形:</label>
                    <select id="cpu-renderer">
                        <option value="flamegraph">火焰图 (FlameGraph)</option>
                        <option value="callgraph">调用图 (callgraph)</option>
                    </select>
                </div>
                <span class="hint">
                    两种图形是<b>不同的表达</b>：火焰图是栈帧堆叠，调用图是节点/边的有向图（且能解析内联帧）。
                    CPU 采样是<b>独占</b>的：若已有请求或宿主程序正在采样，本次会被拒绝（409）。
                    时长太短会采不到样本，建议 ≥10 秒。
                </span>
                <button class="view-btn" onclick="openChart('cpu')">🔗 打开 CPU 图表</button>
                <button class="download-btn" id="cpu-download-btn" onclick="downloadChart('cpu')">📥 下载 CPU 图表</button>
            </div>
        </div>

        <div class="section">
            <h2>Heap Profiler</h2>
            <div>
                <div class="input-group">
                    <label for="heap-duration">采集窗口(秒):</label>
                    <input type="number" id="heap-duration" value="10" min="1" max="300">
                </div>
                <div class="input-group">
                    <label for="heap-renderer">图形:</label>
                    <select id="heap-renderer">
                        <option value="flamegraph">火焰图 (FlameGraph)</option>
                        <option value="callgraph">调用图 (callgraph)</option>
                    </select>
                </div>
                <span class="hint">
                    ⚠️ 只记录<b>采集窗口内发生的分配</b>（流量），不反映此刻已经在堆里的内存（存量）。
                    想看当前堆快照请用下面的 Heap Snapshot。分配稀疏时把窗口调长。
                </span>
                <button class="view-btn" onclick="openChart('heap')">🔗 打开 Heap 图表</button>
                <button class="download-btn" id="heap-download-btn" onclick="downloadChart('heap')">📥 下载 Heap 图表</button>
            </div>
        </div>

        <div class="section">
            <h2>Heap Snapshot</h2>
            <div>
                <span class="hint">
                    状态式：反映<b>堆里现存的内存</b>（tcmalloc 累计采样），对应
                    <code>GET /api/pprof/heap/snapshot</code>。
                    <b>需要</b>在进程启动前设置 <code>TCMALLOC_SAMPLE_PARAMETER</code>，否则返回 500。
                    刚启动的进程可能显示空快照（tcmalloc 在第一次采样事件前不报告数据），稍等再试即可。
                </span>
                <div class="input-group">
                    <label for="snapshot-output">快照输出:</label>
                    <select id="snapshot-output">
                        <option value="profile">原始 profile 文本</option>
                        <option value="flamegraph">火焰图 (FlameGraph)</option>
                        <option value="callgraph">调用图 (callgraph)</option>
                    </select>
                </div>
                <button class="view-btn" onclick="openSnapshot()">🔗 打开</button>
                <button class="download-btn" id="snapshot-btn" onclick="downloadSnapshot()">📥 下载</button>
            </div>
        </div>

        <div class="section">
            <h2>Heap Growth Profiler</h2>
            <div>
                <div class="input-group">
                    <label for="growth-renderer">图形:</label>
                    <select id="growth-renderer">
                        <option value="flamegraph">火焰图 (FlameGraph)</option>
                        <option value="callgraph">调用图 (callgraph)</option>
                    </select>
                </div>
                <span class="hint">堆增长栈分析，不需要 <code>TCMALLOC_SAMPLE_PARAMETER</code>，即时获取。</span>
                <button class="view-btn" onclick="openChart('growth')">🔗 打开 Growth 图表</button>
                <button class="download-btn" id="growth-download-btn" onclick="downloadChart('growth')">📥 下载 Growth 图表</button>
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
        // ------------------------------------------------------------------
        // 图表下载：三个 profiler 共用一条路径 /api/pprof/{cpu,heap,growth}
        //   renderer = flamegraph | callgraph
        //   duration = 采样/采集窗口（秒）
        //   output   = inline | attachment
        // Heap 快照是独立端点 /api/pprof/heap/snapshot（format=profile|svg）。
        // ------------------------------------------------------------------
        const CHART_SPECS = {
            cpu:    { durationId: 'cpu-duration',    rendererId: 'cpu-renderer',    label: 'CPU' },
            heap:   { durationId: 'heap-duration',   rendererId: 'heap-renderer',   label: 'Heap' },
            growth: { durationId: null,              rendererId: 'growth-renderer', label: 'Heap Growth' },
        };

        function chartDuration(spec) {
            if (!spec.durationId) return null;          // growth 采集是即时的，没有窗口
            const v = parseInt(document.getElementById(spec.durationId).value, 10);
            return Number.isFinite(v) && v >= 1 ? Math.min(v, 300) : 10;
        }

        function chartUrl(kind, { output = 'attachment' } = {}) {
            const spec = CHART_SPECS[kind];
            const p = new URLSearchParams();
            p.set('renderer', document.getElementById(spec.rendererId).value);
            p.set('output', output);
            const d = chartDuration(spec);
            if (d !== null) p.set('duration', String(d));
            return `/api/pprof/${kind}?${p.toString()}`;
        }

        function log(message) {
            const output = document.getElementById('output');
            output.textContent = message;
        }

        /// 取回一份产物并以指定文件名存盘。
        /// <a> 必须入 DOM 再 click：游离节点在部分浏览器不会启动下载。
        function saveBlob(blob, filename) {
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = filename;
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            URL.revokeObjectURL(url);
        }

        function errorTextFrom(response, body) {
            let detail = `HTTP ${response.status}`;
            try {
                detail = JSON.parse(body).error || detail;
            } catch (e) {
                if (body) detail = body.trim().slice(0, 200);
            }
            return detail;
        }

        /// 生成文件名用的时间戳，**精确到毫秒**。
        ///
        /// 只精确到秒是不够的：连续下载两次（例如先取火焰图快照、再取调用图快照，
        /// 两次都可能在同 1 秒内完成）会得到**同一个文件名**，浏览器于是用第二个
        /// 覆盖第一个——表现为"点了下载，文件却没多出来"。
        /// ISO 串形如 2026-09-15T14:01:16.123Z，去掉结尾的 'Z' 并替换分隔符即可直接当文件名。
        function timestamp() {
            return new Date().toISOString().replace(/[:.]/g, '-').slice(0, -1);
        }

        /// 在浏览器里直接打开产物（output=inline）。
        ///
        /// 与"下载"的区别只在响应头：不发 Content-Disposition 时浏览器把
        /// image/svg+xml 当文档渲染，因此可以直接导航过去，不需要 blob 中转。
        /// 注意产物内嵌的 pan/zoom 脚本依赖的元素/初始化挂钩缺失，所以页内
        /// 无法缩放（见 README）——需要缩放请下载后用桌面工具。
        function openChart(kind) {
            const spec = CHART_SPECS[kind];
            const url = chartUrl(kind, { output: 'inline' });
            log(`🔗 正在生成并在新标签页打开 ${spec.label} 图表...\n    ${url}`);
            window.open(url, '_blank');
        }

        /// 组装快照请求。下拉里的"原始 profile 文本"走 format=profile，
        /// 图形两项走 format=svg + renderer。
        function snapshotUrl(output, delivery) {
            const p = new URLSearchParams({ output: delivery });
            if (output === 'profile') {
                p.set('format', 'profile');
            } else {
                p.set('format', 'svg');
                p.set('renderer', output);
            }
            return `/api/pprof/heap/snapshot?${p.toString()}`;
        }

        function snapshotOutputLabel(output) {
            return output === 'profile' ? '原始 profile 文本'
                 : output === 'flamegraph' ? '火焰图' : '调用图';
        }

        function openSnapshot() {
            const output = document.getElementById('snapshot-output').value;
            const url = snapshotUrl(output, 'inline');
            log(`🔗 正在打开堆快照 (${snapshotOutputLabel(output)})...\n    ${url}`);
            window.open(url, '_blank');
        }

        /// 通用下载：同一份产物既能存盘，也能用 output=inline 在新标签页显示。
        function downloadChart(kind) {
            const spec = CHART_SPECS[kind];
            const btn = document.getElementById(`${kind}-download-btn`);
            const original = btn.textContent;
            btn.disabled = true;
            btn.textContent = '⏳ 生成中...';
            const d = chartDuration(spec);
            log(`📥 正在生成 ${spec.label} 图表${d ? `（窗口 ${d} 秒）` : ''}...`);

            // Promise.resolve() 包一层：同步异常也会走 catch，否则按钮会永久 disabled
            Promise.resolve()
                .then(() => fetch(chartUrl(kind)))
                .then(r => (r.ok ? r.blob() : r.text().then(t => Promise.reject(new Error(errorTextFrom(r, t))))))
                .then(blob => {
                    saveBlob(blob, `${kind}_${timestamp()}.svg`);
                    log(`✅ ${spec.label} 图表已保存 (${(blob.size / 1024).toFixed(1)} KB)`);
                })
                .catch(e => log(`❌ ${spec.label} 图表失败: ${e.message}`))
                .finally(() => {
                    btn.disabled = false;
                    btn.textContent = original;
                });
        }

        /// Heap 快照：产出由 snapshot-output 下拉决定（原始文本 / 火焰图 / 调用图）。
        function downloadSnapshot() {
            const output = document.getElementById('snapshot-output').value;
            const btn = document.getElementById('snapshot-btn');
            const original = btn.textContent;
            btn.disabled = true;
            btn.textContent = '⏳ 生成中...';
            log(`📥 正在获取堆快照 (${snapshotOutputLabel(output)})...`);

            // Promise.resolve() 包一层：同步异常也会走 catch，否则按钮会永久 disabled
            Promise.resolve()
                .then(() => fetch(snapshotUrl(output, 'attachment')))
                .then(r => (r.ok ? r.blob() : r.text().then(t => Promise.reject(new Error(errorTextFrom(r, t))))))
                .then(blob => {
                    const name = output === 'profile' ? 'heap.prof' : `heap_snapshot_${timestamp()}.svg`;
                    saveBlob(blob, name);
                    log(`✅ 堆快照已保存 ${name} (${(blob.size / 1024).toFixed(1)} KB)\n` +
                        (output === 'profile' ? `   分析: go tool pprof ./your_app ${name}` : ''));
                })
                .catch(e => log(`❌ 堆快照失败: ${e.message}`))
                .finally(() => {
                    btn.disabled = false;
                    btn.textContent = original;
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

std::string WebResources::getIndexPage() {
    return std::string(INDEX_PAGE);
}

PROFILER_NAMESPACE_END
