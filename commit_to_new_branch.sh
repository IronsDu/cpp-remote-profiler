#!/bin/bash

# Git提交脚本 - 创建新分支并提交protobuf解析功能

set -e

echo "========================================"
echo "  创建新分支并提交代码"
echo "========================================"
echo ""

PROJECT_DIR="/home/dodo/cpp-remote-profiler"
cd "$PROJECT_DIR"

echo "1. 查看当前状态"
echo "----------------"
git status
echo ""

echo "2. 创建新分支 feature/protobuf-parser"
echo "--------------------------------------"
git checkout -b feature/protobuf-parser || git checkout feature/protobuf-parser
echo ""

echo "3. 添加所有修改"
echo "---------------"
git add -A
echo ""

echo "4. 查看待提交的文件"
echo "-------------------"
git status
echo ""

echo "5. 提交更改"
echo "-----------"
git commit -m "feat: 实现基于protobuf的profile解析，移除pprof依赖

主要改动：
- 添加protobuf依赖到vcpkg.json
- 创建profile.proto定义（perftools标准格式）
- 实现ProfileParser类，使用protobuf解析profile文件
- 修复CMakeLists.txt配置（protobuf_generate、Drogon链接、include路径）
- 启用profile_parser.cpp中的protobuf解析代码
- 修复vcpkg.json中protobuf的tool feature错误

测试状态：
- ✅ 编译成功
- ✅ 单元测试全部通过（10/10）

已知问题：
- ❌ CPU Profiler启动失败（返回'may already be running'）
- 需要进一步调试ProfilerManager状态管理

技术细节：
- 使用perftools::profiles::Profile protobuf格式
- 从string_table中提取函数名
- 生成collapsed格式数据供火焰图使用
- 移除了对pprof工具的运行时依赖

相关文件：
- vcpkg.json, CMakeLists.txt
- third_party/profile/profile.proto
- include/profile_parser.h, src/profile_parser.cpp
- WORK_NOTES.md（工作记录）
"
echo ""

echo "6. 查看提交详情"
echo "---------------"
git log -1 --stat
echo ""

echo "7. 显示分支信息"
echo "---------------"
git branch
echo ""

echo "========================================"
echo "  ✅ 提交完成！"
echo "========================================"
echo ""
echo "当前分支: feature/protobuf-parser"
echo ""
echo "后续操作："
echo "  - 推送到远程: git push -u origin feature/protobuf-parser"
echo "  - 创建Pull Request合并到main分支"
echo "  - 查看工作记录: cat WORK_NOTES.md"
echo ""
