#!/bin/bash

# 测试火焰图完整流程

echo "========== 火焰图完整测试 =========="
echo ""

# 1. 启动CPU Profiler
echo "1. 启动CPU Profiler..."
curl -s -X POST http://localhost:8080/api/cpu/start | jq .
sleep 3

# 2. 检查状态
echo ""
echo "2. 检查CPU Profiler状态..."
curl -s http://localhost:8080/api/status | jq .

# 3. 等待数据收集
echo ""
echo "3. 等待5秒收集profile数据..."
sleep 5

# 4. 停止CPU Profiler
echo ""
echo "4. 停止CPU Profiler..."
curl -s -X POST http://localhost:8080/api/cpu/stop | jq .

# 5. 获取火焰图数据
echo ""
echo "5. 获取火焰图JSON数据..."
curl -s http://localhost:8080/api/cpu/flamegraph > /tmp/flamegraph_test.json
echo "数据已保存到 /tmp/flamegraph_test.json"

# 6. 验证JSON格式
echo ""
echo "6. 验证JSON格式..."
if command -v jq &> /dev/null; then
    jq . /tmp/flamegraph_test.json > /dev/null 2>&1
    if [ $? -eq 0 ]; then
        echo "✅ JSON格式正确"
        echo ""
        echo "数据结构:"
        jq 'keys' /tmp/flamegraph_test.json
        echo ""
        echo "总采样数:"
        jq '.total' /tmp/flamegraph_test.json
        echo ""
        echo "函数数量:"
        jq '.children | length' /tmp/flamegraph_test.json
    else
        echo "❌ JSON格式错误"
        cat /tmp/flamegraph_test.json
    fi
else
    echo "jq未安装，跳过JSON验证"
fi

# 7. 测试Heap Profiler
echo ""
echo "========== Heap Profiler测试 =========="
echo ""
echo "7. 启动Heap Profiler..."
curl -s -X POST http://localhost:8080/api/heap/start | jq .
sleep 3

echo ""
echo "8. 停止Heap Profiler..."
curl -s -X POST http://localhost:8080/api/heap/stop | jq .

echo ""
echo "9. 获取Heap火焰图数据..."
curl -s http://localhost:8080/api/heap/flamegraph > /tmp/heap_flamegraph_test.json
echo "Heap数据已保存到 /tmp/heap_flamegraph_test.json"

if command -v jq &> /dev/null; then
    echo "Heap总采样数:"
    jq '.total' /tmp/heap_flamegraph_test.json
    echo "Heap函数数量:"
    jq '.children | length' /tmp/heap_flamegraph_test.json
fi

# 10. 生成测试报告
echo ""
echo "========== 测试报告 =========="
echo ""
echo "文件位置:"
echo "  - CPU火焰图JSON: /tmp/flamegraph_test.json"
echo "  - Heap火焰图JSON: /tmp/heap_flamegraph_test.json"
echo ""
echo "浏览器测试:"
echo "  主页: http://localhost:8080"
echo "  CPU火焰图: http://localhost:8080/flamegraph?type=cpu"
echo "  Heap火焰图: http://localhost:8080/flamegraph?type=heap"
echo ""
echo "========== 测试完成 =========="
