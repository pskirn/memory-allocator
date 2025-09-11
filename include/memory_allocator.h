#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <map>
#include <set>
#include <utility>
#include <cassert>
#include <algorithm>

namespace rightware {

/**
 * A deterministic memory allocator specialized for embedded systems.
 * 
 * This allocator provides O(log n) allocation/deallocation performance
 * with strong state guarantees and deterministic behavior.
 */
class MemoryAllocator {
public:
    /**
     * Constructs a memory allocator with a fixed-size memory pool.
     * 
     * @param pool_size Size of the memory pool in bytes
     */
    explicit MemoryAllocator(size_t pool_size);
    
    /**
     * Destructor - ensures all objects are properly destroyed
     */
    ~MemoryAllocator();
    
    // Non-copyable, non-movable for simplicity
    MemoryAllocator(const MemoryAllocator&) = delete;
    MemoryAllocator& operator=(const MemoryAllocator&) = delete;
    MemoryAllocator(MemoryAllocator&&) = delete;
    MemoryAllocator& operator=(MemoryAllocator&&) = delete;
    
    /**
     * Allocates memory for an object of type T and constructs it.
     * 
     * @tparam T The type to allocate and construct
     * @tparam Args Constructor argument types
     * @param args Arguments to forward to T's constructor
     * @return Pointer to constructed object, or nullptr if allocation failed
     * 
     * Complexity: O(log n) amortized, O(n) worst case
     * 
     * Failure modes:
     * - Returns nullptr if insufficient memory available
     * - Returns nullptr if alignment requirements cannot be met
     */
    template<typename T, typename... Args>
    std::optional<T*> allocate(Args&&... args);
    
    /**
     * Destroys an object and deallocates its memory.
     * 
     * @tparam T The type to destroy and deallocate
     * @param ptr Pointer to object (must be previously allocated by this allocator)
     * 
     * Complexity: O(log n)
     * 
     * Preconditions:
     * - ptr must be a valid pointer returned by allocate()
     * - ptr must not have been previously deallocated
     * 
     * Note: This operation never fails for valid pointers
     */
    template<typename T>
    void deallocate(T* ptr);
    
    /**
     * Returns the total size of the memory pool.
     */
    size_t total_size() const noexcept { return pool_size_; }
    
    /**
     * Returns the number of bytes currently allocated.
     */
    size_t allocated_bytes() const noexcept { return allocated_bytes_; }
    
    /**
     * Returns the number of bytes available for allocation.
     */
    size_t available_bytes() const noexcept { 
        return total_size() - allocated_bytes(); 
    }
    
    /**
     * Returns the number of currently allocated objects.
     */
    size_t allocated_count() const noexcept { 
        return allocated_blocks_.size(); 
    }

private:
    struct BlockInfo {
        size_t size;
        size_t alignment;
        void (*destructor)(void*);
    };
    
    // Memory pool - pre-allocated chunk
    std::unique_ptr<char[]> memory_pool_;
    size_t pool_size_;
    
    // Free blocks: offset -> size
    std::map<size_t, size_t> free_blocks_;
    
    // Allocated blocks: pointer -> block info
    std::map<void*, BlockInfo> allocated_blocks_;
    
    // Track total allocated bytes
    size_t allocated_bytes_;
    
    /**
     * Finds a free block that can accommodate the requested size and alignment.
     * 
     * @param size Required size in bytes
     * @param alignment Required alignment in bytes
     * @return Offset in memory pool, or nullopt if no suitable block found
     */
    std::optional<size_t> find_free_block(size_t size, size_t alignment);
    
    /**
     * Allocates a block at the specified offset.
     * 
     * @param offset Offset in memory pool
     * @param size Size to allocate
     * @param alignment Alignment requirement
     * @return Pointer to allocated memory
     */
    void* allocate_at_offset(size_t offset, size_t size, size_t alignment);
    
    /**
     * Returns a block to the free list and merges with adjacent free blocks.
     * 
     * @param ptr Pointer to deallocate
     * @param size Size of the block
     */
    void deallocate_block(void* ptr, size_t size);
    
    /**
     * Merges adjacent free blocks to reduce fragmentation.
     */
    void merge_free_blocks();
    
    /**
     * Converts pointer to offset in memory pool.
     */
    size_t ptr_to_offset(void* ptr) const;
    
    /**
     * Converts offset to pointer in memory pool.
     */
    void* offset_to_ptr(size_t offset) const;
    
    /**
     * Checks if a pointer is valid (within memory pool bounds).
     */
    bool is_valid_pointer(void* ptr) const;
};

// Template implementations

template<typename T, typename... Args>
std::optional<T*> MemoryAllocator::allocate(Args&&... args) {
    static_assert(std::is_destructible_v<T>, "Type T must be destructible");
    
    constexpr size_t size = sizeof(T);
    constexpr size_t alignment = alignof(T);
    
    // Find suitable free block
    auto offset_opt = find_free_block(size, alignment);
    if (!offset_opt) {
        return std::nullopt;  // Allocation failed
    }
    
    // Allocate memory at the found offset
    void* memory = allocate_at_offset(*offset_opt, size, alignment);
    if (!memory) {
        return std::nullopt;
    }
    
    // Construct object using placement new
    T* object = nullptr;
    try {
        object = new(memory) T(std::forward<Args>(args)...);
    } catch (...) {
        // Constructor failed, return memory to free pool
        deallocate_block(memory, size);
        return std::nullopt;
    }
    
    // Track allocation with destructor info
    allocated_blocks_[object] = BlockInfo{
        size, 
        alignment,
        [](void* ptr) { static_cast<T*>(ptr)->~T(); }
    };
    
    allocated_bytes_ += size;
    
    return object;
}

template<typename T>
void MemoryAllocator::deallocate(T* ptr) {
    static_assert(std::is_destructible_v<T>, "Type T must be destructible");
    
    if (!ptr) {
        return;  // Allow deallocation of nullptr
    }
    
    // Verify this is a valid allocated pointer
    auto it = allocated_blocks_.find(ptr);
    assert(it != allocated_blocks_.end() && "Invalid pointer or double deallocation");
    
    if (it == allocated_blocks_.end()) {
        return;  // Invalid pointer - in production, this might log an error
    }
    
    const BlockInfo& info = it->second;
    
    // Call destructor
    info.destructor(ptr);
    
    // Return memory to free pool
    deallocate_block(ptr, info.size);
    
    // Update tracking
    allocated_bytes_ -= info.size;
    allocated_blocks_.erase(it);
}

} // namespace rightware
