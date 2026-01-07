#!/bin/bash

echo "======================================"
echo "C++ Remote Profiler 构建脚本 (vcpkg)"
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
    -DVCPKG_TARGET_TRIPLET=x64-linux-release

echo ""
echo "编译项目..."
make -j$(nproc)

echo ""
if [ $? -eq 0 ]; then
    echo "✅ 编译成功！"
    echo ""
    echo "运行服务："
    echo "  cd build"
    echo "  ./profiler_example"
else
    echo "❌ 编译失败"
    exit 1
fi
