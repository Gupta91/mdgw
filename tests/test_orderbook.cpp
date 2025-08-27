#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "book/OrderBook.hpp"

using mdgw::book::OrderBook;

TEST_CASE("OrderBook snapshot and incremental updates", "[orderbook]") {
  OrderBook ob{"BTC-USDT-SWAP"};

  // Initial snapshot
  std::map<double, double> bids{{30000.0, 1.5}, {29999.5, 2.0}};
  std::map<double, double> asks{{30000.5, 1.2}, {30001.0, 3.0}};
  ob.applySnapshot(bids, asks);

  auto [bbp, bbs] = ob.bestBid();
  auto [bap, bas] = ob.bestAsk();
  REQUIRE(bbp == Catch::Approx(30000.0));
  REQUIRE(bbs == Catch::Approx(1.5));
  REQUIRE(bap == Catch::Approx(30000.5));
  REQUIRE(bas == Catch::Approx(1.2));
  REQUIRE(ob.bidLevels() == 2);
  REQUIRE(ob.askLevels() == 2);

  // Incremental updates
  ob.applyDeltaBid(30000.0, 2.5); // increase best bid size
  ob.applyDeltaAsk(30000.5, 0.0); // remove best ask

  std::tie(bbp, bbs) = ob.bestBid();
  std::tie(bap, bas) = ob.bestAsk();
  REQUIRE(bbp == Catch::Approx(30000.0));
  REQUIRE(bbs == Catch::Approx(2.5));
  REQUIRE(bap == Catch::Approx(30001.0));
  REQUIRE(bas == Catch::Approx(3.0));
  REQUIRE(ob.bidLevels() == 2);
  REQUIRE(ob.askLevels() == 1);

  // Insert new better ask
  ob.applyDeltaAsk(30000.25, 4.2);
  std::tie(bap, bas) = ob.bestAsk();
  REQUIRE(bap == Catch::Approx(30000.25));
  REQUIRE(bas == Catch::Approx(4.2));

  // Remove bid level
  ob.applyDeltaBid(29999.5, 0.0);
  REQUIRE(ob.bidLevels() == 1);
}


