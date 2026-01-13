# TODO List

## 当前待办事项 (2026-01-13)

### 高优先级

1. **优化 Heap 采样配置**
   - 调整 `TCMALLOC_SAMPLE_PARAMETER` 环境变量的默认值
   - 添加更详细的 Heap Profiling 使用文档
   - 验证不同采样频率下的效果

2. **改进采样提示信息**
   - 前端：当采样时间太短（< 5秒）时显示警告
   - 后端：已改进错误提示（✅ 已完成）
   - 用户体验优化

3. **测试覆盖率提升**
   - 为新增的 Web 服务器模块添加单元测试
   - 测试 Web 资源嵌入功能
   - 集成测试覆盖重构后的代码

### 中优先级

4. **文档完善**
   - 更新 README.md，说明如何作为库使用
   - 添加库的使用示例代码
   - 更新 API 文档

5. **性能优化**
   - 优化 pprof 调用性能（可以考虑缓存）
   - 减少重复的符号化请求
   - 大型 profile 文件的流式处理

6. **错误处理增强**
   - 添加更详细的错误日志
   - 改进错误恢复机制
   - 添加降级策略

### 低优先级

7. **功能增强**
   - 支持更多 pprof 输出格式（PDF、PNG等）
   - 添加 profile 数据对比功能
   - 支持历史 profile 数据查看
   - 添加性能指标统计面板

8. **开发体验**
   - 改进开发环境配置
   - 添加更多调试工具
   - 优化构建速度

## 已完成的功能 (2026-01-13)

### ✅ 核心功能
- CPU 和 Heap Profiling 集成
- pprof 自动生成 SVG 火焰图
- Web 界面显示 SVG
- 符号化支持（backward-cpp）
- 标准的 pprof HTTP 接口

### ✅ Web 功能
- 主页面下载按钮（CPU 和 Heap 原始 SVG）
- 下载状态提示和按钮禁用
- 独立状态管理（CPU 和 Heap 可同时下载）
- 错误提示优化

### ✅ 代码重构 (2026-01-13)
- 代码组织优化，支持作为库使用
- Web 资源嵌入到 C++ 代码
- 删除 web/ 目录依赖
- 简化主入口文件（从 772 行到 45 行）
- 分离工作负载代码到 workload.cpp

### ✅ API 接口
- `/api/status` - 获取 profiler 状态
- `/api/cpu/analyze` - CPU 一键分析
- `/api/heap/analyze` - Heap 一键分析
- `/api/cpu/svg_raw` - CPU 原始 SVG 下载
- `/api/heap/svg_raw` - Heap 原始 SVG 下载
- `/pprof/symbol` - 符号解析（支持内联函数）
- `/pprof/profile` - 标准 pprof CPU profile
- `/pprof/heap` - 标准 pprof heap sample

## 参考文档

- **plan.md** - 详细的项目规划和设计文档
- **README.md** - 项目介绍和快速开始
- **notes.md** - 开发规则和流程
