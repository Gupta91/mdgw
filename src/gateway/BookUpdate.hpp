#pragma once

#include <string>
#include <vector>
#include <utility>

namespace mdgw::gateway {

// Message passed through ring buffer from I/O to book thread
struct BookUpdate {
    std::string instrumentId;
    std::vector<std::pair<double, double>> bids;
    std::vector<std::pair<double, double>> asks;
    long long receiveTimeNs;
    std::string checksum; // For validation
    bool isSnapshot; // true for snapshot, false for incremental
    
    BookUpdate() = default;
    
    BookUpdate(std::string instId, 
               std::vector<std::pair<double, double>> bids_,
               std::vector<std::pair<double, double>> asks_,
               long long recvNs,
               bool snapshot = false,
               std::string cs = "")
        : instrumentId(std::move(instId))
        , bids(std::move(bids_))
        , asks(std::move(asks_))
        , receiveTimeNs(recvNs)
        , checksum(std::move(cs))
        , isSnapshot(snapshot) {}
    
    // Move semantics for efficiency
    BookUpdate(BookUpdate&&) = default;
    BookUpdate& operator=(BookUpdate&&) = default;
    
    // Disable copy to force move semantics
    BookUpdate(const BookUpdate&) = delete;
    BookUpdate& operator=(const BookUpdate&) = delete;
};

}  // namespace mdgw::gateway
