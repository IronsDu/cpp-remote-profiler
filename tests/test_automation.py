#!/usr/bin/env python3
"""
C++ Remote Profiler 自动化测试脚本
"""

import requests
import json
import time
import sys

BASE_URL = 'http://localhost:8080'

def run_tests():
    print("=" * 40)
    print("🔥 C++ Remote Profiler 自动化测试")
    print("=" * 40)

    passed = 0
    failed = 0

    # 测试1: API状态检查
    print("\n🧪 测试: API状态检查")
    try:
        response = requests.get(f"{BASE_URL}/api/status")
        assert response.status_code == 200, "状态码应该是200"
        data = response.json()
        assert data is not None, "应该返回JSON数据"
        assert 'cpu' in data, "应该包含cpu字段"
        assert 'heap' in data, "应该包含heap字段"
        print(f"   CPU运行状态: {data['cpu']['running']}")
        print(f"   Heap运行状态: {data['heap']['running']}")
        print("   ✅ 通过")
        passed += 1
    except AssertionError as e:
        print(f"   ❌ 失败: {str(e)}")
        failed += 1
    except Exception as e:
        print(f"   ❌ 错误: {str(e)}")
        failed += 1

    # 测试2: 启动CPU Profiler
    print("\n🧪 测试: 启动CPU Profiler")
    try:
        response = requests.post(f"{BASE_URL}/api/cpu/start")
        assert response.status_code == 200, "状态码应该是200"
        data = response.json()
        assert data.get('success') == True, "应该成功启动"
        print(f"   输出路径: {data.get('output_path')}")

        status = requests.get(f"{BASE_URL}/api/status").json()
        assert status['cpu']['running'] == True, "CPU应该在运行中"
        print("   ✅ 通过")
        passed += 1
    except AssertionError as e:
        print(f"   ❌ 失败: {str(e)}")
        failed += 1
    except Exception as e:
        print(f"   ❌ 错误: {str(e)}")
        failed += 1

    # 等待数据收集
    print("\n⏳ 等待3秒收集profile数据...")
    time.sleep(3)

    # 测试3: 停止CPU Profiler
    print("\n🧪 测试: 停止CPU Profiler")
    try:
        response = requests.post(f"{BASE_URL}/api/cpu/stop")
        assert response.status_code == 200, "状态码应该是200"
        data = response.json()
        assert data.get('success') == True, "应该成功停止"
        print(f"   运行时长: {data.get('duration_ms')}ms")

        status = requests.get(f"{BASE_URL}/api/status").json()
        assert status['cpu']['running'] == False, "CPU应该已停止"
        print("   ✅ 通过")
        passed += 1
    except AssertionError as e:
        print(f"   ❌ 失败: {str(e)}")
        failed += 1
    except Exception as e:
        print(f"   ❌ 错误: {str(e)}")
        failed += 1

    # 测试4: 获取CPU火焰图JSON
    print("\n🧪 测试: 获取CPU火焰图JSON")
    try:
        response = requests.get(f"{BASE_URL}/api/cpu/flamegraph")
        assert response.status_code == 200, "状态码应该是200"
        data = response.json()
        assert data is not None, "应该返回JSON数据"
        assert data.get('name') == 'root', "根节点应该是root"
        assert isinstance(data.get('children'), list), "children应该是数组"
        print(f"   Total值: {data.get('total')}")
        print(f"   Children数量: {len(data.get('children', []))}")

        if data.get('children') and len(data['children']) > 0:
            first_child = data['children'][0]
            assert 'name' in first_child, "子节点应该有name"
            assert 'value' in first_child, "子节点应该有value"
            print(f"   示例节点: {first_child.get('name')} = {first_child.get('value')}")
        print("   ✅ 通过")
        passed += 1
    except AssertionError as e:
        print(f"   ❌ 失败: {str(e)}")
        failed += 1
    except Exception as e:
        print(f"   ❌ 错误: {str(e)}")
        failed += 1

    # 测试5: 获取SVG火焰图
    print("\n🧪 测试: 获取CPU SVG火焰图")
    try:
        response = requests.get(f"{BASE_URL}/api/cpu/svg")
        assert response.status_code == 200, "状态码应该是200"
        svg = response.text
        assert '<?xml version="1.0"' in svg, "应该包含XML声明"
        assert '<svg' in svg, "应该包含SVG标签"
        assert '</svg>' in svg, "应该包含SVG结束标签"
        print(f"   SVG大小: {len(svg)} 字节")
        print("   ✅ 通过")
        passed += 1
    except AssertionError as e:
        print(f"   ❌ 失败: {str(e)}")
        failed += 1
    except Exception as e:
        print(f"   ❌ 错误: {str(e)}")
        failed += 1

    # 测试6: Heap Profiler
    print("\n🧪 测试: 启动/停止Heap Profiler")
    try:
        response = requests.post(f"{BASE_URL}/api/heap/start")
        assert response.status_code == 200, "启动应该成功"

        time.sleep(2)

        response = requests.post(f"{BASE_URL}/api/heap/stop")
        assert response.status_code == 200, "停止应该成功"
        data = response.json()
        print(f"   运行时长: {data.get('duration_ms')}ms")
        print("   ✅ 通过")
        passed += 1
    except AssertionError as e:
        print(f"   ❌ 失败: {str(e)}")
        failed += 1
    except Exception as e:
        print(f"   ❌ 错误: {str(e)}")
        failed += 1

    # 测试7: 获取Heap火焰图JSON
    print("\n🧪 测试: 获取Heap火焰图JSON")
    try:
        response = requests.get(f"{BASE_URL}/api/heap/flamegraph")
        assert response.status_code == 200, "状态码应该是200"
        data = response.json()
        assert data is not None, "应该返回JSON数据"
        assert data.get('name') == 'root', "根节点应该是root"
        assert isinstance(data.get('children'), list), "children应该是数组"
        print(f"   Total值: {data.get('total')}")
        print(f"   Children数量: {len(data.get('children', []))}")
        print("   ✅ 通过")
        passed += 1
    except AssertionError as e:
        print(f"   ❌ 失败: {str(e)}")
        failed += 1
    except Exception as e:
        print(f"   ❌ 错误: {str(e)}")
        failed += 1

    # 测试8: 数据结构完整性
    print("\n🧪 测试: 数据结构完整性验证")
    try:
        response = requests.get(f"{BASE_URL}/api/cpu/flamegraph")
        data = response.json()

        assert 'name' in data, "应该有name字段"
        assert 'value' in data, "应该有value字段"
        assert 'children' in data, "应该有children字段"

        if data.get('children') and len(data['children']) > 0:
            for i, child in enumerate(data['children']):
                assert 'name' in child, f"子节点{i}应该有name"
                assert 'value' in child, f"子节点{i}应该有value"

        print("   所有字段验证通过")
        print("   ✅ 通过")
        passed += 1
    except AssertionError as e:
        print(f"   ❌ 失败: {str(e)}")
        failed += 1
    except Exception as e:
        print(f"   ❌ 错误: {str(e)}")
        failed += 1

    # 测试9: XML转义检查
    print("\n🧪 测试: SVG XML转义检查")
    try:
        response = requests.get(f"{BASE_URL}/api/cpu/svg")
        svg = response.text

        # 检查是否有未闭合的标签或格式错误
        assert '::' not in svg, "C++作用域操作符应该被转义"
        assert svg.startswith('<?xml'), "应该以XML声明开头"

        amp_count = svg.count('&amp;')
        lt_count = svg.count('&lt;')
        gt_count = svg.count('&gt;')

        print(f"   &amp;: {amp_count}, &lt;: {lt_count}, &gt;: {gt_count}")
        print("   ✅ 通过")
        passed += 1
    except AssertionError as e:
        print(f"   ❌ 失败: {str(e)}")
        failed += 1
    except Exception as e:
        print(f"   ❌ 错误: {str(e)}")
        failed += 1

    total = passed + failed

    # 打印总结
    print("\n" + "=" * 40)
    print("📊 测试总结")
    print("=" * 40)
    print(f"总测试数: {total}")
    print(f"✅ 通过: {passed}")
    print(f"❌ 失败: {failed}")
    if total > 0:
        print(f"成功率: {(passed / total * 100):.1f}%")

    if failed == 0:
        print("\n🎉 所有测试通过！")
    else:
        print("\n⚠️ 存在失败的测试")

    print("\n🌐 浏览器测试页面:")
    print(f"   主页: {BASE_URL}/")
    print(f"   交互式火焰图: {BASE_URL}/flamegraph?type=cpu")
    print(f"   自动化测试: {BASE_URL}/test")

    return failed == 0

if __name__ == '__main__':
    success = run_tests()
    sys.exit(0 if success else 1)
