// 测试前端 JS 解析和符号化逻辑

const http = require('http');

// 模拟前端的 processCPUProfileText 函数
async function processCPUProfileText() {
    console.log('1. Fetching CPU profile addresses...');

    return new Promise((resolve, reject) => {
        http.get('http://localhost:8080/api/cpu/addresses', (res) => {
            let data = '';
            res.on('data', (chunk) => data += chunk);
            res.on('end', () => {
                console.log('2. Received address data:');
                console.log(data.substring(0, 500) + '...\n');

                // 解析地址栈文本格式: "count @ addr1 addr2 addr3"
                const lines = data.split('\n');
                const samples = [];
                const addresses = new Set();

                for (const line of lines) {
                    if (!line || line.startsWith('#')) continue;

                    const parts = line.split('@');
                    if (parts.length < 2) continue;

                    const count = parseInt(parts[0].trim());
                    if (isNaN(count) || count <= 0) continue;

                    const addrStr = parts[1].trim();
                    if (!addrStr) continue;

                    const stack = addrStr.split(/\s+/).reverse();
                    for (const addr of stack) {
                        if (addr) addresses.add(addr);
                    }

                    samples.push({
                        stack: stack,
                        value: count
                    });
                }

                console.log(`3. Parsed ${samples.length} samples`);
                console.log(`   Unique addresses: ${addresses.size}`);

                if (samples.length === 0) {
                    reject(new Error('No samples found'));
                    return;
                }

                // 显示前3个样本
                console.log('\n4. First 3 samples:');
                samples.slice(0, 3).forEach((sample, i) => {
                    console.log(`   Sample ${i + 1}: count=${sample.value}, stack_depth=${sample.stack.length}`);
                    console.log(`     Stack: ${sample.stack.slice(0, 3).join(' <- ')}...`);
                });

                resolve({ samples, addresses: Array.from(addresses) });
            });
        }).on('error', reject);
    });
}

// 模拟前端的 batchSymbolize 函数
async function batchSymbolize(addresses) {
    console.log(`\n5. Batch symbolizing ${addresses.length} addresses...`);

    const batchSize = 100;
    const symbolMap = new Map();

    for (let i = 0; i < addresses.length; i += batchSize) {
        const batch = addresses.slice(i, i + batchSize);
        const addressStr = batch.join('\n');

        const result = await new Promise((resolve, reject) => {
            const postData = Buffer.from(addressStr, 'utf8');

            const options = {
                hostname: 'localhost',
                port: 8080,
                path: '/pprof/symbol',
                method: 'POST',
                headers: {
                    'Content-Type': 'text/plain',
                    'Content-Length': postData.length
                }
            };

            const req = http.request(options, (res) => {
                let data = '';
                res.on('data', (chunk) => data += chunk);
                res.on('end', () => resolve(data));
            });
            req.on('error', reject);
            req.write(postData);
            req.end();
        });

        const lines = result.split('\n');
        for (const line of lines) {
            if (!line) continue;
            const parts = line.split(' ');
            if (parts.length >= 2) {
                const addr = parts[0];
                const symbol = parts.slice(1).join(' ');
                symbolMap.set(addr, symbol);
            }
        }
    }

    console.log(`   Symbolized ${symbolMap.size} addresses`);

    // 显示前5个符号化结果
    console.log('\n6. First 5 symbolized results:');
    let count = 0;
    for (const [addr, symbol] of symbolMap) {
        if (count++ >= 5) break;
        console.log(`   ${addr} -> ${symbol}`);
    }

    return symbolMap;
}

// 模拟前端的 buildFlameGraph 函数
function buildFlameGraph(samples, symbolMap) {
    console.log('\n7. Building flame graph data...');

    const root = { name: 'root', value: 0, children: {} };
    let totalSamples = 0;

    for (const sample of samples) {
        const stack = sample.stack;
        const value = sample.value;
        totalSamples += value;

        let current = root;
        for (const addr of stack) {
            const symbol = symbolMap.get(addr) || addr;

            if (!current.children[symbol]) {
                current.children[symbol] = {
                    name: symbol,
                    value: 0,
                    children: {}
                };
            }
            current = current.children[symbol];
        }

        current.value += value;
    }

    console.log(`   Total samples: ${totalSamples}`);
    console.log(`   Unique functions: ${Object.keys(root.children).length}`);

    // 显示顶层函数及其子函数
    console.log('\n8. Flame graph structure:');
    for (const [name, child] of Object.entries(root.children)) {
        const selfValue = child.value;
        const totalValue = calculateNodeTotal(child);
        console.log(`   ${name}: self=${selfValue}, total=${totalValue}, children=${Object.keys(child.children).length}`);

        // 显示子函数
        for (const [childName, grandChild] of Object.entries(child.children)) {
            const gcSelf = grandChild.value;
            const gcTotal = calculateNodeTotal(grandChild);
            if (gcTotal > 0) {
                console.log(`     └─ ${childName}: self=${gcSelf}, total=${gcTotal}`);
            }
        }
    }

    return { root, totalSamples };
}

// 计算节点总 value（包括子节点）
function calculateNodeTotal(node) {
    if (!node.children || Object.keys(node.children).length === 0) {
        return node.value || 0;
    }
    let sum = node.value || 0;
    for (const child of Object.values(node.children)) {
        sum += calculateNodeTotal(child);
    }
    return sum;
}

// 主测试流程
async function testFrontendLogic() {
    try {
        // 1. 获取并解析地址数据
        const { samples, addresses } = await processCPUProfileText();

        // 2. 批量符号化
        const symbolMap = await batchSymbolize(addresses);

        // 3. 构建火焰图数据
        const { root, totalSamples } = buildFlameGraph(samples, symbolMap);

        console.log('\n✅ Frontend logic test PASSED!');
        console.log(`\nSummary:`);
        console.log(`  Samples: ${samples.length}`);
        console.log(`  Total value: ${totalSamples}`);
        console.log(`  Unique symbols: ${symbolMap.size}`);

    } catch (error) {
        console.error('\n❌ Frontend logic test FAILED:', error.message);
        process.exit(1);
    }
}

// 运行测试
testFrontendLogic();
