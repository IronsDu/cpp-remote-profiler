# C++ Remote Profiler 现代化改进路线图

> 基于"最优秀、最现代化的 C++ 库项目"标准的改进计划

---

## 一、构建系统现代化

| 现状 | 建议 | 状态 |
|------|------|------|
| CMake 3.15 | 升级到 **CMake 3.20+**，支持更多现代特性 | ⬜ 待做 |
| 无 CMake Presets | 添加 **`CMakePresets.json`** | ✅ 完成 |
| 无预编译头 | 添加 **PCH (Precompiled Headers)** | ⬜ 待做 |
| 无 Unity Build | 添加 **Unity Build** 选项 | ⬜ 待做 |
| 无 C++20 模块 | 考虑实验性支持 **C++20 Modules** | ⬜ 待做 |

---

## 二、CI/CD 增强

| 现状 | 建议 | 状态 |
|------|------|------|
| 仅 Ubuntu + GCC/Clang | 添加 **macOS** 和 **Windows** 构建矩阵 | ⬜ 待做 |
| TSan 已禁用 | 修复并重新启用 **Thread Sanitizer** | ⬜ 待做 |
| 无代码覆盖率 | 集成 **codecov.io**，在 PR 中显示覆盖率变化 | ✅ 完成 |
| 无 Release 自动化 | 添加 **自动发布流程**（GitHub Releases + 打包）| ⬜ 待做 |
| 无依赖更新自动化 | 添加 **Dependabot** 或 **Renovate** | ⬜ 待做 |
| 无缓存 | 添加 **ccache/sccache** 加速编译 | ⬜ 待做 |
| 无安全扫描 | 添加 **CodeQL** 或 **Snyk** 安全扫描 | ⬜ 待做 |

---

## 三、代码质量提升

| 现状 | 建议 | 状态 |
|------|------|------|
| clang-tidy 仅在 CI | 添加 **cppcheck**、**include-what-you-use (IWYU)** | ⬜ 待做 |
| 无内存泄漏持续检测 | 集成 **Valgrind** 到 CI | ⬜ 待做 |
| 无 Fuzz 测试 | 对关键解析逻辑添加 **Fuzz Testing** | ⬜ 待做 |
| 无基准测试 | 添加 **Google Benchmark** 或 **nanobench** | ⬜ 待做 |
| 无属性测试 | 考虑 **RapidCheck** 属性测试 | ⬜ 待做 |

---

## 四、包管理与分发

| 现状 | 建议 | 状态 |
|------|------|------|
| Conan 1.x 风格 | 升级到 **Conan 2.x** | ⬜ 待做 |
| 无 CPM.cmake | 添加 **CPM.cmake** 支持 | ⬜ 待做 |
| 无 SBOM | 添加 **SBOM** 生成（SPDX/CycloneDX）| ⬜ 待做 |
| 无 Conan Center | 提交到 **Conan Center Index** | ⬜ 待做 |
| 无 Spack 支持 | 添加 **Spack** 包定义 | ⬜ 待做 |

---

## 五、文档与项目治理

| 现状 | 建议 | 状态 |
|------|------|------|
| 有 docs/ 但无 API 文档 | 集成 **Doxygen** + **Sphinx/Breathe** | ⬜ 待做 |
| 无在线文档托管 | 部署到 **GitHub Pages** 或 **Read the Docs** | ⬜ 待做 |
| 无 CHANGELOG.md | 添加 **CHANGELOG.md** | ✅ 完成 |
| 无 CONTRIBUTING.md | 添加贡献指南 | ⬜ 待做 |
| 无 CODE_OF_CONDUCT.md | 添加行为准则 | ⬜ 待做 |
| 无 Issue/PR 模板 | 添加 **GitHub Issue/PR 模板** | ⬜ 待做 |
| 无 SECURITY.md | 添加安全策略 | ⬜ 待做 |
| 无徽章 | 在 README 添加状态徽章 | ⬜ 待做 |

---

## 六、代码现代化

| 现状 | 建议 | 状态 |
|------|------|------|
| C++20 | 考虑部分使用 **C++23** 特性 | ⬜ 待做 |
| 无 Concepts | 使用 **C++20 Concepts** 约束模板参数 | ⬜ 待做 |
| 无 std::format | 使用 **`std::format`** 替代传统格式化 | ⬜ 待做 |
| 无 std::span | 使用 **`std::span`** 替代指针+长度参数 | ⬜ 待做 |
| 无模块化 | 考虑实验性 **C++20 Modules** 支持 | ⬜ 待做 |

---

## 七、API/ABI 管理

| 现状 | 建议 | 状态 |
|------|------|------|
| 有版本化命名空间 ✓ | 这是好的实践 | ✅ 完成 |
| 无符号可见性控制 | 添加 **`-fvisibility=hidden`** + 显式导出宏 | ⬜ 待做 |
| 无 ABI 兼容性检查 | 集成 **libabigail** 检查 ABI 变化 | ⬜ 待做 |
| 无 API 稳定性声明 | 在文档中明确 **API 稳定性承诺** | ⬜ 待做 |

---

## 八、许可证合规

| 现状 | 建议 | 状态 |
|------|------|------|
| 无 SPDX 头 | 添加 **SPDX 许可证标识符**到源文件头部 | ⬜ 待做 |
| 无 REUSE 合规 | 考虑 **REUSE Specification** 合规 | ⬜ 待做 |

---

## 九、测试增强

| 现状 | 建议 | 状态 |
|------|------|------|
| 2个测试文件 | 扩展 **单元测试覆盖**，按模块拆分 | ⬜ 待做 |
| 无集成测试 | 添加 **集成测试** 和 **端到端测试** | ⬜ 待做 |
| 无测试标签 | 使用 GTest 的 **测试标签** | ⬜ 待做 |
| 无测试并行化 | 启用 **ctest 并行执行** | ⬜ 待做 |

---

## 十、其他建议

- [ ] 添加 `SECURITY.md` - 明确漏洞报告流程
- [ ] 添加 `.gitattributes` - 统一换行符处理
- [ ] 添加 `.editorconfig` - 统一编辑器配置
- [x] 添加 `CMakeUserPresets.json` 到 .gitignore
- [ ] vcpkg baseline pinning - 确保可重复构建
- [ ] 添加 Conan lockfile

---

## 📊 优先级

| 优先级 | 改进项 | 状态 |
|--------|--------|------|
| 🔴 高 | 跨平台 CI（macOS/Windows）| ⬜ 待做 |
| 🔴 高 | 代码覆盖率 | ✅ 完成 |
| 🔴 高 | CONTRIBUTING.md | ⬜ 待做 |
| 🔴 高 | Issue/PR 模板 | ⬜ 待做 |
| 🟡 中 | CMakePresets.json | ✅ 完成 |
| 🟡 中 | Doxygen API 文档 | ⬜ 待做 |
| 🟡 中 | CHANGELOG.md | ✅ 完成 |
| 🟡 中 | ccache | ⬜ 待做 |
| 🟡 中 | Conan 2.x | ⬜ 待做 |
| 🟢 低 | C++20 Modules | ⬜ 待做 |
| 🟢 低 | Fuzz Testing | ⬜ 待做 |
| 🟢 低 | SBOM | ⬜ 待做 |
| 🟢 低 | REUSE 合规 | ⬜ 待做 |

---

## 变更日志

| 日期 | 变更 |
|------|------|
| 2026-03-06 | 添加 CMakePresets.json，CI 使用预设构建 |
| 2026-03-04 | 添加代码覆盖率支持（Codecov）|
| 2026-03-04 | 更新 CHANGELOG.md |
| 2026-03-04 | 创建路线图文件 |
