// 测试 buildFlameGraph 函数

// 模拟数据
const samples = [
    {
        stack: ['0x79937d929c6c', '0x79937d89caa4', '0x79937dcecdb4', '0x63ad3ba1aeea', '0x63ad3ba10a15', '0x79937d84a0ad'],
        value: 1
    }
];

const symbolMap = new Map([
    ['0x79937d929c6c', 'clone3'],
    ['0x79937d89caa4', 'start_thread'],
    ['0x79937dcecdb4', 'execute_native_thread_routine--operator()--~unique_ptr'],
    ['0x63ad3ba1aeea', '0x63ad3ba1aeea'],
    ['0x63ad3ba10a15', '0x63ad3ba10a15'],
    ['0x79937d84a0ad', 'rand']
]);

console.log('Testing buildFlameGraph...\n');

function buildFlameGraph(samples, symbolMap) {
    const root = { name: 'root', value: 0, children: {}, hasInline: false };
    let totalSamples = 0;

    for (const sample of samples) {
        const stack = sample.stack;
        const value = sample.value;
        totalSamples += value;

        let current = root;
        for (const addr of stack) {
            const symbol = symbolMap.get(addr.toString()) || addr.toString();

            console.log(`Processing: ${addr} -> ${symbol}`);

            // 检查内联函数
            if (symbol.includes('--')) {
                console.log(`  -> Has inline functions`);
                const inlineFunctions = symbol.split('--');
                const mainFunc = inlineFunctions[0];
                const inlinedFuncs = inlineFunctions.slice(1);

                console.log(`  -> Main: ${mainFunc}`);
                console.log(`  -> Inlined: ${inlinedFuncs.join(', ')}`);

                if (!current.children[mainFunc]) {
                    console.log(`  -> Creating child for ${mainFunc}`);
                    current.children[mainFunc] = {
                        name: mainFunc,
                        value: 0,
                        hasInline: true,
                        inlineChildren: {},
                        children: {}
                    };
                }

                let inlineParent = current.children[mainFunc];
                console.log(`  -> inlineParent before loop:`, inlineParent.name);

                for (const inlineFunc of inlinedFuncs) {
                    if (!inlineFunc) continue;

                    const inlineKey = `[inline] ${inlineFunc}`;
                    console.log(`    -> Processing inline: ${inlineFunc} (key: ${inlineKey})`);
                    console.log(`    -> inlineParent.inlineChildren exists?`, !!inlineParent.inlineChildren);

                    if (!inlineParent.inlineChildren[inlineKey]) {
                        console.log(`      -> Creating inline child`);
                        inlineParent.inlineChildren[inlineKey] = {
                            name: inlineFunc,
                            value: 0,
                            isInlined: true,
                            inlineChildren: {},  // ✅ 添加 inlineChildren 属性
                            children: {}
                        };
                    }
                    inlineParent = inlineParent.inlineChildren[inlineKey];
                    console.log(`      -> Moved inlineParent to:`, inlineParent.name);
                }

                current = inlineParent;
                console.log(`  -> current set to inlineParent:`, current.name);
            } else {
                if (!current.children[symbol]) {
                    console.log(`  -> Creating normal child for ${symbol}`);
                    current.children[symbol] = {
                        name: symbol,
                        value: 0,
                        hasInline: false,
                        inlineChildren: {},
                        children: {}
                    };
                }
                current = current.children[symbol];
                console.log(`  -> current set to: ${symbol}`);
            }
        }

        current.value += value;
        console.log(`-> Added value ${value} to current node\n`);
    }

    // 转换 children 对象为数组
    console.log('\nConverting children to array...\n');
    const convertChildren = (node, depth = 0) => {
        const indent = '  '.repeat(depth);
        console.log(`${indent}convertChildren(${node.name})`);

        if (node.children) {
            console.log(`${indent}  Has children: ${Object.keys(node.children).length}`);
            node.children = Object.values(node.children).map(child => convertChildren(child, depth + 1));

            if (node.inlineChildren && Object.keys(node.inlineChildren).length > 0) {
                console.log(`${indent}  Has inlineChildren: ${Object.keys(node.inlineChildren).length}`);
                const inlineNodes = Object.values(node.inlineChildren).map(child => convertChildren(child, depth + 1));
                node.children = [...inlineNodes, ...node.children];
            }
        }
        return node;
    };

    const tree = convertChildren(root);
    tree.total = totalSamples;

    return tree;
}

try {
    const flameData = buildFlameGraph(samples, symbolMap);
    console.log('\n✅ buildFlameGraph succeeded!');
    console.log('Root:', flameData.name);
    console.log('Total:', flameData.total);
    console.log('Children:', flameData.children ? flameData.children.length : 0);
} catch (error) {
    console.error('\n❌ buildFlameGraph failed:', error.message);
    console.error(error.stack);
}
