#pragma once

#include <expected>
#include <string>

namespace proc_monitor {

struct Error {
  int code;
  std::string message;
};

template <typename T>
using Result = std::expected<T, Error>;

} // namespace proc_monitor
