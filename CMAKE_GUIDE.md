# CMake 配置完成 - 请手动执行

## ✅ 已完成

我已经更新了 `CMakeLists.txt`，使用了正确的 `protobuf_generate()` 配置：

```cmake
# 关键配置
find_package(Protobuf CONFIG REQUIRED)

# 生成 protobuf 并关联到目标
protobuf_generate(
    TARGETS profiler_lib profiler_example profiler_test
    LANGUAGE cpp
    OUT_VAR PROTO_FILES
    PROTOS ${PROTO_SRC}
)

# 添加到源文件
add_library(profiler_lib ${PROFILER_SOURCES} ${PROTO_FILES})
add_executable(profiler_example example/main.cpp ${PROTO_FILES})
```

**这与你提供的参考完全一致！**

## 📋 需要你手动执行

由于 Bash 工具遇到技术问题，请手动执行：

```bash
cd /home/dodo/cpp-remote-profiler

# 1. 清理并重新构建
rm -rf build
mkdir build
cd build

# 2. 配置 CMake
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-linux-release

# 3. 编译
make -j$(nproc)
```

## 预期输出

成功的 CMake 配置应该显示：
```
-- Found Protobuf: ...
-- Generated protobuf files: .../profile.pb.cc .../profile.pb.h
-- Configuring done
-- Generating done
```

成功的编译应该显示：
```
[100%] Building CXX object CMakeFiles/profiler_lib.dir/src/profile_parser.cpp.o
[100%] Linking CXX executable profiler_example
[100%] Built target profiler_example
```

## 如果失败

### 错误 1: 找不到 Protobuf
```
Could not find Protobuf
```
**解决**: 确保 vcpkg 安装了 protobuf（带 tool 特性）

### 错误 2: protobuf_generate 未知
```
Unknown CMake command "protobuf_generate"
```
**解决**: 升级 CMake 到 3.25+ 或使用 `protobuf_generate_cpp`

### 错误 3: 编译错误
将完整的错误信息发给我。

## 成功标志

编译成功后，你会看到：
- `build/profile.pb.cc`
- `build/profile.pb.h`
- `build/profiler_example`
- `build/profiler_test`

## 下一步

编译成功后，告诉我，我会帮你测试火焰图功能！
