#pragma once

#include <iostream>
#include <memory>
#include <string>

namespace udb {
template <typename... Args>
std::string FormatString(const std::string &format, Args... args) {
  size_t size = 1 + snprintf(nullptr, 0, format.c_str(), args...);
  char bytes[size];
  snprintf(bytes, size, format.c_str(), args...);
  return string(bytes);
}

} // namespace udb