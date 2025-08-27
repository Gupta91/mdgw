#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace mdgw {

struct BestQuote {
  std::string instrumentId;
  double bestBidPrice {0.0};
  double bestBidSize {0.0};
  double bestAskPrice {0.0};
  double bestAskSize {0.0};
  // Nanoseconds between WS receipt and book updated
  long long tickToBookLatencyNs {0};
};

class MarketDataGateway {
public:
  using BestQuoteCallback = std::function<void(const BestQuote&)>;

  virtual ~MarketDataGateway() = default;

  virtual void setInstruments(const std::vector<std::string>& instruments) = 0;
  virtual void setBestQuoteCallback(BestQuoteCallback callback) = 0;

  // Establish network resources but do not block.
  virtual void start() = 0;

  // Stop network resources and join threads.
  virtual void stop() = 0;
};

}  // namespace mdgw


