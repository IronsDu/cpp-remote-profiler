# C++ Remote Profiler 文档

欢迎来到 C++ Remote Profiler 文档中心！

## 📚 文档目录

### 用户指南
适合所有库用户，从入门到精通。

1. **[快速开始指南](user_guide/01_quick_start.md)**
   - 5 分钟快速集成
   - 最简单的示例
   - 编译和运行
   - 下一步指引

2. **[安装指南](user_guide/05_installation.md)** ⭐ 新增
   - 方法 1: 使用 vcpkg（推荐）
   - 方法 2: 使用 Conan
   - 方法 3: 使用 FetchContent
   - 方法 4: 手动编译
   - 方法 5: 系统包管理器
   - 验证安装和故障排除

3. **[使用 find_package](user_guide/06_using_find_package.md)** ⭐ 新增
   - 基本用法
   - 使用 Web 功能
   - 完整示例
   - 故障排除

4. **[API 参考手册](user_guide/02_api_reference.md)**
   - ProfilerManager API
   - CPU/Heap Profiling API
   - 线程堆栈 API
   - 符号化 API
   - 完整的函数签名和参数说明

5. **[集成示例](user_guide/03_integration_examples.md)**
   - 场景 1: 仅使用核心功能
   - 场景 2: 集成 Web 界面
   - 场景 3: 与现有服务器集成
   - 场景 4: 定时 profiling
   - 场景 5: 条件触发 profiling
   - 场景 6: 多进程 profiling

6. **[故障排除指南](user_guide/04_troubleshooting.md)**
   - 编译问题
   - 链接问题
   - 运行时错误
   - 符号化问题
   - 性能问题
   - 常见问题 FAQ

### 库 API 文档
详细的 API 文档（待完善）。

- [ProfilerManager API](library_api/profiler_manager.md)
- [HTTP Handlers API](library_api/http_handlers.md)
- [Log Sink API](library_api/log_sink.md)

### 开发者指南
贡献者和维护者文档。

- [开发者指南](developer_guide/README.md)

## 🚀 快速导航

### 我是新用户
👉 从 [快速开始指南](user_guide/01_quick_start.md) 开始

### 我想了解 API
👉 查看 [API 参考手册](user_guide/02_api_reference.md)

### 我想看集成示例
👉 阅读 [集成示例](user_guide/03_integration_examples.md)

### 我遇到了问题
👉 参考 [故障排除指南](user_guide/04_troubleshooting.md)

### 我想贡献代码
👉 阅读 [开发者指南](developer_guide/README.md)

## 📖 相关资源

- **主 README**: [项目根目录 README](../README.md)
- **项目规划**: [plan.md](../plan.md)
- **开发规则**: [notes.md](../notes.md)
- **GitHub 仓库**: [https://github.com/your-org/cpp-remote-profiler](https://github.com/your-org/cpp-remote-profiler)

## 🔗 外部参考

- [gperftools 文档](https://github.com/gperftools/gperftools)
- [FlameGraph 工具](https://github.com/brendangregg/FlameGraph)
- [Go pprof 文档](https://github.com/google/pprof)
- [Drogon 框架](https://github.com/drogonframework/drogon)

## 📝 版本说明

**当前版本**: v0.1.0 (开发阶段)

⚠️ **注意**: API 可能随时变化，不建议用于生产环境。

详见 [版本管理](../plan.md#api-稳定性策略)。

## 🤝 贡献

欢迎贡献文档！请参阅 [开发者指南](developer_guide/README.md)。

---

**最后更新**: 2026-02-05
