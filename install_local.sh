#!/bin/bash

# C++ Remote Profiler 本地安装脚本（不需要 sudo）
# 将依赖安装到 ~/.local

set -e

echo "======================================"
echo "C++ Remote Profiler 本地依赖安装"
echo "======================================"
echo

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# 设置安装目录
INSTALL_DIR="$HOME/.local"
BUILD_DIR="/tmp/cpp_profiler_build"

mkdir -p "$INSTALL_DIR"
mkdir -p "$BUILD_DIR"

echo "安装目录: $INSTALL_DIR"
echo

# 1. 检查是否有必要的工具
echo -e "${GREEN}[1/5] 检查构建工具...${NC}"

for cmd in git cmake wget g++ make; do
    if ! command -v $cmd &> /dev/null; then
        echo -e "${RED}错误: 未找到 $cmd，请先安装: $ sudo apt-get install $cmd${NC}"
        exit 1
    fi
    echo "  ✓ $cmd"
done

echo -e "${GREEN}✓ 构建工具检查完成${NC}"
echo

# 2. 安装 gperftools（如果没有）
echo -e "${GREEN}[2/5] 检查 gperftools...${NC}"

if pkg-config --exists libprofiler 2>/dev/null; then
    echo -e "${GREEN}✓ gperftools 已安装${NC}"
    pkg-config --modversion libprofiler
else
    echo "正在安装 gperftools 到 $INSTALL_DIR..."

    cd "$BUILD_DIR"
    wget -q https://github.com/gperftools/gperftools/releases/download/gperftools-2.10/gperftools-2.10.tar.gz
    tar xzf gperftools-2.10.tar.gz
    cd gperftools-2.10

    ./configure --prefix="$INSTALL_DIR" --disable-debugalloc
    make -j$(nproc)
    make install

    echo -e "${GREEN}✓ gperftools 安装完成${NC}"
fi

echo

# 3. 安装 uuid（Drogon 依赖）
echo -e "${GREEN}[3/5] 检查 uuid 库...${NC}"

if pkg-config --exists uuid 2>/dev/null; then
    echo -e "${GREEN}✓ uuid 库已安装${NC}"
else
    echo "正在安装 uuid 到 $INSTALL_DIR..."

    cd "$BUILD_DIR"
    wget -q https://sourceforge.net/projects/libuuid/files/libuuid-1.0.3.tar.gz/download -O libuuid-1.0.3.tar.gz
    tar xzf libuuid-1.0.3.tar.gz
    cd libuuid-1.0.3

    ./configure --prefix="$INSTALL_DIR"
    make -j$(nproc)
    make install

    echo -e "${GREEN}✓ uuid 库安装完成${NC}"
fi

echo

# 4. 安装 Brotli（Drogon 依赖）
echo -e "${GREEN}[4/5] 检查 Brotli 库...${NC}"

if pkg-config --exists libbrotlidec 2>/dev/null; then
    echo -e "${GREEN}✓ Brotli 库已安装${NC}"
else
    echo "正在安装 Brotli 到 $INSTALL_DIR..."

    cd "$BUILD_DIR"
    git clone --depth 1 https://github.com/google/brotli.git
    cd brotli
    mkdir build && cd build
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" ..
    make -j$(nproc)
    make install

    echo -e "${GREEN}✓ Brotli 库安装完成${NC}"
fi

echo

# 5. 安装 c-ares（Drogon 依赖）
echo -e "${GREEN}[5/5] 检查 c-ares 库...${NC}"

if pkg-config --exists libcares 2>/dev/null; then
    echo -e "${GREEN}✓ c-ares 库已安装${NC}"
else
    echo "正在安装 c-ares 到 $INSTALL_DIR..."

    cd "$BUILD_DIR"
    wget -q https://c-ares.haxx.se/download/c-ares-1.19.0.tar.gz -O c-ares.tar.gz
    tar xzf c-ares.tar.gz
    cd c-ares-1.19.0

    ./configure --prefix="$INSTALL_DIR"
    make -j$(nproc)
    make install

    echo -e "${GREEN}✓ c-ares 库安装完成${NC}"
fi

echo

# 6. 更新 PKG_CONFIG_PATH
echo "======================================"
echo -e "${GREEN}本地依赖安装完成！${NC}"
echo "======================================"
echo
echo "请设置以下环境变量："
echo
echo "  export PKG_CONFIG_PATH=\"$INSTALL_DIR/lib/pkgconfig:\$PKG_CONFIG_PATH\""
echo "  export LD_LIBRARY_PATH=\"$INSTALL_DIR/lib:\$LD_LIBRARY_PATH\""
echo "  export PATH=\"$INSTALL_DIR/bin:\$PATH\""
echo
echo "或者将以下内容添加到 ~/.bashrc："
echo
cat << EOF
export PKG_CONFIG_PATH="$INSTALL_DIR/lib/pkgconfig:\$PKG_CONFIG_PATH"
export LD_LIBRARY_PATH="$INSTALL_DIR/lib:\$LD_LIBRARY_PATH"
export PATH="$INSTALL_DIR/bin:\$PATH"
EOF
echo

# 清理
read -p "是否删除临时构建目录? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    rm -rf "$BUILD_DIR"
    echo "临时文件已清理"
fi

echo
echo "接下来运行:"
echo "  source ~/.bashrc  # 如果添加了环境变量"
echo "  cd /home/dodo/cpp-remote-profiler"
echo "  ./build.sh"
