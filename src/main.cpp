#include <csignal>
#include <atomic>
#include <iostream>

#include <spdlog/spdlog.h>

#include "gateway/OkxMarketDataGateway.hpp"
#include "metrics/Metrics.hpp"
#include "util/Log.hpp"

using mdgw::OkxMarketDataGateway;
using mdgw::metrics::MetricsRegistry;
using mdgw::metrics::MetricsReporter;

static std::atomic<bool> g_running{true};

static void handle_sigint(int) { g_running.store(false); }

int main() {
  mdgw::log::init();
  std::signal(SIGINT, handle_sigint);

  MetricsRegistry metrics;
  metrics.registerInstrument("BTC-USDT-SWAP");
  metrics.registerInstrument("ETH-USDT-SWAP");

  OkxMarketDataGateway gw;
  gw.setInstruments({"BTC-USDT-SWAP", "ETH-USDT-SWAP"});

  mdgw::BestQuote lastBtc{};
  mdgw::BestQuote lastEth{};

  gw.setBestQuoteCallback([&](const mdgw::BestQuote& q) {
    metrics.incUpdates(q.instrumentId);
    metrics.addLatencyNs(q.instrumentId, static_cast<uint64_t>(q.tickToBookLatencyNs));

    auto& last = (q.instrumentId == std::string("BTC-USDT-SWAP")) ? lastBtc : lastEth;
    const bool changed = (q.bestBidPrice != last.bestBidPrice) || (q.bestBidSize != last.bestBidSize) ||
                         (q.bestAskPrice != last.bestAskPrice) || (q.bestAskSize != last.bestAskSize);
    if (changed) {
      spdlog::info("{} BB {:.2f}x{:.6f} | BA {:.2f}x{:.6f}", q.instrumentId,
                   q.bestBidPrice, q.bestBidSize, q.bestAskPrice, q.bestAskSize);
      last = q;
    }
  });

  // Start the gateway first
  spdlog::debug("Starting OKX Market Data Gateway...");
  gw.start();
  
  // Wait a moment for initial connection attempt
  std::this_thread::sleep_for(std::chrono::seconds(3));
  
  // Now start the metrics reporter
  MetricsReporter reporter(metrics);
  reporter.start();

  while (g_running.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  gw.stop();
  reporter.stop();
  spdlog::info("Shutdown complete");
  return 0;
}


