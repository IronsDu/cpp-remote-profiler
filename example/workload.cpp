#include "workload.h"
#include <algorithm>
#include <random>
#include <cstring>

void DataProcessor::sortData(std::vector<int>& data) {
    std::sort(data.begin(), data.end());
}

void DataProcessor::reverseData(std::vector<int>& data) {
    std::reverse(data.begin(), data.end());
}

void DataProcessor::shuffleData(std::vector<int>& data) {
    std::shuffle(data.begin(), data.end(), std::mt19937(std::random_device()()));
}

void DataProcessor::processData(std::vector<int>& data) {
    sortData(data);
    reverseData(data);
    shuffleData(data);
    sortData(data); // 再次排序
}

int FibonacciCalculator::recursive(int n) {
    if (n <= 1) return n;
    return recursive(n - 1) + recursive(n - 2);
}

int FibonacciCalculator::iterative(int n) {
    if (n <= 1) return n;
    int a = 0, b = 1;
    for (int i = 2; i <= n; ++i) {
        int temp = a + b;
        a = b;
        b = temp;
    }
    return b;
}

int FibonacciCalculator::memoized(int n, std::map<int, int>& cache) {
    if (n <= 1) return n;
    if (cache.find(n) != cache.end()) return cache[n];
    cache[n] = memoized(n - 1, cache) + memoized(n - 2, cache);
    return cache[n];
}

std::vector<std::vector<int>> MatrixOperations::createMatrix(int rows, int cols) {
    std::vector<std::vector<int>> matrix;
    for (int i = 0; i < rows; ++i) {
        std::vector<int> row;
        for (int j = 0; j < cols; ++j) {
            row.push_back(rand() % 1000);
        }
        matrix.push_back(row);
    }
    return matrix;
}

std::vector<std::vector<int>> MatrixOperations::transposeMatrix(const std::vector<std::vector<int>>& matrix) {
    std::vector<std::vector<int>> result;
    for (size_t j = 0; j < matrix[0].size(); ++j) {
        std::vector<int> row;
        for (size_t i = 0; i < matrix.size(); ++i) {
            row.push_back(matrix[i][j]);
        }
        result.push_back(row);
    }
    return result;
}

std::vector<std::vector<int>> MatrixOperations::multiplyMatrices(const std::vector<std::vector<int>>& a,
                                                                 const std::vector<std::vector<int>>& b) {
    std::vector<std::vector<int>> result(a.size(), std::vector<int>(b[0].size(), 0));
    for (size_t i = 0; i < a.size(); ++i) {
        for (size_t j = 0; j < b[0].size(); ++j) {
            for (size_t k = 0; k < b.size(); ++k) {
                result[i][j] += a[i][k] * b[k][j];
            }
        }
    }
    return result;
}

size_t HashCalculator::simpleHash(const std::string& str) {
    size_t hash = 0;
    for (char c : str) {
        hash = hash * 31 + c;
    }
    return hash;
}

std::map<std::string, size_t> HashCalculator::batchHash(const std::vector<std::string>& strings) {
    std::map<std::string, size_t> results;
    for (const auto& str : strings) {
        results[str] = simpleHash(str);
    }
    return results;
}

std::vector<size_t> HashCalculator::parallelHash(const std::vector<std::string>& strings) {
    std::vector<size_t> hashes;
    for (const auto& str : strings) {
        hashes.push_back(simpleHash(str));
    }
    return hashes;
}

void cpuIntensiveTask() {
    DataProcessor processor;
    FibonacciCalculator fib;
    MatrixOperations matrixOps;
    HashCalculator hashCalc;

    for (int i = 0; i < 100; ++i) {
        // 1. 数据处理分支
        std::vector<int> data(1000);
        for (auto& val : data) {
            val = rand();
        }

        processor.processData(data);

        // 2. Fibonacci计算分支
        volatile int result1 = fib.recursive(25);
        volatile int result2 = fib.iterative(30);
        (void)result1; (void)result2;

        // 3. 矩阵运算分支
        auto matrix1 = matrixOps.createMatrix(10, 10);
        auto matrix2 = matrixOps.createMatrix(10, 10);
        auto transposed = matrixOps.transposeMatrix(matrix1);
        auto multiplied = matrixOps.multiplyMatrices(matrix1, matrix2);

        // 4. Hash计算分支
        std::vector<std::string> strings = {"hello", "world", "test", "data"};
        auto hashes = hashCalc.parallelHash(strings);
        auto batchHashes = hashCalc.batchHash(strings);

        // 5. 递归计算
        std::map<int, int> cache;
        volatile int result3 = fib.memoized(20, cache);
        (void)result3;

        // 防止编译器优化
        volatile int sink = data[0] + result1 + result2 + result3 + hashes[0];
        (void)sink;
        (void)transposed;
        (void)multiplied;
        (void)batchHashes;
    }
}

void memoryIntensiveTask() {
    // 使用各种内存分配模式
    std::vector<std::vector<int>> matrixData;
    std::vector<std::string> stringData;
    std::map<int, std::vector<int>> mapData;
    std::vector<std::list<int>> listData;

    for (int i = 0; i < 50; ++i) {
        // 1. 大数组分配
        std::vector<int> largeArray(10000);
        for (auto& val : largeArray) {
            val = rand();
        }

        // 2. 矩阵分配
        for (int j = 0; j < 20; ++j) {
            std::vector<int> row(1000);
            for (auto& val : row) {
                val = rand();
            }
            matrixData.push_back(row);
        }

        // 3. 字符串分配
        std::vector<std::string> strings;
        for (int j = 0; j < 100; ++j) {
            strings.push_back("Test string data " + std::to_string(j));
        }
        stringData.insert(stringData.end(), strings.begin(), strings.end());

        // 4. Map分配
        for (int j = 0; j < 10; ++j) {
            std::vector<int> values(100);
            for (auto& val : values) {
                val = j * 10 + rand() % 100;
            }
            mapData[j] = values;
        }

        // 5. 动态分配
        int* dynamicArray = new int[1000];
        for (int k = 0; k < 1000; ++k) {
            dynamicArray[k] = rand();
        }
        delete[] dynamicArray;

        // 6. 小对象频繁分配
        std::list<int> smallList;
        for (int k = 0; k < 200; ++k) {
            smallList.push_back(rand());
        }
        listData.push_back(smallList);
    }

    // 模拟内存泄漏（故意不释放）
    for (int i = 0; i < 10; ++i) {
        auto* leak = new int[5000];
        for (int j = 0; j < 5000; ++j) {
            leak[j] = i;
        }
        (void)leak; // 故意泄漏
    }
}
