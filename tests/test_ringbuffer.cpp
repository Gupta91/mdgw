#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <atomic>

#include "util/SPSCRingBuffer.hpp"

using mdgw::util::SPSCRingBuffer;

TEST_CASE("SPSCRingBuffer basic operations", "[ringbuffer]") {
    SPSCRingBuffer<int, 16> buffer;
    
    REQUIRE(buffer.empty());
    REQUIRE(buffer.size() == 0);
    
    // Test single push/pop
    REQUIRE(buffer.tryEmplace(42));
    REQUIRE(buffer.size() == 1);
    REQUIRE_FALSE(buffer.empty());
    
    int value;
    REQUIRE(buffer.tryPop(value));
    REQUIRE(value == 42);
    REQUIRE(buffer.empty());
    REQUIRE(buffer.size() == 0);
}

TEST_CASE("SPSCRingBuffer capacity limits", "[ringbuffer]") {
    SPSCRingBuffer<int, 4> buffer;
    
    // Fill to capacity-1 (one slot reserved)
    REQUIRE(buffer.tryEmplace(1));
    REQUIRE(buffer.tryEmplace(2));
    REQUIRE(buffer.tryEmplace(3));
    
    // Buffer should be full now
    REQUIRE_FALSE(buffer.tryEmplace(4));
    
    // Pop one, should allow one more push
    int value;
    REQUIRE(buffer.tryPop(value));
    REQUIRE(value == 1);
    REQUIRE(buffer.tryEmplace(4));
}

TEST_CASE("SPSCRingBuffer threaded producer/consumer", "[ringbuffer]") {
    SPSCRingBuffer<int, 1024> buffer;
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    constexpr int kNumItems = 10000;
    
    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < kNumItems; ++i) {
            while (!buffer.tryEmplace(i)) {
                std::this_thread::yield();
            }
            produced.fetch_add(1);
        }
    });
    
    // Consumer thread
    std::thread consumer([&]() {
        int value;
        for (int i = 0; i < kNumItems; ++i) {
            while (!buffer.tryPop(value)) {
                std::this_thread::yield();
            }
            REQUIRE(value == i); // Values should come in order
            consumed.fetch_add(1);
        }
    });
    
    producer.join();
    consumer.join();
    
    REQUIRE(produced.load() == kNumItems);
    REQUIRE(consumed.load() == kNumItems);
    REQUIRE(buffer.empty());
}
