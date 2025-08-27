#pragma once

#include "MarketDataGateway.hpp"
#include "book/OrderBook.hpp"
#include "metrics/Metrics.hpp"
#include "util/Time.hpp"
#include "util/SPSCRingBuffer.hpp"
#include "BookUpdate.hpp"

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>

namespace mdgw {

class OkxMarketDataGateway : public MarketDataGateway {
public:
  OkxMarketDataGateway();
  ~OkxMarketDataGateway() override;

  void setInstruments(const std::vector<std::string>& instruments) override;
  void setBestQuoteCallback(BestQuoteCallback callback) override;
  void start() override;
  void stop() override;

private:
  // I/O thread functions
  void ioThreadRun();
  bool connectAndSubscribe();
  bool readOnceAndProcess();
  
  // Book processing thread functions
  void bookThreadRun();
  void processBookUpdate(const gateway::BookUpdate& update);

  std::vector<std::string> instruments_;
  BestQuoteCallback bestQuoteCallback_;

  std::unordered_map<std::string, book::OrderBook> books_;

  std::atomic<bool> running_ {false};
  std::thread ioThread_;
  std::thread bookThread_;
  
  // Lock-free ring buffer for I/O -> Book communication
  static constexpr size_t kRingBufferSize = 4096; // Power of 2
  util::SPSCRingBuffer<gateway::BookUpdate, kRingBufferSize> ringBuffer_;

  // Networking
  using tcp = boost::asio::ip::tcp;
  boost::asio::io_context ioc_;
  boost::asio::ssl::context ssl_{boost::asio::ssl::context::tlsv12_client};
  std::unique_ptr<boost::beast::websocket::stream<boost::beast::ssl_stream<tcp::socket>>> ws_;
};

}  // namespace mdgw


