#!/bin/bash
# 启动示例服务。注意：ProfilerManager 会在工作目录下生成 pprof / flamegraph.pl，
# 因此必须从可写目录启动（本脚本会 cd 到可执行文件所在目录）。

set -e

# 依次尝试手动构建与 CMake Presets 的输出路径
BIN=""
for candidate in \
    "build/profiler_example" \
    "build/release/profiler_example" \
    "build/relwithdebinfo/profiler_example" \
    "build/debug/profiler_example"
do
    if [ -x "$candidate" ]; then
        BIN="$candidate"
        break
    fi
done

if [ -z "$BIN" ]; then
    echo "错误: 未找到已编译的 profiler_example"
    echo "请先构建，例如:"
    echo "  cmake --preset=release && cmake --build build/release -j\$(nproc)"
    echo "  # 或"
    echo "  ./build.sh"
    exit 1
fi

echo "======================================"
echo " C++ Remote Profiler"
echo "======================================"
echo ""

# 切到可执行文件所在目录：pprof / flamegraph.pl 会生成在这里，且相对路径调用它们
cd "$(dirname "$BIN")"
BIN="./$(basename "$BIN")"

# tcmalloc 在进程初始化时读取该变量，必须在启动前导出（默认 0 = 关闭采样）
export TCMALLOC_SAMPLE_PARAMETER="${TCMALLOC_SAMPLE_PARAMETER:-524288}"

if ! command -v perl >/dev/null 2>&1; then
    echo "警告: 未找到 perl，火焰图/图表接口将无法工作"
    echo "      安装: sudo apt-get install -y perl"
    echo ""
fi

echo "启动服务器..."
echo "  控制面板:      http://localhost:8080"
echo "  CPU 查看器:    http://localhost:8080/show_svg.html"
echo "  Heap 查看器:   http://localhost:8080/show_heap_svg.html"
echo "  Growth 查看器: http://localhost:8080/show_growth_svg.html"
echo ""
echo "  Heap 采样参数 TCMALLOC_SAMPLE_PARAMETER=$TCMALLOC_SAMPLE_PARAMETER"
echo "  按 Ctrl+C 停止服务器"
echo ""

exec "$BIN"
