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

# 7. 获取SVG火焰图
echo ""
echo "7. 获取SVG火焰图..."
curl -s http://localhost:8080/api/cpu/svg > /tmp/flamegraph_test.svg
echo "SVG已保存到 /tmp/flamegraph_test.svg"

# 8. 验证SVG格式
echo ""
echo "8. 验证SVG格式..."
if grep -q "<?xml version=\"1.0\"" /tmp/flamegraph_test.svg && \
   grep -q "<svg" /tmp/flamegraph_test.svg && \
   grep -q "</svg>" /tmp/flamegraph_test.svg; then
    echo "✅ SVG格式正确"
    echo ""
    echo "SVG大小: $(wc -c < /tmp/flamegraph_test.svg) 字节"
    echo "SVG预览（前20行）:"
    head -20 /tmp/flamegraph_test.svg
else
    echo "❌ SVG格式错误"
    head -30 /tmp/flamegraph_test.svg
fi

# 9. 检查XML转义
echo ""
echo "9. 检查XML转义..."
if grep -q "&lt;" /tmp/flamegraph_test.svg || \
   grep -q "&gt;" /tmp/flamegraph_test.svg || \
   grep -q "&amp;" /tmp/flamegraph_test.svg; then
    echo "✅ 发现XML转义字符"
    echo "转义示例:"
    grep -o '&[a-z]*;' /tmp/flamegraph_test.svg | sort -u
else
    echo "ℹ️  未发现XML转义字符（可能没有需要转义的特殊字符）"
fi

# 10. 获取文本格式
echo ""
echo "10. 获取文本格式profile..."
curl -s http://localhost:8080/api/cpu/text > /tmp/flamegraph_test.txt
echo "文本数据已保存到 /tmp/flamegraph_test.txt"
echo ""
echo "文本内容预览:"
head -30 /tmp/flamegraph_test.txt

# 11. 测试Heap Profiler
echo ""
echo "========== Heap Profiler测试 =========="
echo ""
echo "11. 启动Heap Profiler..."
curl -s -X POST http://localhost:8080/api/heap/start | jq .
sleep 3

echo ""
echo "12. 停止Heap Profiler..."
curl -s -X POST http://localhost:8080/api/heap/stop | jq .

echo ""
echo "13. 获取Heap火焰图数据..."
curl -s http://localhost:8080/api/heap/flamegraph > /tmp/heap_flamegraph_test.json
echo "Heap数据已保存到 /tmp/heap_flamegraph_test.json"

if command -v jq &> /dev/null; then
    echo "Heap总采样数:"
    jq '.total' /tmp/heap_flamegraph_test.json
    echo "Heap函数数量:"
    jq '.children | length' /tmp/heap_flamegraph_test.json
fi

# 14. 生成测试报告
echo ""
echo "========== 测试报告 =========="
echo ""
echo "文件位置:"
echo "  - CPU火焰图JSON: /tmp/flamegraph_test.json"
echo "  - CPU火焰图SVG:  /tmp/flamegraph_test.svg"
echo "  - CPU文本数据:    /tmp/flamegraph_test.txt"
echo "  - Heap火焰图JSON: /tmp/heap_flamegraph_test.json"
echo ""
echo "浏览器测试:"
echo "  主页: http://localhost:8080"
echo "  CPU火焰图: http://localhost:8080/flamegraph?type=cpu"
echo "  Heap火焰图: http://localhost:8080/flamegraph?type=heap"
echo ""
echo "========== 测试完成 =========="
