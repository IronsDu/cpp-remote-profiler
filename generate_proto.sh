#!/bin/bash
# 生成 protobuf 代码的辅助脚本

cd "$(dirname "$0")"

echo "=== 生成 profile protobuf 代码 ==="

# 检查 protoc
PROTOC="build/vcpkg_installed/x64-linux-release/tools/protobuf/protoc-29.5.0"

if [ ! -f "$PROTOC" ]; then
    echo "错误: protoc 不存在于 $PROTOC"
    echo "请先运行 ./build.sh 安装依赖"
    exit 1
fi

echo "使用 protoc: $PROTOC"

# 生成 protobuf 代码
$PROTOC --proto_path=third_party/profile --cpp_out=build third_party/profile/profile.proto

if [ $? -eq 0 ]; then
    echo "✅ Protobuf 代码生成成功"
    echo "生成的文件:"
    ls -la build/profile.pb.*
else
    echo "❌ 生成失败"
    exit 1
fi
