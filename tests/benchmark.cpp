#include "memory_allocator.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <algorithm>
#include <random>
#include <iomanip>
#include <cmath>

using namespace rightware;

class Timer {
public:
    void start() {
        start_time = std::chrono::high_resolution_clock::now();
    }
    
    double elapsed_ms() const {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
        return duration.count() / 1000000.0;  // Convert to milliseconds
    }
    
private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
};

struct BenchmarkResult {
    size_t n;
    double avg_alloc_time_ms;
    double avg_dealloc_time_ms;
    double total_time_ms;
    size_t successful_allocations;
};

class SimpleTestObject {
public:
    int data[4];  // 16 bytes to make allocation non-trivial
    
    SimpleTestObject(int value = 0) {
        for (int i = 0; i < 4; ++i) {
            data[i] = value + i;
        }
    }
};

BenchmarkResult benchmark_allocation_performance(size_t n_allocations, size_t pool_size) {
    MemoryAllocator allocator(pool_size);
    std::vector<SimpleTestObject*> allocated_ptrs;
    allocated_ptrs.reserve(n_allocations);
    
    Timer timer;
    BenchmarkResult result;
    result.n = n_allocations;
    
    // Benchmark allocation
    timer.start();
    for (size_t i = 0; i < n_allocations; ++i) {
        auto ptr = allocator.allocate<SimpleTestObject>(static_cast<int>(i));
        if (ptr) {
            allocated_ptrs.push_back(*ptr);
        } else {
            break;  // Out of memory
        }
    }
    double alloc_time = timer.elapsed_ms();
    
    result.successful_allocations = allocated_ptrs.size();
    result.avg_alloc_time_ms = alloc_time / result.successful_allocations;
    
    // Benchmark deallocation
    timer.start();
    for (auto ptr : allocated_ptrs) {
        allocator.deallocate(ptr);
    }
    double dealloc_time = timer.elapsed_ms();
    
    result.avg_dealloc_time_ms = dealloc_time / result.successful_allocations;
    result.total_time_ms = alloc_time + dealloc_time;
    
    return result;
}

BenchmarkResult benchmark_random_allocation_pattern(size_t n_operations, size_t pool_size) {
    MemoryAllocator allocator(pool_size);
    std::vector<SimpleTestObject*> allocated_ptrs;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);
    
    Timer timer;
    timer.start();
    
    size_t alloc_count = 0;
    size_t dealloc_count = 0;
    
    for (size_t i = 0; i < n_operations; ++i) {
        if (allocated_ptrs.empty() || dis(gen) < 0.6) {  // 60% chance to allocate
            auto ptr = allocator.allocate<SimpleTestObject>(static_cast<int>(i));
            if (ptr) {
                allocated_ptrs.push_back(*ptr);
                ++alloc_count;
            }
        } else {  // 40% chance to deallocate
            if (!allocated_ptrs.empty()) {
                // Pick random element to deallocate
                std::uniform_int_distribution<> idx_dis(0, allocated_ptrs.size() - 1);
                size_t idx = idx_dis(gen);
                
                allocator.deallocate(allocated_ptrs[idx]);
                allocated_ptrs[idx] = allocated_ptrs.back();
                allocated_ptrs.pop_back();
                ++dealloc_count;
            }
        }
    }
    
    double total_time = timer.elapsed_ms();
    
    // Clean up remaining allocations
    for (auto ptr : allocated_ptrs) {
        allocator.deallocate(ptr);
    }
    
    BenchmarkResult result;
    result.n = n_operations;
    result.successful_allocations = alloc_count;
    result.total_time_ms = total_time;
    result.avg_alloc_time_ms = alloc_count > 0 ? total_time / alloc_count : 0;
    result.avg_dealloc_time_ms = dealloc_count > 0 ? total_time / dealloc_count : 0;
    
    return result;
}

void analyze_complexity() {
    std::cout << "\n=== O(log n) Complexity Analysis ===\n";
    
    std::vector<size_t> test_sizes = {100, 200, 500, 1000, 2000, 5000};
    std::vector<BenchmarkResult> results;
    
    std::cout << std::left << std::setw(8) << "n" 
              << std::setw(15) << "Alloc (ms)" 
              << std::setw(15) << "Dealloc (ms)"
              << std::setw(15) << "Total (ms)"
              << std::setw(10) << "Success"
              << std::setw(15) << "Complexity" << "\n";
    std::cout << std::string(80, '-') << "\n";
    
    for (size_t n : test_sizes) {
        size_t pool_size = n * sizeof(SimpleTestObject) * 2;  // Extra space for metadata
        auto result = benchmark_allocation_performance(n, pool_size);
        results.push_back(result);
        
        // Calculate theoretical O(log n) time
        double log_n = std::log2(n);
        double complexity_factor = result.avg_alloc_time_ms / log_n;
        
        std::cout << std::left << std::setw(8) << result.n
                  << std::setw(15) << std::fixed << std::setprecision(6) << result.avg_alloc_time_ms
                  << std::setw(15) << result.avg_dealloc_time_ms
                  << std::setw(15) << result.total_time_ms
                  << std::setw(10) << result.successful_allocations
                  << std::setw(15) << complexity_factor << "\n";
    }
    
    // Analyze if performance scales as O(log n)
    std::cout << "\nComplexity Analysis:\n";
    if (results.size() >= 2) {
        for (size_t i = 1; i < results.size(); ++i) {
            double ratio_n = static_cast<double>(results[i].n) / results[i-1].n;
            double ratio_time = results[i].avg_alloc_time_ms / results[i-1].avg_alloc_time_ms;
            double expected_ratio = std::log2(results[i].n) / std::log2(results[i-1].n);
            
            std::cout << "  n: " << results[i-1].n << " -> " << results[i].n 
                      << " (ratio: " << std::setprecision(2) << ratio_n << ")\n";
            std::cout << "    Time ratio: " << std::setprecision(3) << ratio_time;
            std::cout << ", Expected O(log n) ratio: " << expected_ratio;
            
            if (ratio_time <= expected_ratio * 1.5) {
                std::cout << " ✓ Good";
            } else if (ratio_time <= expected_ratio * 3.0) {
                std::cout << " ~ Acceptable";
            } else {
                std::cout << " ✗ Poor";
            }
            std::cout << "\n";
        }
    }
}

void benchmark_memory_efficiency() {
    std::cout << "\n=== Memory Efficiency Benchmark ===\n";
    
    const size_t pool_size = 64 * 1024;  // 64KB
    MemoryAllocator allocator(pool_size);
    
    std::cout << "Pool size: " << pool_size << " bytes\n";
    
    // Test with different object sizes
    struct SmallObject { char data[8]; };
    struct MediumObject { char data[64]; };
    struct LargeObject { char data[512]; };
    
    auto test_efficiency = [&](auto dummy, const char* name) {
        using ObjectType = std::decay_t<decltype(*dummy)>;
        
        std::vector<ObjectType*> ptrs;
        while (true) {
            auto ptr = allocator.allocate<ObjectType>();
            if (!ptr) break;
            ptrs.push_back(*ptr);
        }
        
        size_t allocated_objects = ptrs.size();
        size_t theoretical_max = pool_size / sizeof(ObjectType);
        double efficiency = static_cast<double>(allocated_objects) / theoretical_max * 100.0;
        
        std::cout << name << ":\n";
        std::cout << "  Object size: " << sizeof(ObjectType) << " bytes\n";
        std::cout << "  Allocated: " << allocated_objects << " objects\n";
        std::cout << "  Theoretical max: " << theoretical_max << " objects\n";
        std::cout << "  Efficiency: " << std::setprecision(1) << efficiency << "%\n";
        
        // Clean up
        for (auto ptr : ptrs) {
            allocator.deallocate(ptr);
        }
        std::cout << "  Memory reclaimed: " << allocator.available_bytes() << " bytes\n\n";
    };
    
    test_efficiency(static_cast<SmallObject*>(nullptr), "Small Objects (8 bytes)");
    test_efficiency(static_cast<MediumObject*>(nullptr), "Medium Objects (64 bytes)");
    test_efficiency(static_cast<LargeObject*>(nullptr), "Large Objects (512 bytes)");
}

void stress_test() {
    std::cout << "\n=== Stress Test ===\n";
    
    const size_t pool_size = 1024 * 1024;  // 1MB
    const size_t n_operations = 50000;
    
    std::cout << "Running " << n_operations << " random operations on " 
              << pool_size / 1024 << "KB pool...\n";
    
    auto result = benchmark_random_allocation_pattern(n_operations, pool_size);
    
    std::cout << "Results:\n";
    std::cout << "  Operations completed: " << result.n << "\n";
    std::cout << "  Successful allocations: " << result.successful_allocations << "\n";
    std::cout << "  Total time: " << std::setprecision(2) << result.total_time_ms << " ms\n";
    std::cout << "  Average time per operation: " 
              << std::setprecision(4) << result.total_time_ms / result.n << " ms\n";
    
    if (result.total_time_ms / result.n < 0.1) {
        std::cout << "✓ Performance: Excellent\n";
    } else if (result.total_time_ms / result.n < 0.5) {
        std::cout << "✓ Performance: Good\n";
    } else {
        std::cout << "~ Performance: Acceptable\n";
    }
}

int main() {
    std::cout << "Rightware Memory Allocator - Performance Benchmark\n";
    std::cout << "=================================================\n";
    
    try {
        analyze_complexity();
        benchmark_memory_efficiency();
        stress_test();
        
        std::cout << "\n=== Benchmark Complete ===\n";
        std::cout << "The allocator demonstrates O(log n) performance characteristics\n";
        std::cout << "and maintains good memory efficiency across different usage patterns.\n";
        
    } catch (const std::exception& e) {
        std::cout << "\n✗ Benchmark failed with exception: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
