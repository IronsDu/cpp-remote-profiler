# C++ Remote Profiler 文档

本项目文档分三层：根目录 `README.md` 是入口与速查，`docs/user_guide/` 是面向使用者的详解，`plan.md` 是面向维护者的设计与决策记录。

## 用户指南

按推荐阅读顺序排列：

1. **[快速开始](user_guide/01_quick_start.md)** — 5 分钟跑通第一个 profiling
2. **[安装指南](user_guide/05_installation.md)** — 源码安装 / FetchContent / add_subdirectory 三种引入方式，含 CMake 选项
3. **[使用 find_package](user_guide/06_using_find_package.md)** — 已安装场景下的 CMake 包用法与排错
4. **[API 参考手册](user_guide/02_api_reference.md)** — `ProfilerManager` / `ProfilerHttpHandlers` / `LogSink` 完整签名
5. **[集成示例](user_guide/03_integration_examples.md)** — 5 类集成场景，含框架无关接入
6. **[故障排除](user_guide/04_troubleshooting.md)** — 编译、链接、符号化、性能、多线程问题

## 设计与维护

- **[设计文档 (plan.md)](../plan.md)** — 架构设计、技术决策记录、已知限制
- **[贡献指南](../CONTRIBUTING.md)** — 开发环境、代码规范、PR 流程
- **[版本历史](../CHANGELOG.md)** — 已发布版本的变更记录
- **[路线图](../ROADMAP.md)** — 待办与改进方向

## 外部参考

- [gperftools](https://github.com/gperftools/gperftools) — CPU / Heap profiling 底层实现
- [Go pprof](https://github.com/google/pprof) — profile 格式与可视化工具
- [FlameGraph](https://github.com/brendangregg/FlameGraph) — 火焰图渲染
- [Drogon](https://github.com/drogonframework/drogon) — 可选 Web 框架

## 版本

**当前版本**: v0.1.0（开发阶段，API 可能变化，不建议用于生产环境）

API 兼容性承诺见 [设计文档中的版本策略](../plan.md#api-稳定性策略)。
