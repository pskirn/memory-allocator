#include "memory_allocator.h"
#include <cstring>
#include <stdexcept>

namespace rightware {

MemoryAllocator::MemoryAllocator(size_t pool_size) 
    : memory_pool_(std::make_unique<char[]>(pool_size))
    , pool_size_(pool_size)
    , allocated_bytes_(0) {
    
    if (pool_size == 0) {
        throw std::invalid_argument("Pool size must be greater than 0");
    }
    
    // Initially, the entire pool is one large free block
    free_blocks_[0] = pool_size;
}

MemoryAllocator::~MemoryAllocator() {
    // Destroy all remaining allocated objects
    for (auto& [ptr, info] : allocated_blocks_) {
        info.destructor(ptr);
    }
    allocated_blocks_.clear();
}

std::optional<size_t> MemoryAllocator::find_free_block(size_t size, size_t alignment) {
    for (auto& [offset, block_size] : free_blocks_) {
        // Calculate aligned start position within this block
        void* block_start = offset_to_ptr(offset);
        void* aligned_ptr = block_start;
        size_t space = block_size;
        
        // Use std::align to find properly aligned address
        if (std::align(alignment, size, aligned_ptr, space)) {
            // Check if aligned memory fits in the block
            size_t aligned_offset = ptr_to_offset(aligned_ptr);
            size_t required_space = size;
            
            if (space >= required_space) {
                return aligned_offset;
            }
        }
    }
    
    return std::nullopt;  // No suitable block found
}

void* MemoryAllocator::allocate_at_offset(size_t offset, size_t size, size_t alignment) {
    (void)alignment;  // Mark as intentionally unused - alignment already handled in find_free_block
    // Find the free block containing this offset
    auto it = free_blocks_.upper_bound(offset);
    if (it != free_blocks_.begin()) {
        --it;
    }
    
    if (it == free_blocks_.end() || it->first > offset) {
        return nullptr;  // Invalid offset
    }
    
    size_t block_start = it->first;
    size_t block_size = it->second;
    size_t block_end = block_start + block_size;
    
    if (offset + size > block_end) {
        return nullptr;  // Doesn't fit
    }
    
    // Remove the old free block
    free_blocks_.erase(it);
    
    // Create new free blocks for unused portions
    // Before the allocation
    if (offset > block_start) {
        free_blocks_[block_start] = offset - block_start;
    }
    
    // After the allocation
    size_t allocation_end = offset + size;
    if (allocation_end < block_end) {
        free_blocks_[allocation_end] = block_end - allocation_end;
    }
    
    return offset_to_ptr(offset);
}

void MemoryAllocator::deallocate_block(void* ptr, size_t size) {
    if (!is_valid_pointer(ptr)) {
        return;  // Invalid pointer
    }
    
    size_t offset = ptr_to_offset(ptr);
    
    // Add block back to free list
    free_blocks_[offset] = size;
    
    // Merge with adjacent free blocks
    merge_free_blocks();
}

void MemoryAllocator::merge_free_blocks() {
    if (free_blocks_.empty()) {
        return;
    }
    
    auto current = free_blocks_.begin();
    
    while (current != free_blocks_.end()) {
        auto next = std::next(current);
        
        // Check if current block is adjacent to next block
        if (next != free_blocks_.end()) {
            size_t current_end = current->first + current->second;
            
            if (current_end == next->first) {
                // Merge blocks
                current->second += next->second;
                free_blocks_.erase(next);
                // Don't increment current, check for more merges
                continue;
            }
        }
        
        ++current;
    }
}

size_t MemoryAllocator::ptr_to_offset(void* ptr) const {
    if (!ptr) {
        return 0;
    }
    
    char* char_ptr = static_cast<char*>(ptr);
    char* pool_start = memory_pool_.get();
    
    return static_cast<size_t>(char_ptr - pool_start);
}

void* MemoryAllocator::offset_to_ptr(size_t offset) const {
    if (offset >= pool_size_) {
        return nullptr;
    }
    
    return memory_pool_.get() + offset;
}

bool MemoryAllocator::is_valid_pointer(void* ptr) const {
    if (!ptr) {
        return false;
    }
    
    char* char_ptr = static_cast<char*>(ptr);
    char* pool_start = memory_pool_.get();
    char* pool_end = pool_start + pool_size_;
    
    return char_ptr >= pool_start && char_ptr < pool_end;
}

} // namespace rightware
