#include "memory_allocator.h"
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>

using namespace rightware;

// Example class for demonstration
class Vehicle {
private:
    std::string model_;
    int year_;
    double price_;

public:
    Vehicle(const std::string& model, int year, double price)
        : model_(model), year_(year), price_(price) {
        std::cout << "  Constructing Vehicle: " << model_ << " (" << year_ << ")\n";
    }
    
    ~Vehicle() {
        std::cout << "  Destroying Vehicle: " << model_ << " (" << year_ << ")\n";
    }
    
    void display() const {
        std::cout << "  " << year_ << " " << model_ << " - $" << std::fixed 
                  << std::setprecision(2) << price_ << "\n";
    }
    
    const std::string& model() const { return model_; }
    int year() const { return year_; }
    double price() const { return price_; }
};

void demonstrate_basic_usage() {
    std::cout << "\n=== Basic Usage Example ===\n";
    
    // Create allocator with 4KB pool
    MemoryAllocator allocator(4096);
    
    std::cout << "Initial state:\n";
    std::cout << "  Total size: " << allocator.total_size() << " bytes\n";
    std::cout << "  Available: " << allocator.available_bytes() << " bytes\n";
    
    // Allocate primitive types
    std::cout << "\nAllocating primitive types:\n";
    auto int_ptr = allocator.allocate<int>(42);
    auto double_ptr = allocator.allocate<double>(3.14159);
    
    if (int_ptr && double_ptr) {
        std::cout << "  int: " << **int_ptr << "\n";
        std::cout << "  double: " << **double_ptr << "\n";
        
        std::cout << "  Allocated count: " << allocator.allocated_count() << "\n";
        std::cout << "  Allocated bytes: " << allocator.allocated_bytes() << " bytes\n";
    }
    
    // Allocate complex objects
    std::cout << "\nAllocating complex objects:\n";
    auto vehicle1 = allocator.allocate<Vehicle>("Tesla Model S", 2023, 89990.0);
    auto vehicle2 = allocator.allocate<Vehicle>("BMW i4", 2023, 67995.0);
    
    if (vehicle1 && vehicle2) {
        std::cout << "\nCreated vehicles:\n";
        (*vehicle1)->display();
        (*vehicle2)->display();
    }
    
    std::cout << "\nAfter all allocations:\n";
    std::cout << "  Allocated count: " << allocator.allocated_count() << "\n";
    std::cout << "  Allocated bytes: " << allocator.allocated_bytes() << " bytes\n";
    std::cout << "  Available bytes: " << allocator.available_bytes() << " bytes\n";
    
    // Deallocate everything
    std::cout << "\nDeallocating objects:\n";
    if (int_ptr) allocator.deallocate(*int_ptr);
    if (double_ptr) allocator.deallocate(*double_ptr);
    if (vehicle1) allocator.deallocate(*vehicle1);
    if (vehicle2) allocator.deallocate(*vehicle2);
    
    std::cout << "\nAfter deallocation:\n";
    std::cout << "  Allocated count: " << allocator.allocated_count() << "\n";
    std::cout << "  Available bytes: " << allocator.available_bytes() << " bytes\n";
}

void demonstrate_state_reversibility() {
    std::cout << "\n=== State Reversibility Example ===\n";
    
    MemoryAllocator allocator(2048);
    
    // Record initial state
    auto initial_available = allocator.available_bytes();
    auto initial_count = allocator.allocated_count();
    
    std::cout << "Initial state: " << initial_available << " bytes available, " 
              << initial_count << " objects\n";
    
    // Allocate a sequence of objects
    std::vector<Vehicle*> vehicles;
    std::vector<std::string> models = {"Toyota Camry", "Honda Accord", "Ford F-150"};
    
    std::cout << "\nAllocating sequence of vehicles:\n";
    for (size_t i = 0; i < models.size(); ++i) {
        auto vehicle = allocator.allocate<Vehicle>(models[i], 2023, 30000.0 + i * 5000);
        if (vehicle) {
            vehicles.push_back(*vehicle);
        }
    }
    
    auto after_alloc_available = allocator.available_bytes();
    auto after_alloc_count = allocator.allocated_count();
    
    std::cout << "After allocation: " << after_alloc_available << " bytes available, " 
              << after_alloc_count << " objects\n";
    
    // Deallocate in reverse order
    std::cout << "\nDeallocating in reverse order:\n";
    for (auto it = vehicles.rbegin(); it != vehicles.rend(); ++it) {
        allocator.deallocate(*it);
    }
    
    auto final_available = allocator.available_bytes();
    auto final_count = allocator.allocated_count();
    
    std::cout << "Final state: " << final_available << " bytes available, " 
              << final_count << " objects\n";
    
    // Verify state reversibility
    if (final_available == initial_available && final_count == initial_count) {
        std::cout << "✓ State successfully reversed!\n";
        
        // Verify the same sequence can be allocated again
        std::cout << "\nTesting same sequence allocation again:\n";
        vehicles.clear();
        for (size_t i = 0; i < models.size(); ++i) {
            auto vehicle = allocator.allocate<Vehicle>(models[i], 2023, 30000.0 + i * 5000);
            if (vehicle) {
                vehicles.push_back(*vehicle);
            }
        }
        
        if (allocator.available_bytes() == after_alloc_available && 
            allocator.allocated_count() == after_alloc_count) {
            std::cout << "✓ Same allocation sequence succeeded with identical state!\n";
        }
        
        // Clean up
        for (auto vehicle : vehicles) {
            allocator.deallocate(vehicle);
        }
        
    } else {
        std::cout << "✗ State not properly reversed\n";
    }
}

void demonstrate_failure_handling() {
    std::cout << "\n=== Failure Handling Example ===\n";
    
    // Create small allocator that will run out of memory
    MemoryAllocator allocator(256);  // Very small pool
    
    std::cout << "Created allocator with " << allocator.total_size() << " bytes\n";
    
    std::vector<Vehicle*> vehicles;
    int allocation_count = 0;
    
    std::cout << "\nAllocating until failure:\n";
    while (true) {
        auto vehicle = allocator.allocate<Vehicle>("Test Car", 2023, 25000.0);
        if (!vehicle) {
            std::cout << "  Allocation failed after " << allocation_count << " successful allocations\n";
            break;
        }
        vehicles.push_back(*vehicle);
        ++allocation_count;
        
        if (allocation_count % 5 == 0) {
            std::cout << "  Allocated " << allocation_count << " vehicles, " 
                      << allocator.available_bytes() << " bytes remaining\n";
        }
    }
    
    // Record state when allocation fails
    auto fail_available = allocator.available_bytes();
    auto fail_count = allocator.allocated_count();
    
    std::cout << "\nState when allocation failed: " << fail_available 
              << " bytes available, " << fail_count << " objects\n";
    
    // Try same allocation again - should still fail
    auto retry = allocator.allocate<Vehicle>("Another Car", 2023, 25000.0);
    if (!retry) {
        std::cout << "✓ Retry allocation correctly failed\n";
        
        // State should be unchanged
        if (allocator.available_bytes() == fail_available && 
            allocator.allocated_count() == fail_count) {
            std::cout << "✓ State unchanged after failed allocation\n";
        }
    }
    
    // Clean up
    std::cout << "\nCleaning up allocated vehicles:\n";
    for (auto vehicle : vehicles) {
        allocator.deallocate(vehicle);
    }
    
    std::cout << "Final state: " << allocator.available_bytes() 
              << " bytes available, " << allocator.allocated_count() << " objects\n";
}

int main() {
    std::cout << "Rightware Memory Allocator - Usage Examples\n";
    std::cout << "==========================================\n";
    
    try {
        demonstrate_basic_usage();
        demonstrate_state_reversibility();
        demonstrate_failure_handling();
        
        std::cout << "\n=== All Examples Completed Successfully ===\n";
        
    } catch (const std::exception& e) {
        std::cout << "\n✗ Example failed with exception: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
