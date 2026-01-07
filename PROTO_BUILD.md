# Protobuf 构建指南

由于 Bash 工具暂时无法使用，请手动执行以下步骤：

## 步骤 1: 生成 Protobuf 代码

```bash
cd /home/dodo/cpp-remote-profiler

# 方式 1: 使用辅助脚本
chmod +x generate_proto.sh
./generate_proto.sh

# 方式 2: 直接运行 protoc
build/vcpkg_installed/x64-linux-release/tools/protobuf/protoc-29.5.0 \
  --proto_path=third_party/profile \
  --cpp_out=build \
  third_party/profile/profile.proto
```

## 步骤 2: 编译项目

```bash
cd build
make -j$(nproc)
```

## 步骤 3: 验证

```bash
# 检查生成的文件
ls -la build/profile.pb.*

# 运行测试
./profiler_test

# 启动服务器
./profiler_example
```

## 如果 protoc 无法执行

```bash
# 检查文件权限
ls -la build/vcpkg_installed/x64-linux-release/tools/protobuf/protoc-29.5.0

# 添加执行权限
chmod +x build/vcpkg_installed/x64-linux-release/tools/protobuf/protoc-29.5.0

# 或创建符号链接
ln -sf build/vcpkg_installed/x64-linux-release/tools/protobuf/protoc-29.5.0 \
        build/vcpkg_installed/x64-linux-release/tools/protobuf/protoc
```

## 预期输出

成功后会生成：
- `build/profile.pb.h` - 头文件
- `build/profile.pb.cc` - 实现文件
- `profiler_example` - 可执行文件
- `profiler_test` - 测试程序
