#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

namespace mdgw::metrics {

struct RateCounter {
  std::atomic<uint64_t> count {0};
  void inc(uint64_t n = 1) { count.fetch_add(n, std::memory_order_relaxed); }
  uint64_t reset() { return count.exchange(0, std::memory_order_acq_rel); }
};

class MetricsRegistry {
public:
  void registerInstrument(const std::string& inst) {
    std::lock_guard<std::mutex> lg(mutex_);
    (void)updates_[inst];
    (void)latency_[inst];
    instruments_.push_back(inst);
  }

  void incUpdates(const std::string& inst) { updates_[inst].inc(); }
  uint64_t resetUpdates(const std::string& inst) { return updates_[inst].reset(); }

  void addLatencyNs(const std::string& inst, uint64_t ns) {
    auto& p = latency_[inst];
    p.first.fetch_add(ns, std::memory_order_relaxed);
    p.second.fetch_add(1, std::memory_order_relaxed);
  }

  std::pair<uint64_t, uint64_t> resetLatency(const std::string& inst) {
    auto& p = latency_[inst];
    return {p.first.exchange(0, std::memory_order_acq_rel),
            p.second.exchange(0, std::memory_order_acq_rel)};
  }

  std::vector<std::string> instruments() const {
    std::lock_guard<std::mutex> lg(mutex_);
    return instruments_;
  }

private:
  std::unordered_map<std::string, RateCounter> updates_;
  std::unordered_map<std::string, std::pair<std::atomic<uint64_t>, std::atomic<uint64_t>>> latency_;
  mutable std::mutex mutex_;
  std::vector<std::string> instruments_;
};

class MetricsReporter {
public:
  MetricsReporter(MetricsRegistry& registry,
                  int rateIntervalSeconds = 5,
                  int latencyIntervalSeconds = 60)
      : registry_(registry),
        rateIntervalSeconds_(rateIntervalSeconds),
        latencyIntervalSeconds_(latencyIntervalSeconds) {}

  void start() {
    running_.store(true, std::memory_order_release);
    thread_ = std::thread([this] { run(); });
  }

  void stop() {
    running_.store(false, std::memory_order_release);
    if (thread_.joinable()) thread_.join();
  }

  ~MetricsReporter() { stop(); }

private:
  void run() {
    using clock = std::chrono::steady_clock;
    auto nextRate = clock::now() + std::chrono::seconds(rateIntervalSeconds_);
    auto nextLatency = clock::now() + std::chrono::seconds(latencyIntervalSeconds_);

    while (running_.load(std::memory_order_acquire)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      auto now = clock::now();

      if (now >= nextRate) {
        const auto insts = registry_.instruments();
        for (const auto& inst : insts) {
          const auto updates = registry_.resetUpdates(inst);
          const double rate = static_cast<double>(updates) / static_cast<double>(rateIntervalSeconds_);
          spdlog::info("[metrics] {} updates/sec: {:.2f}", inst, rate);
        }
        nextRate = now + std::chrono::seconds(rateIntervalSeconds_);
      }

      if (now >= nextLatency) {
        const auto insts = registry_.instruments();
        for (const auto& inst : insts) {
          auto [totalNs, count] = registry_.resetLatency(inst);
          if (count > 0) {
            const double avgUs = static_cast<double>(totalNs) / static_cast<double>(count) / 1000.0;
            spdlog::info("[metrics] {} avg tick->book latency over {}s: {:.2f} us (n={})",
                         inst, latencyIntervalSeconds_, avgUs, count);
          } else {
            spdlog::info("[metrics] {} avg tick->book latency over {}s: n=0", inst, latencyIntervalSeconds_);
          }
        }
        nextLatency = now + std::chrono::seconds(latencyIntervalSeconds_);
      }
    }
  }

  MetricsRegistry& registry_;
  int rateIntervalSeconds_;
  int latencyIntervalSeconds_;
  std::atomic<bool> running_ {false};
  std::thread thread_;
};

}


