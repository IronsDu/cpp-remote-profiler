#!/bin/bash
set -e

echo "=== C++ Remote Profiler - 详细诊断和构建 ==="
echo ""

PROJECT_DIR="/home/dodo/cpp-remote-profiler"
cd "$PROJECT_DIR"

echo "1. 检查必要文件"
echo "   - profile.proto:"
ls -la third_party/profile/profile.proto
echo "   - profile_parser.cpp:"
ls -la src/profile_parser.cpp
echo "   - CMakeLists.txt:"
ls -la CMakeLists.txt

echo ""
echo "2. 检查 vcpkg"
VCPKG_ROOT="$PROJECT_DIR/vcpkg"
if [ -d "$VCPKG_ROOT" ]; then
    echo "   ✅ vcpkg 目录存在"
else
    echo "   ❌ vcpkg 目录不存在"
    exit 1
fi

echo ""
echo "3. 检查 protoc"
PROTOC_PATHS=(
    "build/vcpkg_installed/x64-linux/tools/protobuf/protoc"
    "build/vcpkg_installed/x64-linux-release/tools/protobuf/protoc-29.5.0"
)

for protoc_path in "${PROTOC_PATHS[@]}"; do
    if [ -f "$protoc_path" ]; then
        echo "   ✅ 找到 protoc: $protoc_path"
        export FOUND_PROTOC="$protoc_path"
        break
    fi
done

if [ -z "$FOUND_PROTOC" ]; then
    echo "   ❌ protoc 不存在"
fi

echo ""
echo "4. 手动生成 protobuf 文件"
mkdir -p build
if [ -n "$FOUND_PROTOC" ]; then
    echo "   运行: $FOUND_PROTOC --proto_path=third_party/profile --cpp_out=build third_party/profile/profile.proto"
    $FOUND_PROTOC --proto_path=third_party/profile --cpp_out=build third_party/profile/profile.proto
    echo "   ✅ 生成完成"
    ls -la build/profile.pb.*
else
    echo "   ❌ 跳过 - protoc 未找到"
fi

echo ""
echo "5. 运行 CMake"
cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_TARGET_TRIPLET=x64-linux-release \
    2>&1 | tee cmake_output.log

if [ ${PIPESTATUS[0]} -eq 0 ]; then
    echo "   ✅ CMake 成功"
else
    echo "   ❌ CMake 失败 - 查看 cmake_output.log"
    echo ""
    echo "=== 最后 30 行错误 ==="
    tail -30 cmake_output.log
    exit 1
fi

echo ""
echo "6. 编译"
make -j$(nproc) 2>&1 | tee make_output.log

if [ ${PIPESTATUS[0]} -eq 0 ]; then
    echo "   ✅ 编译成功"
    echo ""
    echo "=== 生成的文件 ==="
    ls -la profiler_example profiler_test
else
    echo "   ❌ 编译失败 - 查看 make_output.log"
    echo ""
    echo "=== 最后 50 行错误 ==="
    tail -50 make_output.log
    exit 1
fi

echo ""
echo "7. 运行测试"
./profiler_test 2>&1 | tee test_output.log

if [ ${PIPESTATUS[0]} -eq 0 ]; then
    echo "   ✅ 测试通过"
else
    echo "   ⚠️  测试失败 - 查看 test_output.log"
fi

echo ""
echo "=== 构建完成 ==="
echo "可执行文件位置:"
echo "  - build/profiler_example"
echo "  - build/profiler_test"
echo ""
echo "启动服务器:"
echo "  cd build && ./profiler_example"
