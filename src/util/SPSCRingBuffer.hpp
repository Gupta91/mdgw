#pragma once

#include <atomic>
#include <memory>
#include <array>
#include <cstddef>

namespace mdgw::util {

// Lock-free SPSC ring buffer optimized for low latency
template<typename T, size_t Capacity>
class SPSCRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    
public:
    SPSCRingBuffer() : head_(0), tail_(0) {}
    
    // Producer side (I/O thread)
    template<typename... Args>
    bool tryEmplace(Args&&... args) {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t next_head = (head + 1) & mask_;
        
        if (next_head == tail_.load(std::memory_order_acquire)) {
            return false; // Buffer full
        }
        
        new (&buffer_[head]) T(std::forward<Args>(args)...);
        head_.store(next_head, std::memory_order_release);
        return true;
    }
    
    // Consumer side (book thread)
    bool tryPop(T& item) {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        
        if (tail == head_.load(std::memory_order_acquire)) {
            return false; // Buffer empty
        }
        
        T* obj = reinterpret_cast<T*>(&buffer_[tail]);
        item = std::move(*obj);
        obj->~T();
        tail_.store((tail + 1) & mask_, std::memory_order_release);
        return true;
    }
    
    size_t size() const {
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t tail = tail_.load(std::memory_order_acquire);
        return (head - tail) & mask_;
    }
    
    bool empty() const {
        return head_.load(std::memory_order_acquire) == 
               tail_.load(std::memory_order_acquire);
    }
    
private:
    static constexpr size_t mask_ = Capacity - 1;
    
    // Cache line alignment to avoid false sharing
    alignas(64) std::atomic<size_t> head_;
    alignas(64) std::atomic<size_t> tail_;
    alignas(64) std::array<typename std::aligned_storage<sizeof(T), alignof(T)>::type, Capacity> buffer_;
};

}  // namespace mdgw::util
