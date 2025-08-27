#pragma once

#include <spdlog/spdlog.h>

namespace mdgw::log {

inline void init() {
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
  spdlog::set_level(spdlog::level::info);
}

}


