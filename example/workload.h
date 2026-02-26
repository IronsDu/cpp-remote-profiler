#pragma once

#include <list>
#include <map>
#include <string>
#include <vector>

// 数据处理类 - 用于生成 CPU 火焰图中的数据处理分支
class DataProcessor {
  public:
    void sortData(std::vector<int>& data);
    void reverseData(std::vector<int>& data);
    void shuffleData(std::vector<int>& data);
    void processData(std::vector<int>& data);
};

// Fibonacci 计算类 - 用于生成递归和迭代调用的火焰图
class FibonacciCalculator {
  public:
    int recursive(int n);
    int iterative(int n);
    int memoized(int n, std::map<int, int>& cache);
};

// 矩阵运算类 - 用于生成矩阵操作的火焰图
class MatrixOperations {
  public:
    std::vector<std::vector<int>> createMatrix(int rows, int cols);
    std::vector<std::vector<int>> transposeMatrix(const std::vector<std::vector<int>>& matrix);
    std::vector<std::vector<int>> multiplyMatrices(const std::vector<std::vector<int>>& a,
                                                   const std::vector<std::vector<int>>& b);
};

// Hash 计算类 - 用于生成 hash 计算的火焰图
class HashCalculator {
  public:
    size_t simpleHash(const std::string& str);
    std::map<std::string, size_t> batchHash(const std::vector<std::string>& strings);
    std::vector<size_t> parallelHash(const std::vector<std::string>& strings);
};

// CPU 密集型任务 - 复杂调用链，用于生成详细的 CPU 火焰图
void cpuIntensiveTask();

// 内存密集型任务 - 复杂分配模式，用于生成 Heap 火焰图
void memoryIntensiveTask();
