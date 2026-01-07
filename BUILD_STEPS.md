# 完整构建指南 - protobuf 方案

## 问题说明

自动化构建脚本遇到 Bash 工具技术问题。请手动执行以下步骤。

## 第一步：准备环境

```bash
cd /home/dodo/cpp-remote-profiler

# 确认必要文件存在
ls third_party/profile/profile.proto      # ✅ 应该存在
ls src/profile_parser.cpp                 # ✅ 应该存在
ls include/profile_parser.h               # ✅ 应该存在
```

## 第二步：生成 Protobuf 代码

```bash
# 方式 1：使用辅助脚本（如果 protoc 在标准位置）
./generate_proto.sh

# 方式 2：手动执行
build/vcpkg_installed/x64-linux-release/tools/protobuf/protoc-29.5.0 \
  --proto_path=third_party/profile \
  --cpp_out=build \
  third_party/profile/profile.proto

# 验证生成成功
ls -la build/profile.pb.cc build/profile.pb.h
```

预期输出：
```
-rw-r--r-- 1 dodo dodo ... build/profile.pb.cc
-rw-r--r-- 1 dodo dodo ... build/profile.pb.h
```

## 第三步：配置 CMake

```bash
cd build

cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-linux-release
```

预期输出：
```
-- Found Protobuf: /home/dodo/cpp-remote-profiler/build/vcpkg_installed/...
-- Configuring done
-- Generating done
```

## 第四步：编译

```bash
# 在 build 目录中
make -j$(nproc)
```

预期输出：
```
[100%] Linking CXX executable profiler_example
[100%] Built target profiler_example
```

## 第五步：测试

```bash
# 运行单元测试
./profiler_test

# 应该看到：
# [==========] Running 10 tests from 1 test suite.
# [ PASSED ] 10 tests.
```

## 第六步：运行服务器

```bash
./profiler_example

# 应该看到：
# C++ Remote Profiler Example
# ============================
# Starting HTTP server on 0.0.0.0:8080
```

## 第七步：测试火焰图

在另一个终端：

```bash
# 启动 CPU profiler
curl -X POST http://localhost:8080/api/cpu/start

# 等待 5 秒
sleep 5

# 停止 profiler
curl -X POST http://localhost:8080/api/cpu/stop

# 获取 collapsed 数据（应该显示函数名）
curl http://localhost:8080/api/cpu/collapsed | head -20
```

预期输出应该包含函数名而不是十六进制地址：
```
# collapsed stack traces
# Total samples: XXX
main;cpuIntensiveTask;std::sort 50
main;fibonacci_recursive 30
```

## 故障排除

### 问题 1：CMake 找不到 protobuf

**错误信息：**
```
Could NOT find Protobuf
```

**解决方案：**
```bash
# 检查 vcpkg 是否安装了 protobuf
ls vcpkg_installed/x64-linux-release/include/google/protobuf/

# 如果没有，需要先运行 vcpkg 安装
# 通常 build.sh 会自动处理
```

### 问题 2：编译错误 - 找不到 profile.pb.h

**错误信息：**
```
fatal error: profile.pb.h: No such file or directory
```

**解决方案：**
```bash
# 确认 protoc 已生成文件
ls build/profile.pb.*

# 如果不存在，重新运行第二步
```

### 问题 3：链接错误 - undefined reference

**错误信息：**
```
undefined reference to `google::protobuf::...'
```

**解决方案：**
```bash
# 确认 CMakeLists.txt 包含：
# protobuf::libprotobuf
# 在 target_link_libraries 中

grep "protobuf::libprotobuf" ../CMakeLists.txt
```

### 问题 4：运行时错误

**错误信息：**
```
Error while loading profile: Failed to parse profile file
```

**解决方案：**
```bash
# 检查 cpu.prof 是否是有效的 protobuf 格式
file cpu.prof

# 应该显示：
# cpu.prof: data
```

## 验证安装成功

完成所有步骤后，你应该能够：

1. ✅ 运行 `./profiler_example` 启动服务器
2. ✅ 访问 http://localhost:8080 查看主页
3. ✅ 启动/停止 CPU profiler
4. ✅ 获取 collapsed 格式的数据
5. ✅ 查看火焰图显示函数名而不是地址

## 架构说明

```
┌─────────────────┐
│ gperftools CPU  │
│   Profiler      │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  cpu.prof       │ (protobuf 二进制格式)
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ ProfileParser   │
│ (protobuf 解析) │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ collapsed 格式  │ (func1;func2;func3 count)
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  前端火焰图     │
│  显示函数名     │
└─────────────────┘
```

## 需要帮助？

如果遇到问题，请提供：

1. 具体的错误信息
2. 运行的命令
3. 输出的日志

相关文件：
- `full_build.sh` - 完整构建脚本（带详细输出）
- `diagnose_build.sh` - 诊断脚本
- `TROUBLESHOOTING.md` - 故障排除指南
- `README_BUILD.md` - Protobuf 构建指南
