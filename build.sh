#!/bin/bash

echo "======================================"
echo "C++ Remote Profiler 构建脚本"
echo "======================================"
echo ""

VCPKG_ROOT="$(cd "$(dirname "$0")/vcpkg" && pwd)"
export VCPKG_ROOT

echo "VCPKG_ROOT: $VCPKG_ROOT"
echo ""

# 清理并创建build目录
rm -rf build
mkdir build
cd build

echo "配置CMake..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_TARGET_TRIPLET=x64-linux-release \
    --trace-expand \
    2>&1 | tee cmake_debug.log

echo ""
echo "编译项目..."
make -j$(nproc) VERBOSE=1 2>&1 | tee make_debug.log

echo ""
if [ $? -eq 0 ]; then
    echo "✅ 编译成功！"
    echo ""
    echo "运行测试..."
    ./test_cpu_profile 2>&1 | tee test_debug.log
    echo ""
    echo "运行服务："
    echo "  ./start.sh"
    echo ""
    echo "或直接运行："
    echo "  cd build"
    echo "  ./profiler_example"
else
    echo "❌ 编译失败"
    exit 1
fi
