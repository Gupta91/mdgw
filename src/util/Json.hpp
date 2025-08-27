#pragma once

#include <rapidjson/document.h>
#include <string>

namespace mdgw::json {

inline rapidjson::Document parse(const std::string& text) {
  rapidjson::Document d;
  d.Parse(text.c_str());
  return d;
}

}


