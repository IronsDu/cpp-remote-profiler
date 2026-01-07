# 当前工作记录和TODO

## 已完成的工作

### 1. Protobuf集成 ✅
- **目的**: 移除pprof依赖，使用纯C++解析profile文件
- **文件修改**:
  - `vcpkg.json`: 添加protobuf依赖（移除不存在的tool feature）
  - `CMakeLists.txt`: 正确配置protobuf_generate()和include路径
  - `third_party/profile/profile.proto`: 创建perftools标准protobuf定义
  - `include/profile_parser.h`: 创建ProfileParser类
  - `src/profile_parser.cpp`: 实现protobuf解析逻辑
  - `src/profiler_manager.cpp`: 启用ProfileParser解析代码

### 2. 构建系统修复 ✅
- 修复protobuf文件找不到的问题（添加include路径）
- 修复链接错误（添加Drogon::Drogon链接库）
- 编译成功，单元测试全部通过

### 3. 架构设计
```
gperftools CPU Profiler (ProfilerStart/ProfilerStop)
         ↓
    cpu.prof (protobuf binary format)
         ↓
ProfileParser (parse perftools::profiles::Profile)
         ↓
  Extract function names from string_table
         ↓
Collapsed format (func1;func2;func3 count)
         ↓
   Frontend flamegraph (d3.js)
```

## 当前问题

### ❌ CPU Profiler无法启动
**错误**: "Failed to start CPU profiler (may already be running)"

**可能原因**:
1. ProfilerState状态管理问题
2. gperftools的ProfilerStart()调用失败
3. 之前的profiler没有正确停止，导致状态残留
4. main.cpp中的worker线程可能干扰了profiler状态

### 调试步骤
1. 检查ProfilerManager::startCPUProfiler()的返回值
2. 验证ProfilerStart()是否真的成功
3. 检查是否有多个ProfilerManager实例被创建
4. 验证mutex锁是否正常工作

## TODO - 接下来要处理

### 高优先级
1. **修复CPU Profiler启动问题**
   - 添加详细日志输出到ProfilerManager
   - 验证ProfilerStart()的返回值
   - 确保profiler状态正确初始化
   - 测试手动启动/停止profiler

2. **验证火焰图数据显示**
   - 确保collapsed格式包含函数名而非地址
   - 测试protobuf符号解析是否正确
   - 验证前端火焰图渲染

3. **测试完整工作流程**
   - 启动服务器
   - 启动CPU profiler
   - 收集数据
   - 停止profiler
   - 获取collapsed数据
   - 在浏览器中查看火焰图

### 中优先级
4. **清理临时调试代码**
   - 移除StackCollector（不再使用runtime stack collection）
   - 清理example中的多余代码
   - 整理日志输出

5. **性能优化**
   - ProfileParser缓存（避免重复解析）
   - 减少protobuf解析开销
   - 优化collapsed格式生成

### 低优先级
6. **文档完善**
   - 更新README说明新架构
   - 添加API使用文档
   - 添加故障排查指南

7. **测试增强**
   - 添加更多边界条件测试
   - 添加性能测试
   - 添加并发测试

## 关键文件清单

### 核心代码
- `src/profiler_manager.cpp` - Profiler管理（状态管理）
- `src/profile_parser.cpp` - Protobuf解析（NEW）
- `include/profile_parser.h` - Parser接口（NEW）
- `example/main.cpp` - HTTP服务器和API

### 配置
- `CMakeLists.txt` - 构建配置
- `vcpkg.json` - 依赖管理
- `third_party/profile/profile.proto` - Protobuf定义（NEW）

### 测试
- `tests/profiler_test.cpp` - 单元测试
- `web/flamegraph.html` - 前端火焰图

## 技术决策记录

### 为什么选择protobuf而不是runtime stack collection？
1. **准确性**: gperftools已经包含了完整的调用栈信息
2. **性能**: 不需要额外的采样线程
3. **兼容性**: 使用perftools标准格式，与pprof兼容
4. **符号解析**: protobuf包含完整的symbol table

### 为什么移除pprof依赖？
1. 用户明确要求不依赖pprof工具
2. 纯C++实现更容易集成和部署
3. 可以完全控制解析逻辑

### 为什么使用collapsed格式？
1. Flamegraph标准格式
2. 简单易解析
3. 前端可以直接渲染

## 已知限制

1. **符号解析限制**: protobuf只包含函数名，不包含文件名和行号
2. **内联函数**: 无法显示内联函数的调用关系
3. **动态库符号**: 可能无法解析动态库中的符号

## 下次继续工作的起点

从 `src/profiler_manager.cpp` 的 `startCPUProfiler()` 函数开始：
1. 添加日志输出，打印ProfilerStart()的返回值
2. 验证状态初始化
3. 检查是否有残留的.prof文件影响启动
