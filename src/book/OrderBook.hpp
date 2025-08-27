#pragma once

#include <map>
#include <string>
#include <utility>

namespace mdgw::book {

class OrderBook {
public:
  explicit OrderBook(std::string instrumentId) : instrumentId_(std::move(instrumentId)) {}

  void clear() {
    bids_.clear();
    asks_.clear();
  }

  // Snapshot replace at price levels.
  void applySnapshot(const std::map<double, double>& bids, const std::map<double, double>& asks) {
    bids_ = bids;
    asks_ = asks;
  }

  // Incremental update: size == 0 removes level.
  void applyDeltaBid(double price, double size) {
    if (size == 0.0) {
      bids_.erase(price);
    } else {
      bids_[price] = size;
    }
  }

  void applyDeltaAsk(double price, double size) {
    if (size == 0.0) {
      asks_.erase(price);
    } else {
      asks_[price] = size;
    }
  }

  std::pair<double, double> bestBid() const {
    if (bids_.empty()) return {0.0, 0.0};
    auto it = bids_.rbegin();
    return {it->first, it->second};
  }

  std::pair<double, double> bestAsk() const {
    if (asks_.empty()) return {0.0, 0.0};
    auto it = asks_.begin();
    return {it->first, it->second};
  }

  size_t bidLevels() const { return bids_.size(); }
  size_t askLevels() const { return asks_.size(); }
  const std::string& instrumentId() const { return instrumentId_; }

private:
  std::string instrumentId_;
  // Price-level aggregated maps: bids ascending; use rbegin for best.
  std::map<double, double> bids_;
  std::map<double, double> asks_;
};

}  // namespace mdgw::book


