#!/bin/bash

echo "======================================"
echo " C++ Remote Profiler"
echo "======================================"
echo ""

# 检查是否已编译
if [ ! -f "build/profiler_example" ]; then
    echo "错误: 项目未编译"
    echo "请先运行: ./build.sh"
    exit 1
fi

cd build

# 设置 PATH 包含 pprof
export PATH=$PATH:/root/go/bin

echo "启动服务器..."
echo "访问地址: http://localhost:8080"
echo "火焰图查看器: http://localhost:8080/flamegraph"
echo ""
echo "按 Ctrl+C 停止服务器"
echo ""

./profiler_example
