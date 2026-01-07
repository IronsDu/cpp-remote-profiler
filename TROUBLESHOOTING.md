# 手动编译指南

## 当前问题

Bash 工具暂时无法使用，无法获取详细编译错误。

## 诊断步骤

### 1. 检查编译状态

```bash
cd /home/dodo/cpp-remote-profiler/build

# 检查生成的文件
ls -la profile.pb.*
ls -la CMakeFiles/profiler_lib.dir/src/*.o

# 运行诊断脚本
chmod +x ../diagnose_build.sh
../diagnose_build.sh
```

### 2. 如果缺少 profile_parser.cpp.o

```bash
# 清理并重新配置
rm -rf *
cmake .. -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_TARGET_TRIPLET=x64-linux-release

# 重新编译
make -j$(nproc)
```

### 3. 如果有编译错误

将错误信息发送给我，常见问题：

**错误 A: 找不到 profile.pb.h**
```bash
# 确认在 build 目录下
cd /home/dodo/cpp-remote-profiler/build
ls profile.pb.h
```

**错误 B: protobuf 链接错误**
```bash
# 检查 protobuf 库
ls vcpkg_installed/x64-linux-release/lib/*protobuf*
```

**错误 C: undefined reference**
- 检查是否链接了 protobuf::libprotobuf
- 查看 CMakeLists.txt 中的 target_link_libraries

### 4. 验证可执行文件

```bash
# 检查可执行文件
file profiler_example
ldd profiler_example | grep protobuf

# 运行测试
./profiler_test
```

## 预期结果

成功编译后应该有：
- `build/profile.pb.cc`
- `build/profile.pb.h`
- `build/CMakeFiles/profiler_lib.dir/src/profile_parser.cpp.o`
- `build/profiler_example` (可执行)
- `build/profiler_test` (可执行)

## 如果还是失败

请运行以下命令并提供输出：

```bash
cd /home/dodo/cpp-remote-profiler/build
make 2>&1 | tee /tmp/make_error.log
cat /tmp/make_error.log
```

或者运行诊断脚本：
```bash
cd /home/dodo/cpp-remote-profiler
chmod +x diagnose_build.sh
./diagnose_build.sh > /tmp/diagnose.log 2>&1
cat /tmp/diagnose.log
```
