#!/usr/bin/env node

const http = require('http');

const BASE_URL = 'http://localhost:8080';

function httpRequest(path, method = 'GET', data = null) {
    return new Promise((resolve, reject) => {
        const url = new URL(path, BASE_URL);
        const options = {
            hostname: url.hostname,
            port: url.port,
            path: url.pathname + url.search,
            method: method,
            headers: {
                'Content-Type': 'application/json',
            }
        };

        if (data) {
            const jsonData = JSON.stringify(data);
            options.headers['Content-Length'] = Buffer.byteLength(jsonData);
        }

        const req = http.request(options, (res) => {
            let body = '';
            res.on('data', (chunk) => body += chunk);
            res.on('end', () => {
                try {
                    const json = JSON.parse(body);
                    resolve({ status: res.statusCode, data: json, raw: body });
                } catch {
                    resolve({ status: res.statusCode, data: null, raw: body });
                }
            });
        });

        req.on('error', reject);

        if (data) {
            req.write(JSON.stringify(data));
        }

        req.end();
    });
}

async function test(name, testFn) {
    try {
        console.log(`\n🧪 测试: ${name}`);
        await testFn();
        console.log(`   ✅ 通过`);
        return true;
    } catch (error) {
        console.log(`   ❌ 失败: ${error.message}`);
        return false;
    }
}

async function assert(condition, message) {
    if (!condition) {
        throw new Error(message || 'Assertion failed');
    }
}

async function runTests() {
    console.log('========================================');
    console.log('🔥 C++ Remote Profiler 自动化测试');
    console.log('========================================');

    const results = {
        passed: 0,
        failed: 0,
        tests: []
    };

    // 测试1: API状态检查
    results.tests.push(await test('API状态检查', async () => {
        const response = await httpRequest('/api/status');
        assert(response.status === 200, '状态码应该是200');
        assert(response.data !== null, '应该返回JSON数据');
        assert(response.data.cpu !== undefined, '应该包含cpu字段');
        assert(response.data.heap !== undefined, '应该包含heap字段');
        console.log(`   CPU运行状态: ${response.data.cpu.running}`);
        console.log(`   Heap运行状态: ${response.data.heap.running}`);
    }));

    // 测试2: 启动CPU Profiler
    results.tests.push(await test('启动CPU Profiler', async () => {
        const response = await httpRequest('/api/cpu/start', 'POST');
        assert(response.status === 200, '状态码应该是200');
        assert(response.data.success === true, '应该成功启动');
        console.log(`   输出路径: ${response.data.output_path}`);

        // 验证状态
        const status = await httpRequest('/api/status');
        assert(status.data.cpu.running === true, 'CPU应该在运行中');
    }));

    // 等待数据收集
    console.log('\n⏳ 等待3秒收集profile数据...');
    await new Promise(resolve => setTimeout(resolve, 3000));

    // 测试3: 停止CPU Profiler
    results.tests.push(await test('停止CPU Profiler', async () => {
        const response = await httpRequest('/api/cpu/stop', 'POST');
        assert(response.status === 200, '状态码应该是200');
        assert(response.data.success === true, '应该成功停止');
        console.log(`   运行时长: ${response.data.duration_ms}ms`);

        // 验证状态
        const status = await httpRequest('/api/status');
        assert(status.data.cpu.running === false, 'CPU应该已停止');
    }));

    // 测试4: 获取CPU火焰图JSON数据
    results.tests.push(await test('获取CPU火焰图JSON', async () => {
        const response = await httpRequest('/api/cpu/flamegraph');
        assert(response.status === 200, '状态码应该是200');
        assert(response.data !== null, '应该返回JSON数据');
        assert(response.data.name === 'root', '根节点应该是root');
        assert(Array.isArray(response.data.children), 'children应该是数组');
        console.log(`   Total值: ${response.data.total}`);
        console.log(`   Children数量: ${response.data.children.length}`);

        if (response.data.children.length > 0) {
            const firstChild = response.data.children[0];
            assert(firstChild.name !== undefined, '子节点应该有name');
            assert(firstChild.value !== undefined, '子节点应该有value');
            console.log(`   示例节点: ${firstChild.name} = ${firstChild.value}`);
        }
    }));

    // 测试5: 获取SVG火焰图
    results.tests.push(await test('获取CPU SVG火焰图', async () => {
        const response = await httpRequest('/api/cpu/svg');
        assert(response.status === 200, '状态码应该是200');
        assert(response.raw.includes('<?xml version="1.0"'), '应该包含XML声明');
        assert(response.raw.includes('<svg'), '应该包含SVG标签');
        assert(response.raw.includes('</svg>'), '应该包含SVG结束标签');
        console.log(`   SVG大小: ${response.raw.length} 字节`);
    }));

    // 测试6: 测试Heap Profiler
    results.tests.push(await test('启动/停止Heap Profiler', async () => {
        let response = await httpRequest('/api/heap/start', 'POST');
        assert(response.status === 200, '启动应该成功');

        await new Promise(resolve => setTimeout(resolve, 2000));

        response = await httpRequest('/api/heap/stop', 'POST');
        assert(response.status === 200, '停止应该成功');
        console.log(`   运行时长: ${response.data.duration_ms}ms`);
    }));

    // 测试7: 获取Heap火焰图JSON
    results.tests.push(await test('获取Heap火焰图JSON', async () => {
        const response = await httpRequest('/api/heap/flamegraph');
        assert(response.status === 200, '状态码应该是200');
        assert(response.data !== null, '应该返回JSON数据');
        assert(response.data.name === 'root', '根节点应该是root');
        assert(Array.isArray(response.data.children), 'children应该是数组');
        console.log(`   Total值: ${response.data.total}`);
        console.log(`   Children数量: ${response.data.children.length}`);
    }));

    // 测试8: 验证数据结构完整性
    results.tests.push(await test('数据结构完整性验证', async () => {
        const cpuResponse = await httpRequest('/api/cpu/flamegraph');
        const data = cpuResponse.data;

        assert(data.name !== undefined, '应该有name字段');
        assert(data.value !== undefined, '应该有value字段');
        assert(data.children !== undefined, '应该有children字段');

        if (data.children.length > 0) {
            data.children.forEach((child, i) => {
                assert(child.name !== undefined, `子节点${i}应该有name`);
                assert(child.value !== undefined, `子节点${i}应该有value`);
            });
        }
        console.log('   所有字段验证通过');
    }));

    // 测试9: SVG XML转义检查
    results.tests.push(await test('SVG XML转义检查', async () => {
        const response = await httpRequest('/api/cpu/svg');
        const svg = response.raw;

        // 检查是否有未闭合的标签或格式错误
        assert(!svg.includes('::'), 'C++作用域操作符应该被转义');
        assert(svg.indexOf('<?xml') === 0, '应该以XML声明开头');

        // 统计转义字符
        const ampCount = (svg.match(/&amp;/g) || []).length;
        const ltCount = (svg.match(/&lt;/g) || []).length;
        const gtCount = (svg.match(/&gt;/g) || []).length;

        console.log(`   &amp;: ${ampCount}, &lt;: ${ltCount}, &gt;: ${gtCount}`);
    }));

    // 统计结果
    results.passed = results.tests.filter(t => t).length;
    results.failed = results.tests.filter(t => !t).length;

    // 打印总结
    console.log('\n========================================');
    console.log('📊 测试总结');
    console.log('========================================');
    console.log(`总测试数: ${results.tests.length}`);
    console.log(`✅ 通过: ${results.passed}`);
    console.log(`❌ 失败: ${results.failed}`);
    console.log(`成功率: ${((results.passed / results.tests.length) * 100).toFixed(1)}%`);

    if (results.failed === 0) {
        console.log('\n🎉 所有测试通过！');
    } else {
        console.log('\n⚠️ 存在失败的测试');
    }

    console.log('\n🌐 浏览器测试页面:');
    console.log(`   主页: ${BASE_URL}/`);
    console.log(`   交互式火焰图: ${BASE_URL}/flamegraph?type=cpu`);
    console.log(`   自动化测试: ${BASE_URL}/test`);

    process.exit(results.failed === 0 ? 0 : 1);
}

// 运行测试
runTests().catch(error => {
    console.error('❌ 测试运行失败:', error);
    process.exit(1);
});
