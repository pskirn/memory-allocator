#include "memory_allocator.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <cassert>
#include <string>
#include <random>
#include <algorithm>

using namespace rightware;

// Test classes for verification
class SimpleClass {
public:
    int value;
    static int construct_count;
    static int destruct_count;
    
    SimpleClass(int v = 0) : value(v) { ++construct_count; }
    ~SimpleClass() { ++destruct_count; }
};

int SimpleClass::construct_count = 0;
int SimpleClass::destruct_count = 0;

class AlignedClass {
public:
    alignas(64) double data[8];  // Force 64-byte alignment
    int id;
    
    AlignedClass(int i) : id(i) {
        for (int j = 0; j < 8; ++j) {
            data[j] = i * j;
        }
    }
};

class ComplexClass {
    std::string name;
    std::vector<int> data;
public:
    ComplexClass(const std::string& n, std::vector<int> d) 
        : name(n), data(std::move(d)) {}
    
    const std::string& get_name() const { return name; }
    const std::vector<int>& get_data() const { return data; }
};

// Test utilities
void reset_counters() {
    SimpleClass::construct_count = 0;
    SimpleClass::destruct_count = 0;
}

template<typename Func>
double measure_time_ms(Func&& f) {
    auto start = std::chrono::high_resolution_clock::now();
    f();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    return duration.count() / 1000.0;  // Convert to milliseconds
}

// Test functions
bool test_basic_allocation() {
    std::cout << "Testing basic allocation and deallocation...\n";
    
    MemoryAllocator allocator(1024);
    reset_counters();
    
    // Test 1: Simple allocation
    auto ptr1 = allocator.allocate<SimpleClass>(42);
    assert(ptr1.has_value());
    assert((*ptr1)->value == 42);
    assert(SimpleClass::construct_count == 1);
    assert(allocator.allocated_count() == 1);
    
    // Test 2: Multiple allocations
    auto ptr2 = allocator.allocate<SimpleClass>(100);
    auto ptr3 = allocator.allocate<SimpleClass>(200);
    assert(ptr2.has_value() && ptr3.has_value());
    assert(SimpleClass::construct_count == 3);
    assert(allocator.allocated_count() == 3);
    
    // Test 3: Deallocation
    allocator.deallocate(*ptr1);
    assert(SimpleClass::destruct_count == 1);
    assert(allocator.allocated_count() == 2);
    
    allocator.deallocate(*ptr2);
    allocator.deallocate(*ptr3);
    assert(SimpleClass::destruct_count == 3);
    assert(allocator.allocated_count() == 0);
    
    std::cout << "✓ Basic allocation test passed\n";
    return true;
}

bool test_alignment() {
    std::cout << "Testing memory alignment...\n";
    
    MemoryAllocator allocator(4096);
    
    // Test aligned class allocation
    auto ptr = allocator.allocate<AlignedClass>(123);
    assert(ptr.has_value());
    
    // Check alignment
    uintptr_t address = reinterpret_cast<uintptr_t>(*ptr);
    assert(address % 64 == 0);  // Should be 64-byte aligned
    assert((*ptr)->id == 123);
    
    allocator.deallocate(*ptr);
    
    std::cout << "✓ Alignment test passed\n";
    return true;
}

bool test_state_reversibility() {
    std::cout << "Testing state reversibility (Requirement 5)...\n";
    
    MemoryAllocator allocator(2048);
    
    // Record initial state
    size_t initial_allocated = allocator.allocated_bytes();
    size_t initial_available = allocator.available_bytes();
    
    // Allocate several objects
    std::vector<SimpleClass*> ptrs;
    for (int i = 0; i < 10; ++i) {
        auto ptr = allocator.allocate<SimpleClass>(i);
        assert(ptr.has_value());
        ptrs.push_back(*ptr);
    }
    
    // Record state after allocations
    size_t after_alloc_allocated = allocator.allocated_bytes();
    size_t after_alloc_available = allocator.available_bytes();
    
    // Deallocate in different order (reverse order)
    std::reverse(ptrs.begin(), ptrs.end());
    for (auto ptr : ptrs) {
        allocator.deallocate(ptr);
    }
    
    // Check if we're back to initial state
    assert(allocator.allocated_bytes() == initial_allocated);
    assert(allocator.available_bytes() == initial_available);
    
    // Test the same sequence again - should succeed
    ptrs.clear();
    for (int i = 0; i < 10; ++i) {
        auto ptr = allocator.allocate<SimpleClass>(i);
        assert(ptr.has_value());
        ptrs.push_back(*ptr);
    }
    
    // Should have same allocated bytes as before
    assert(allocator.allocated_bytes() == after_alloc_allocated);
    assert(allocator.available_bytes() == after_alloc_available);
    
    // Clean up
    for (auto ptr : ptrs) {
        allocator.deallocate(ptr);
    }
    
    std::cout << "✓ State reversibility test passed\n";
    return true;
}

bool test_failure_consistency() {
    std::cout << "Testing failure state consistency (Requirement 6)...\n";
    
    // Create small allocator that will run out of memory
    MemoryAllocator allocator(128);
    
    // Fill up the allocator
    std::vector<SimpleClass*> ptrs;
    while (true) {
        auto ptr = allocator.allocate<SimpleClass>(42);
        if (!ptr.has_value()) {
            break;  // Out of memory
        }
        ptrs.push_back(*ptr);
    }
    
    // Record state when allocation fails
    size_t fail_allocated = allocator.allocated_bytes();
    size_t fail_available = allocator.available_bytes();
    
    // Try the same allocation again - should still fail
    auto ptr = allocator.allocate<SimpleClass>(42);
    assert(!ptr.has_value());
    
    // State should be unchanged
    assert(allocator.allocated_bytes() == fail_allocated);
    assert(allocator.available_bytes() == fail_available);
    
    // Clean up
    for (auto p : ptrs) {
        allocator.deallocate(p);
    }
    
    std::cout << "✓ Failure consistency test passed\n";
    return true;
}

bool test_complex_objects() {
    std::cout << "Testing complex object construction...\n";
    
    MemoryAllocator allocator(4096);
    
    // Test object with constructor arguments
    std::vector<int> test_data = {1, 2, 3, 4, 5};
    auto ptr = allocator.allocate<ComplexClass>("TestObject", test_data);
    assert(ptr.has_value());
    assert((*ptr)->get_name() == "TestObject");
    assert((*ptr)->get_data() == test_data);
    
    allocator.deallocate(*ptr);
    
    std::cout << "✓ Complex object test passed\n";
    return true;
}

bool test_performance() {
    std::cout << "Testing performance characteristics...\n";
    
    MemoryAllocator allocator(1024 * 1024);  // 1MB pool
    
    const int n_allocations = 1000;
    std::vector<SimpleClass*> ptrs;
    ptrs.reserve(n_allocations);
    
    // Test allocation performance
    double alloc_time = measure_time_ms([&]() {
        for (int i = 0; i < n_allocations; ++i) {
            auto ptr = allocator.allocate<SimpleClass>(i);
            if (ptr.has_value()) {
                ptrs.push_back(*ptr);
            }
        }
    });
    
    std::cout << "  Allocated " << ptrs.size() << " objects in " << alloc_time << " ms\n";
    std::cout << "  Average allocation time: " << (alloc_time / ptrs.size()) << " ms\n";
    
    // Test deallocation performance
    double dealloc_time = measure_time_ms([&]() {
        for (auto ptr : ptrs) {
            allocator.deallocate(ptr);
        }
    });
    
    std::cout << "  Deallocated " << ptrs.size() << " objects in " << dealloc_time << " ms\n";
    std::cout << "  Average deallocation time: " << (dealloc_time / ptrs.size()) << " ms\n";
    
    std::cout << "✓ Performance test completed\n";
    return true;
}

bool test_memory_efficiency() {
    std::cout << "Testing memory efficiency and fragmentation...\n";
    
    MemoryAllocator allocator(1024);
    
    // Allocate and deallocate in a pattern that could cause fragmentation
    std::vector<SimpleClass*> ptrs;
    
    // Allocate many small objects
    for (int i = 0; i < 20; ++i) {
        auto ptr = allocator.allocate<SimpleClass>(i);
        if (ptr.has_value()) {
            ptrs.push_back(*ptr);
        }
    }
    
    // Deallocate every other object
    for (size_t i = 1; i < ptrs.size(); i += 2) {
        allocator.deallocate(ptrs[i]);
        ptrs[i] = nullptr;
    }
    
    // Try to allocate a larger object - should work due to merging
    auto large_ptr = allocator.allocate<AlignedClass>(999);
    
    // Clean up remaining objects
    for (auto ptr : ptrs) {
        if (ptr) {
            allocator.deallocate(ptr);
        }
    }
    
    if (large_ptr.has_value()) {
        allocator.deallocate(*large_ptr);
    }
    
    std::cout << "✓ Memory efficiency test passed\n";
    return true;
}

int main() {
    std::cout << "=== Memory Allocator Test Suite ===\n\n";
    
    try {
        bool all_passed = true;
        
        all_passed &= test_basic_allocation();
        all_passed &= test_alignment();
        all_passed &= test_state_reversibility();
        all_passed &= test_failure_consistency();
        all_passed &= test_complex_objects();
        all_passed &= test_performance();
        all_passed &= test_memory_efficiency();
        
        std::cout << "\n=== Test Results ===\n";
        if (all_passed) {
            std::cout << "✓ All tests passed!\n";
            return 0;
        } else {
            std::cout << "✗ Some tests failed!\n";
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cout << "✗ Test failed with exception: " << e.what() << "\n";
        return 1;
    }
}
