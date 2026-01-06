#!/bin/bash

# C++ Remote Profiler 依赖安装脚本

set -e

echo "======================================"
echo "C++ Remote Profiler 依赖安装"
echo "======================================"
echo

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 检查是否有 sudo 权限
if [ "$EUID" -ne 0 ]; then
    echo -e "${YELLOW}注意: 需要使用 sudo 安装系统依赖${NC}"
    sudo -v || { echo -e "${RED}错误: 需要 sudo 权限${NC}"; exit 1; }
fi

# 1. 安装系统依赖
echo -e "${GREEN}[1/4] 安装系统依赖...${NC}"
sudo apt-get update
sudo apt-get install -y \
    libgoogle-perftools-dev \
    libssl-dev \
    zlib1g-dev \
    libjsoncpp-dev \
    cmake \
    build-essential \
    git \
    pkg-config \
    wget \
    ca-certificates

echo -e "${GREEN}✓ 系统依赖安装完成${NC}"
echo

# 2. 安装 Drogon 框架
echo -e "${GREEN}[2/4] 安装 Drogon 框架...${NC}"

DROGON_DIR="/tmp/drogon_install"
if [ -d "$DROGON_DIR" ]; then
    rm -rf "$DROGON_DIR"
fi

git clone --depth 1 --branch v1.9.0 https://github.com/drogonframework/drogon.git "$DROGON_DIR"
cd "$DROGON_DIR"

# 更新子模块
git submodule update --init

# 构建和安装
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DBUILD_TESTING=OFF \
         -DBUILD_EXAMPLES=OFF

make -j$(nproc)
sudo make install

# 清理
cd /
rm -rf "$DROGON_DIR"

echo -e "${GREEN}✓ Drogon 框架安装完成${NC}"
echo

# 3. 更新动态链接库缓存
echo -e "${GREEN}[3/4] 更新动态链接库缓存...${NC}"
sudo ldconfig

echo -e "${GREEN}✓ 动态链接库缓存更新完成${NC}"
echo

# 4. 验证安装
echo -e "${GREEN}[4/4] 验证安装...${NC}"

# 检查 gperftools
if pkg-config --exists libprofiler; then
    echo -e "${GREEN}✓ gperftools 已安装${NC}"
    pkg-config --modversion libprofiler
else
    echo -e "${RED}✗ gperftools 未找到${NC}"
    exit 1
fi

# 检查 Drogon
if pkg-config --exists drogon; then
    echo -e "${GREEN}✓ Drogon 已安装${NC}"
    pkg-config --modversion drogon
else
    echo -e "${YELLOW}⚠ Drogon pkg-config 未找到（可能已安装但未在 pkg-config 中注册）${NC}"
fi

# 检查 trantor (Drogon 依赖)
if pkg-config --exists trantor; then
    echo -e "${GREEN}✓ Trantor 已安装${NC}"
else
    echo -e "${YELLOW}⚠ Trantor pkg-config 未找到${NC}"
fi

echo
echo "======================================"
echo -e "${GREEN}依赖安装完成！${NC}"
echo "======================================"
echo
echo "接下来运行:"
echo "  cd /home/dodo/cpp-remote-profiler"
echo "  ./build.sh"
echo
