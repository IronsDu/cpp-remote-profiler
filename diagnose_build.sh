#!/bin/bash
# 诊断构建问题

echo "=== 检查生成的文件 ==="
cd /home/dodo/cpp-remote-profiler/build

echo "1. Protobuf 文件:"
ls -la profile.pb.* 2>&1

echo ""
echo "2. 目标文件 (.o):"
ls -la CMakeFiles/profiler_lib.dir/src/*.o 2>&1

echo ""
echo "3. 可执行文件:"
ls -la profiler_example profiler_test 2>&1

echo ""
echo "=== 尝试手动编译 profile_parser ==="
echo "编译命令:"
g++ -c -std=c++20 \
    -I../include \
    -I. \
    -Ivcpkg_installed/x64-linux-release/include \
    ../src/profile_parser.cpp \
    -o profile_parser.o \
    2>&1

echo ""
echo "如果编译失败，上面的错误信息会显示具体问题"

echo ""
echo "=== 尝试链接 ==="
echo "检查是否缺少 profile_parser.cpp.o"
if [ ! -f "CMakeFiles/profiler_lib.dir/src/profile_parser.cpp.o" ]; then
    echo "❌ profile_parser.cpp.o 不存在 - 这是问题所在"
    echo ""
    echo "解决方案："
    echo "1. 删除 CMakeCache.txt: rm CMakeCache.txt"
    echo "2. 重新运行 cmake: cmake .."
    echo "3. 重新编译: make"
else
    echo "✅ profile_parser.cpp.o 存在"
fi
