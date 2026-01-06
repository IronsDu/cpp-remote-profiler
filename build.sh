#!/bin/bash

# C++ Remote Profiler 构建脚本

set -e

echo "======================================"
echo "C++ Remote Profiler 构建脚本"
echo "======================================"
echo

# 检查依赖
echo "检查依赖..."

# 检查 gperftools
if ! pkg-config --exists libprofiler; then
    echo "错误: 未找到 libprofiler，请安装 gperftools"
    echo "Ubuntu/Debian: sudo apt-get install libgoogle-perftools-dev"
    exit 1
fi

# 检查 Drogon
if ! pkg-config --exists drogon; then
    echo "错误: 未找到 Drogon 框架"
    echo "请参考 README.md 安装 Drogon"
    exit 1
fi

echo "依赖检查完成"
echo

# 创建构建目录
echo "创建构建目录..."
mkdir -p build
cd build

# 运行 CMake
echo "运行 CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

# 编译
echo "开始编译..."
make -j$(nproc)

echo
echo "======================================"
echo "编译完成！"
echo "======================================"
echo
echo "运行示例程序:"
echo "  cd build"
echo "  ./profiler_example"
echo
echo "然后访问: http://localhost:8080"
