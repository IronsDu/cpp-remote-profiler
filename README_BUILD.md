# C++ Remote Profiler - Protobuf 构建指南

## 当前状态

✅ 已完成：
- vcpkg 依赖配置（包括 protobuf）
- profile.proto 文件创建
- CMakeLists.txt 配置
- profile_parser 框架代码

❌ 待完成：
- 生成 profile.pb.cc 和 profile.pb.h
- 启用 profile_parser 中的 protobuf 解析代码
- 测试完整的解析流程

## 快速构建步骤

### 步骤 1: 生成 Protobuf 代码

```bash
cd /home/dodo/cpp-remote-profiler

# 给脚本添加执行权限
chmod +x generate_proto.sh

# 运行生成脚本
./generate_proto.sh

# 验证文件已生成
ls -la build/profile.pb.*
```

预期输出：
```
build/profile.pb.cc
build/profile.pb.h
```

### 步骤 2: 编译项目

```bash
cd build
make -j$(nproc)
```

预期输出：
```
[100%] Linking CXX executable profiler_example
[100%] Built target profiler_example
```

### 步骤 3: 测试

```bash
# 运行单元测试
./profiler_test

# 启动服务器
./profiler_example
```

## 下一步：启用 Protobuf 解析

生成 `profile.pb.cc` 后，需要编辑 `src/profile_parser.cpp`：

1. 取消注释 `#include "profile.pb.h"`
2. 取消注释 `parseToCollapsed()` 中的 protobuf 解析代码
3. 删除临时的占位符返回语句
4. 重新编译

## 故障排除

### protoc 无法执行

```bash
# 检查权限
ls -la build/vcpkg_installed/x64-linux-release/tools/protobuf/protoc-29.5.0

# 添加执行权限
chmod +x build/vcpkg_installed/x64-linux-release/tools/protobuf/protoc-29.5.0
```

### protobuf_generate_cpp 失败

CMake 找不到 protoc 时，手动指定：

```cmake
# 在 CMakeLists.txt 中找到这一行：
# protobuf_generate_cpp(PROTO_GEN_SRCS PROTO_GEN_HDRS ${PROTO_SRC})

# 替换为：
set(PROTOC_BIN ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-linux-release/tools/protobuf/protoc-29.5.0)
protobuf_generate_cpp(PROTO_GEN_SRCS PROTO_GEN_HDRS ${PROTO_SRC})
```

### 编译错误

如果提示找不到 `profile.pb.h`，确保：
1. protoc 已成功生成文件
2. 文件在 `build/` 目录下
3. CMake 的 include 路径正确

## 架构说明

```
gperftools CPU Profiler
    ↓
cpu.prof (protobuf 格式)
    ↓
ProfileParser (使用 protobuf 解析)
    ↓
collapsed 格式 (func1;func2;func3 count)
    ↓
前端火焰图渲染
```

## 技术细节

### Protobuf 格式
- 定义文件: `third_party/profile/profile.proto`
- 生成文件: `build/profile.pb.{h,cc}`
- 包含的 message: Profile, Sample, Location, Function, Mapping

### 解析流程
1. 读取 cpu.prof 二进制文件
2. 使用 Profile::ParseFromIstream() 解析
3. 提取 string_table, function, location, sample
4. 构建调用栈并聚合
5. 生成 collapsed 格式文本

## 参考资源

- [perftools profile.proto](https://github.com/google/perftools/blob/master/src/profile.proto)
- [pprof profile.proto](https://github.com/google/pprof/blob/main/proto/profile.proto)
- [vcpkg protobuf](https://github.com/microsoft/vcpkg/tree/master/ports/protobuf)
