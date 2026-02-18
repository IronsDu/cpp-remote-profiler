# Contributing to cpp-remote-profiler

感谢你考虑为 cpp-remote-profiler 做出贡献！

## 目录

- [行为准则](#行为准则)
- [如何贡献](#如何贡献)
- [开发环境搭建](#开发环境搭建)
- [代码风格](#代码风格)
- [提交信息规范](#提交信息规范)
- [Pull Request 流程](#pull-request-流程)
- [测试要求](#测试要求)

## 行为准则

本项目采用贡献者公约作为行为准则。参与此项目即表示你同意遵守其条款。请阅读并遵守行为准则，帮助我们一起维护开放和友好的社区环境。

## 如何贡献

### 报告 Bug

如果你发现了 bug，请通过 [GitHub Issues](https://github.com/IronsDu/cpp-remote-profiler/issues) 提交。提交前请：

1. 搜索现有的 Issues，确认该问题未被报告
2. 使用 Issue 模板，提供以下信息：
   - 操作系统和版本
   - 编译器和版本
   - 复现步骤
   - 期望行为和实际行为
   - 相关日志或错误信息

### 提出新功能

欢迎提出新功能建议！请：

1. 先在 Issues 中讨论你的想法
2. 说明功能的使用场景和价值
3. 等待维护者反馈后再开始实现

### 提交代码

请参考下方的 [Pull Request 流程](#pull-request-流程)。

## 开发环境搭建

### 系统要求

- Linux (Ubuntu 20.04+ 或同等发行版)
- CMake 3.15+
- GCC 10+ 或 Clang 12+
- Git

### 安装依赖

```bash
# 安装系统依赖
sudo apt-get update
sudo apt-get install -y cmake build-essential git pkg-config wget

# 克隆项目（包含 vcpkg 子模块）
git clone https://github.com/IronsDu/cpp-remote-profiler.git
cd cpp-remote-profiler

# 初始化 vcpkg（如果未自动初始化）
git submodule update --init --recursive

# 安装 vcpkg 依赖
cd vcpkg
./bootstrap-vcpkg.sh
./vcpkg install --triplet=x64-linux-release
cd ..
```

### 构建项目

```bash
# 创建构建目录
mkdir build && cd build

# 配置 CMake
cmake .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-linux-release \
  -DBUILD_SHARED_LIBS=ON \
  -DREMOTE_PROFILER_BUILD_EXAMPLES=ON \
  -DREMOTE_PROFILER_BUILD_TESTS=ON

# 编译
make -j$(nproc)

# 运行测试
ctest --output-on-failure
```

### CMake 构建选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `BUILD_SHARED_LIBS` | ON | 构建动态库 |
| `REMOTE_PROFILER_INSTALL` | ON | 生成安装目标 |
| `REMOTE_PROFILER_BUILD_EXAMPLES` | ON | 构建示例程序 |
| `REMOTE_PROFILER_BUILD_TESTS` | ON | 构建测试程序 |

## 代码风格

### 基本原则

- 遵循项目现有的代码风格
- 使用 C++20 标准
- 保持代码简洁、可读

### 命名约定

- **类名**: PascalCase (例如: `ProfilerManager`)
- **函数名**: snake_case (例如: `start_cpu_profile`)
- **变量名**: snake_case (例如: `profile_duration`)
- **常量**: UPPER_SNAKE_CASE (例如: `MAX_PROFILE_DURATION`)
- **成员变量**: 带 `_` 后缀 (例如: `server_running_`)

### 格式化

- 缩进使用 4 个空格
- 每行最多 100 字符
- 大括号放在同一行

### 注释

- 为公共 API 添加注释
- 复杂逻辑需要解释
- 避免冗余注释

## 提交信息规范

本项目使用 [Conventional Commits](https://www.conventionalcommits.org/) 规范：

```
<type>(<scope>): <subject>

<body>

<footer>
```

### Type 类型

| 类型 | 说明 |
|------|------|
| `feat` | 新功能 |
| `fix` | Bug 修复 |
| `docs` | 文档更新 |
| `style` | 代码格式（不影响功能） |
| `refactor` | 代码重构 |
| `test` | 测试相关 |
| `chore` | 构建/工具相关 |
| `perf` | 性能优化 |

### 示例

```
feat: 添加内存分配追踪功能

- 支持追踪 malloc/free 调用
- 添加内存泄漏检测 API

Closes #123
```

```
fix: 修复 CPU profiling 信号处理冲突

当用户程序注册了 SIGUSR1 处理器时，profiling 会失败。
现在会保存并恢复用户的信号处理器。
```

## Pull Request 流程

### 提交前检查清单

- [ ] 代码通过所有测试
- [ ] 新功能有对应的测试用例
- [ ] 代码风格与项目一致
- [ ] 提交信息符合规范
- [ ] 更新相关文档

### 流程步骤

1. **Fork 项目** 并克隆到本地

2. **创建分支**
   ```bash
   git checkout -b feature/your-feature-name
   ```
   分支命名建议：
   - `feat/xxx` - 新功能
   - `fix/xxx` - Bug 修复
   - `docs/xxx` - 文档更新
   - `refactor/xxx` - 代码重构

3. **进行开发**
   - 编写代码
   - 添加测试
   - 更新文档

4. **运行测试**
   ```bash
   cd build
   ctest --output-on-failure
   ```

5. **提交更改**
   ```bash
   git add <files>
   git commit -m "feat: 你的功能描述"
   ```

6. **推送到 Fork**
   ```bash
   git push origin feature/your-feature-name
   ```

7. **创建 Pull Request**
   - 前往 GitHub 创建 PR
   - 填写 PR 模板
   - 等待 CI 通过和代码审查

### CI 检查

每个 PR 都会自动运行以下检查：

- **构建测试**: GCC 和 Clang 编译
- **单元测试**: 所有测试用例
- **代码质量**: AddressSanitizer、UBSan、Clang-Tidy

请确保所有检查通过。如果失败，请在 PR 中修复问题。

## 测试要求

### 单元测试

- 新功能必须添加测试用例
- 测试文件放在 `tests/` 目录
- 使用 Google Test 框架

### 测试命名

```cpp
TEST(TestSuiteName, TestCaseName) {
    // 测试代码
}
```

### 运行特定测试

```bash
cd build
ctest -R <test_name> --output-on-failure
```

## 需要帮助？

如果你有任何问题，可以：

- 在 [Discussions](https://github.com/IronsDu/cpp-remote-profiler/discussions) 中提问
- 在 Issue 中留言
- 发送邮件给维护者

感谢你的贡献！
