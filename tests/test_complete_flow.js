// 完整模拟浏览器端的火焰图渲染流程

const http = require('http');

// 1. 获取 CPU 地址数据
async function fetchAddresses() {
    return new Promise((resolve, reject) => {
        http.get('http://localhost:8080/api/cpu/addresses', (res) => {
            let data = '';
            res.on('data', chunk => data += chunk);
            res.on('end', () => resolve(data));
        }).on('error', reject);
    });
}

// 2. 批量符号化
async function batchSymbolize(addresses) {
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
                res.on('data', chunk => data += chunk);
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

    return symbolMap;
}

// 3. 构建火焰图数据（完全按照前端JS的实现）
function buildFlameGraph(samples, symbolMap) {
    console.log('\n=== buildFlameGraph ===');

    const root = { name: 'root', value: 0, children: {}, hasInline: false };
    let totalSamples = 0;

    for (const sample of samples) {
        const stack = sample.stack;
        const value = sample.value;
        totalSamples += value;

        let current = root;
        for (const addr of stack) {
            const symbol = symbolMap.get(addr.toString()) || addr.toString();

            // 检查内联函数
            if (symbol.includes('--')) {
                const inlineFunctions = symbol.split('--');
                const mainFunc = inlineFunctions[0];
                const inlinedFuncs = inlineFunctions.slice(1);

                if (!current.children[mainFunc]) {
                    current.children[mainFunc] = {
                        name: mainFunc,
                        value: 0,
                        hasInline: true,
                        inlineChildren: {},
                        inlineChildren: {},
                        children: {}
                    };
                }

                let inlineParent = current.children[mainFunc];
                for (const inlineFunc of inlinedFuncs) {
                    if (!inlineFunc) continue;
                    const inlineKey = '[inline] ' + inlineFunc;
                    if (!inlineParent.inlineChildren[inlineKey]) {
                        inlineParent.inlineChildren[inlineKey] = {
                            name: inlineFunc,
                            value: 0,
                            isInlined: true,
                            inlineChildren: {},
                        children: {}
                        };
                    }
                    inlineParent = inlineParent.inlineChildren[inlineKey];
                }

                current = inlineParent;
            } else {
                if (!current.children[symbol]) {
                    current.children[symbol] = {
                        name: symbol,
                        value: 0,
                        hasInline: false,
                        inlineChildren: {},
                        inlineChildren: {},
                        children: {}
                    };
                }
                current = current.children[symbol];
            }
        }

        current.value += value;
    }

    console.log('Total samples: ' + totalSamples);
    console.log('Root children: ' + Object.keys(root.children).length);

    // 转换 children 对象为数组
    const convertChildren = (node) => {
        if (node.children) {
            node.children = Object.values(node.children).map(convertChildren);
            if (node.inlineChildren && Object.keys(node.inlineChildren).length > 0) {
                const inlineNodes = Object.values(node.inlineChildren).map(convertChildren);
                node.children = [...inlineNodes, ...node.children];
            }
        }
        return node;
    };

    const tree = convertChildren(root);
    tree.total = totalSamples;

    return tree;
}

// 4. calculateTotal（修复后的版本）
function calculateTotal(node) {
    if (!node || !node.children) return 0;

    let total = 0;
    for (const child of node.children) {
        total += calculateNodeTotal(child);
    }
    return total;
}

function calculateNodeTotal(node) {
    if (node.value && node.value > 0) return node.value;
    if (!node.children || node.children.length === 0) {
        return node.value || 0;
    }
    let sum = 0;
    for (const child of node.children) {
        sum += calculateNodeTotal(child);
    }
    return sum;
}

// 主函数
async function test() {
    try {
        console.log('1. Fetching addresses...');
        const addressData = await fetchAddresses();
        const validLines = addressData.split('\n').filter(l => l && !l.startsWith('#'));
        console.log('   Got ' + validLines.length + ' samples');

        console.log('\n2. Parsing addresses...');
        const lines = addressData.split('\n');
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

            samples.push({ stack, value: count });
        }

        console.log('   Parsed ' + samples.length + ' samples');
        console.log('   Unique addresses: ' + addresses.size);

        console.log('\n3. Symbolizing addresses...');
        const symbolMap = await batchSymbolize(Array.from(addresses));
        console.log('   Symbolized ' + symbolMap.size + ' addresses');

        console.log('\n4. Building flame graph...');
        const flameData = buildFlameGraph(samples, symbolMap);

        console.log('\n5. Testing calculateTotal...');
        const total = calculateTotal(flameData);
        console.log('   calculateTotal(root) = ' + total);
        console.log('   flameData.total = ' + flameData.total);

        if (total === 0) {
            console.log('\n❌ ERROR: calculateTotal returned 0!');
            console.log('This would cause the flame graph to be empty!');
        } else {
            console.log('\n✅ calculateTotal is correct!');
        }

        console.log('\n6. Flame graph structure:');
        printTree(flameData, 0);

    } catch (error) {
        console.error('Error:', error.message);
    }
}

function printTree(node, depth) {
    const indent = '  '.repeat(depth);
    const selfValue = node.value || 0;
    const totalValue = calculateNodeTotal(node);
    console.log(indent + node.name + ': self=' + selfValue + ', total=' + totalValue);

    if (node.children && node.children.length > 0) {
        for (const child of node.children) {
            if (depth < 3) { // 只打印前3层
                printTree(child, depth + 1);
            }
        }
    }
}

test();
