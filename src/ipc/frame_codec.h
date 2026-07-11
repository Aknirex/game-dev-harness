#pragma once

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace harness::ipc {

inline constexpr std::string_view kContentType =
    "application/vnd.harness+json; charset=utf-8";
inline constexpr std::size_t kDefaultMaxBodySize = 16 * 1024 * 1024;
inline constexpr std::size_t kMaxHeaderSize = 8 * 1024;

class FrameError final : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

[[nodiscard]] std::string EncodeFrame(std::string_view body);

class FrameDecoder final {
 public:
  explicit FrameDecoder(std::size_t max_body_size = kDefaultMaxBodySize);

  void Feed(std::string_view bytes);
  [[nodiscard]] std::optional<std::string> TryPop();
  [[nodiscard]] std::size_t BufferedBytes() const noexcept;

 private:
  [[noreturn]] void Fail(std::string_view message);

  std::string buffer_;
  std::size_t max_body_size_;
};

}  // namespace harness::ipc
