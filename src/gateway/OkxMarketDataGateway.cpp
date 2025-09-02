#include "OkxMarketDataGateway.hpp"
#include "BookUpdate.hpp"

#include <spdlog/spdlog.h>

#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>

// Suppress deprecated iterator warnings from RapidJSON
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#pragma GCC diagnostic pop

#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <openssl/ssl.h>
#include <zlib.h>

namespace mdgw {

namespace {
constexpr const char* kHost = "ws.okx.com";  // public market data
constexpr const char* kPort = "443";
constexpr const char* kPath = "/ws/v5/public";

std::string computeOkxChecksum(const std::vector<std::pair<double,double>>& bids,
                               const std::vector<std::pair<double,double>>& asks) {
  std::ostringstream oss;
  for (const auto& [px, sz] : bids) {
    oss << std::fixed << std::setprecision(8) << px << ":" << sz << ":";
  }
  for (const auto& [px, sz] : asks) {
    oss << std::fixed << std::setprecision(8) << px << ":" << sz << ":";
  }
  
  std::string data = oss.str();
  if (!data.empty() && data.back() == ':') {
    data.pop_back(); // Remove trailing ':'
  }
  
  uLong crc = crc32(0L, Z_NULL, 0);
  crc = crc32(crc, reinterpret_cast<const Bytef*>(data.c_str()), static_cast<uInt>(data.length()));
  return std::to_string(crc);
}
}

OkxMarketDataGateway::OkxMarketDataGateway() {
  SSL_CTX_set_options(ssl_.native_handle(), SSL_OP_NO_COMPRESSION);
  // For simplicity in this exercise, disable certificate verification.
  // In production, load and verify against a proper CA bundle.
  ssl_.set_verify_mode(boost::asio::ssl::verify_none);
}

OkxMarketDataGateway::~OkxMarketDataGateway() { stop(); }

void OkxMarketDataGateway::setInstruments(const std::vector<std::string>& instruments) {
  instruments_ = instruments;
  books_.clear();
  for (const auto& inst : instruments_) {
    books_.emplace(inst, book::OrderBook(inst));
  }
}

void OkxMarketDataGateway::setBestQuoteCallback(BestQuoteCallback callback) {
  bestQuoteCallback_ = std::move(callback);
}

void OkxMarketDataGateway::start() {
  if (running_.exchange(true)) return;
  ioThread_ = std::thread([this] { ioThreadRun(); });
  bookThread_ = std::thread([this] { bookThreadRun(); });
}

void OkxMarketDataGateway::stop() {
  if (!running_.exchange(false)) return;
  if (ws_) {
    boost::system::error_code ec;
    ws_->next_layer().next_layer().shutdown(tcp::socket::shutdown_both, ec);
    ws_.reset();
  }
  if (ioThread_.joinable()) ioThread_.join();
  if (bookThread_.joinable()) bookThread_.join();
}

void OkxMarketDataGateway::ioThreadRun() {
  while (running_.load()) {
    try {
      if (!connectAndSubscribe()) {
        spdlog::warn("OKX connect failed; retrying in 2s");
        std::this_thread::sleep_for(std::chrono::seconds(2));
        continue;
      }

      while (running_.load()) {
        if (!readOnceAndProcess()) {
          spdlog::warn("OKX read/process failed; reconnecting");
          break;
        }
      }
    } catch (const std::exception& ex) {
      spdlog::error("Exception in ioThreadRun(): {}", ex.what());
    }
  }
}

void OkxMarketDataGateway::bookThreadRun() {
  gateway::BookUpdate update;
  while (running_.load()) {
    if (ringBuffer_.tryPop(update)) {
      processBookUpdate(update);
    } else {
      // No data available, yield CPU briefly
      std::this_thread::sleep_for(std::chrono::nanoseconds(100));
    }
  }
  
  // Process remaining updates during shutdown
  while (ringBuffer_.tryPop(update)) {
    processBookUpdate(update);
  }
}

bool OkxMarketDataGateway::connectAndSubscribe() {
  using namespace boost;
  system::error_code ec;

  
  
  tcp::resolver resolver{ioc_};
  
  auto const results = resolver.resolve(kHost, kPort, ec);
  if (ec) {
    spdlog::error("resolve error: {}", ec.message());
    return false;
  }
  

  auto socket = std::make_unique<beast::ssl_stream<tcp::socket>>(ioc_, ssl_);
  
  asio::connect(socket->next_layer(), results, ec);
  if (ec) {
    spdlog::error("connect error: {}", ec.message());
    return false;
  }
  

  // Enable SNI (Server Name Indication) for TLS
  
  if (!SSL_set_tlsext_host_name(socket->native_handle(), kHost)) {
    spdlog::error("failed to set SNI host name");
    return false;
  }

  socket->handshake(asio::ssl::stream_base::client, ec);
  if (ec) {
    spdlog::error("tls handshake error: {}", ec.message());
    return false;
  }
  

  ws_ = std::make_unique<beast::websocket::stream<beast::ssl_stream<tcp::socket>>>(std::move(*socket));
  ws_->set_option(beast::websocket::stream_base::timeout::suggested(beast::role_type::client));
  ws_->set_option(beast::websocket::stream_base::decorator(
      [](beast::websocket::request_type& req) {
        req.set(beast::http::field::user_agent, std::string{"mdgw/0.1"});
      }));

  
  ws_->handshake(kHost, kPath, ec);
  if (ec) {
    spdlog::error("ws handshake error: {}", ec.message());
    return false;
  }
  

  // Subscribe to books (full depth) for instruments
  
  rapidjson::Document sub;
  sub.SetObject();
  auto& alloc = sub.GetAllocator();
  sub.AddMember("op", "subscribe", alloc);
  rapidjson::Value args(rapidjson::kArrayType);
  for (const auto& inst : instruments_) {
    rapidjson::Value obj(rapidjson::kObjectType);
    obj.AddMember("channel", "books", alloc);
    obj.AddMember("instId", rapidjson::Value(inst.c_str(), alloc), alloc);
    args.PushBack(obj, alloc);
  }
  sub.AddMember("args", args, alloc);

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  sub.Accept(writer);

  ws_->write(boost::asio::buffer(std::string(buffer.GetString(), buffer.GetSize())), ec);
  if (ec) {
    spdlog::error("ws write subscribe error: {}", ec.message());
    return false;
  }
  

  return true;
}

bool OkxMarketDataGateway::readOnceAndProcess() {
  using namespace boost;
  system::error_code ec;
  beast::flat_buffer buffer;
  ws_->read(buffer, ec);
  if (ec) {
    spdlog::warn("ws read error: {}", ec.message());
    return false;
  }

  const long long recvNs = timeutil::nowSteadyNanos();
  auto data = beast::buffers_to_string(buffer.cdata());

  rapidjson::Document d;
  d.Parse(data.c_str());
  if (!d.IsObject()) {
    return true;
  }
  if (!d.HasMember("arg")) {
    return true;  // ignore events
  }
  const auto& arg = d["arg"];
  if (!arg.HasMember("channel")) {
    return true;
  }
  if (std::string(arg["channel"].GetString()) != "books") {
    return true;
  }
  if (!d.HasMember("data")) {
    return true;
  }
  

  const auto& dataArr = d["data"];
  if (!dataArr.IsArray() || dataArr.Empty()) {
    return true;
  }
  const auto& book = dataArr[0];
  
  // Get instrument ID from arg, not from book data
  std::string instId = arg["instId"].GetString();
  
  
  // Check if this is a snapshot (action="snapshot") or incremental update (action="update")
  // The action is at the root level, not in the book data
  bool isSnapshot = false;
  if (d.HasMember("action") && d["action"].IsString()) {
    std::string action = d["action"].GetString();
    isSnapshot = (action == "snapshot");
    
  }

  std::vector<std::pair<double,double>> bids;
  std::vector<std::pair<double,double>> asks;
  std::string receivedChecksum;

  if (book.HasMember("bids") && book["bids"].IsArray()) {
    for (auto& lvl : book["bids"].GetArray()) {
      if (lvl.IsArray() && lvl.Size() >= 2) {
        double px = std::stod(lvl[0].GetString());
        double sz = std::stod(lvl[1].GetString());
        bids.emplace_back(px, sz);
      }
    }
  }
  if (book.HasMember("asks") && book["asks"].IsArray()) {
    for (auto& lvl : book["asks"].GetArray()) {
      if (lvl.IsArray() && lvl.Size() >= 2) {
        double px = std::stod(lvl[0].GetString());
        double sz = std::stod(lvl[1].GetString());
        asks.emplace_back(px, sz);
      }
    }
  }
  
  // Extract checksum if present
  if (book.HasMember("cs") && book["cs"].IsString()) {
    receivedChecksum = book["cs"].GetString();
  }

  // Validate checksum
  if (!receivedChecksum.empty()) {
    std::string computedChecksum = computeOkxChecksum(bids, asks);
    if (receivedChecksum != computedChecksum) {
      spdlog::warn("Checksum mismatch for {}: received={}, computed={}", instId, receivedChecksum, computedChecksum);
      // In production, trigger re-subscription here
      return true; // Continue processing despite mismatch for now
    }
  }

  // Push to ring buffer (non-blocking I/O thread)
  
  if (!ringBuffer_.tryEmplace(std::move(instId), std::move(bids), std::move(asks), recvNs, isSnapshot, std::move(receivedChecksum))) {
    spdlog::warn("Ring buffer full, dropping update for {}", instId);
    // In production, consider expanding buffer or applying backpressure
  }
  
  return true;
}

void OkxMarketDataGateway::processBookUpdate(const gateway::BookUpdate& update) {
  
  auto it = books_.find(update.instrumentId);
  if (it == books_.end()) {
    spdlog::warn("No order book found for instrument: {}", update.instrumentId);
    return;
  }
  auto& ob = it->second;

  if (update.isSnapshot) {
    // Snapshot: replace entire order book
    std::map<double,double> bidMap;
    std::map<double,double> askMap;
    for (const auto& [px, sz] : update.bids) bidMap[px] = sz;
    for (const auto& [px, sz] : update.asks) askMap[px] = sz;
    ob.applySnapshot(bidMap, askMap);
  } else {
    // Incremental update: size=0 means remove level, size>0 means update level
    for (const auto& [px, sz] : update.bids) {
      ob.applyDeltaBid(px, sz);
    }
    for (const auto& [px, sz] : update.asks) {
      ob.applyDeltaAsk(px, sz);
    }
  }

  auto [bbp, bbs] = ob.bestBid();
  auto [bap, bas] = ob.bestAsk();

  BestQuote q{update.instrumentId, bbp, bbs, bap, bas, timeutil::nowSteadyNanos() - update.receiveTimeNs};
  
  if (bestQuoteCallback_) {
    bestQuoteCallback_(q);
    
  } else {
    spdlog::warn("No callback set!");
  }
}

}  // namespace mdgw


