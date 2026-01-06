#!/bin/bash

# 一键启动脚本 - 检查依赖、安装、构建、运行

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

PROJECT_DIR="/home/dodo/cpp-remote-profiler"
BUILD_DIR="$PROJECT_DIR/build"

echo -e "${BLUE}======================================"
echo "C++ Remote Profiler 一键启动"
echo -e "======================================${NC}"
echo

# 检查依赖
echo -e "${YELLOW}[1/4] 检查依赖...${NC}"

MISSING_DEPS=()

# 检查 gperftools
if ! pkg-config --exists libprofiler 2>/dev/null; then
    MISSING_DEPS+=("libgoogle-perftools-dev")
fi

# 检查 jsoncpp
if ! pkg-config --exists jsoncpp 2>/dev/null; then
    MISSING_DEPS+=("libjsoncpp-dev")
fi

# 检查 OpenSSL
if ! pkg-config --exists openssl 2>/dev/null; then
    MISSING_DEPS+=("libssl-dev")
fi

# 检查 zlib
if ! pkg-config --exists zlib 2>/dev/null; then
    MISSING_DEPS+=("zlib1g-dev")
fi

# 检查 Drogon
if ! pkg-config --exists drogon 2>/dev/null; then
    MISSING_DEPS+=("Drogon")
fi

if [ ${#MISSING_DEPS[@]} -gt 0 ]; then
    echo -e "${RED}发现缺失的依赖:${NC}"
    for dep in "${MISSING_DEPS[@]}"; do
        echo "  - $dep"
    done
    echo

    # 尝试安装系统包
    if [ -n "$MISSING_DEPS" ]; then
        echo -e "${YELLOW}尝试安装系统依赖...${NC}"
        echo "请输入 sudo 密码以继续安装:"
        echo

        # 安装系统包
        sudo apt-get update -y
        sudo apt-get install -y \
            libgoogle-perftools-dev \
            libssl-dev \
            zlib1g-dev \
            libjsoncpp-dev \
            cmake \
            build-essential \
            git \
            pkg-config

        echo -e "${GREEN}✓ 系统依赖安装完成${NC}"
    fi

    # 检查是否还需要安装 Drogon
    if ! pkg-config --exists drogon 2>/dev/null; then
        echo
        echo -e "${YELLOW}需要安装 Drogon 框架...${NC}"

        DROGON_BUILD_DIR="/tmp/drogon_build_$$"
        git clone --depth 1 https://github.com/drogonframework/drogon.git "$DROGON_BUILD_DIR"
        cd "$DROGON_BUILD_DIR"
        git submodule update --init
        mkdir build && cd build
        cmake .. -DCMAKE_BUILD_TYPE=Release
        make -j$(nproc)
        sudo make install
        sudo ldconfig
        cd /
        rm -rf "$DROGON_BUILD_DIR"

        echo -e "${GREEN}✓ Drogon 安装完成${NC}"
    fi
else
    echo -e "${GREEN}✓ 所有依赖已满足${NC}"
fi

echo

# 构建项目
echo -e "${YELLOW}[2/4] 构建项目...${NC}"
cd "$PROJECT_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

echo -e "${GREEN}✓ 项目构建完成${NC}"
echo

# 检查构建结果
echo -e "${YELLOW}[3/4] 检查构建结果...${NC}"

if [ ! -f "$BUILD_DIR/profiler_example" ]; then
    echo -e "${RED}错误: 构建失败，找不到可执行文件${NC}"
    exit 1
fi

echo -e "${GREEN}✓ 可执行文件已生成${NC}"
echo

# 启动服务
echo -e "${YELLOW}[4/4] 启动服务...${NC}"
echo -e "${GREEN}======================================"
echo "服务启动中..."
echo "访问地址: http://localhost:8080"
echo -e "======================================${NC}"
echo

cd "$BUILD_DIR"
exec ./profiler_example
