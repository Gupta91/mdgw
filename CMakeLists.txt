cmake_minimum_required(VERSION 3.20)
project(mdgw VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(FetchContent)

# Dependencies
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.12.0
)
FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(
  rapidjson
  GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
  GIT_TAG v1.1.0
)
set(RAPIDJSON_BUILD_DOC OFF CACHE BOOL "Disable rapidjson docs" FORCE)
set(RAPIDJSON_BUILD_TESTS OFF CACHE BOOL "Disable rapidjson tests" FORCE)
set(RAPIDJSON_BUILD_EXAMPLES OFF CACHE BOOL "Disable rapidjson examples" FORCE)
FetchContent_MakeAvailable(rapidjson)

add_executable(mdgw
  src/main.cpp
  src/gateway/MarketDataGateway.hpp
  src/gateway/OkxMarketDataGateway.cpp
  src/gateway/OkxMarketDataGateway.hpp
  src/book/OrderBook.cpp
  src/book/OrderBook.hpp
  src/metrics/Metrics.cpp
  src/metrics/Metrics.hpp
  src/util/Log.hpp
  src/util/Time.hpp
  src/util/Json.hpp
)

target_include_directories(mdgw PRIVATE
  ${rapidjson_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(mdgw PRIVATE
  OpenSSL::SSL
  OpenSSL::Crypto
  ZLIB::ZLIB
  spdlog::spdlog_header_only
)

if(APPLE)
  target_compile_definitions(mdgw PRIVATE _DARWIN_C_SOURCE)
endif()

target_compile_options(mdgw PRIVATE
  -Wall -Wextra -Wpedantic -Wconversion -Wno-sign-conversion
)

# Use Boost.System header-only to avoid linking libboost_system
target_compile_definitions(mdgw PRIVATE BOOST_ERROR_CODE_HEADER_ONLY)


# -------------------------
# Tests (Catch2)
# -------------------------
option(MDgw_BUILD_TESTS "Build mdgw tests" ON)
if(MDgw_BUILD_TESTS)
  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.3
  )
  FetchContent_MakeAvailable(Catch2)

  enable_testing()
  add_executable(mdgw_tests
    tests/test_orderbook.cpp
    tests/test_ringbuffer.cpp
    src/book/OrderBook.cpp
  )
  target_include_directories(mdgw_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(mdgw_tests PRIVATE Catch2::Catch2WithMain)
  add_test(NAME mdgw_tests COMMAND mdgw_tests)
endif()


