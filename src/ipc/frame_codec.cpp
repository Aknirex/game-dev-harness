#include "ipc/frame_codec.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <limits>

namespace harness::ipc {
namespace {

constexpr std::string_view kHeaderSeparator = "\r\n\r\n";

std::string_view Trim(std::string_view value) {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
    value.remove_prefix(1);
  }
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
    value.remove_suffix(1);
  }
  return value;
}

bool EqualsIgnoreCase(std::string_view left, std::string_view right) {
  return left.size() == right.size() &&
         std::equal(left.begin(), left.end(), right.begin(), [](char a, char b) {
           return std::tolower(static_cast<unsigned char>(a)) ==
                  std::tolower(static_cast<unsigned char>(b));
         });
}

}  // namespace

std::string EncodeFrame(std::string_view body) {
  std::string frame;
  frame.reserve(body.size() + 128);
  frame += "Content-Length: ";
  frame += std::to_string(body.size());
  frame += "\r\nContent-Type: ";
  frame += kContentType;
  frame += kHeaderSeparator;
  frame.append(body);
  return frame;
}

FrameDecoder::FrameDecoder(std::size_t max_body_size)
    : max_body_size_(max_body_size) {
  if (max_body_size_ == 0) {
    throw std::invalid_argument("max_body_size must be positive");
  }
}

void FrameDecoder::Feed(std::string_view bytes) {
  if (bytes.size() > std::numeric_limits<std::size_t>::max() - buffer_.size()) {
    Fail("frame buffer size overflow");
  }
  buffer_.append(bytes);
  if (buffer_.find(kHeaderSeparator) == std::string::npos &&
      buffer_.size() > kMaxHeaderSize) {
    Fail("frame header exceeds size limit");
  }
}

std::optional<std::string> FrameDecoder::TryPop() {
  const auto header_end = buffer_.find(kHeaderSeparator);
  if (header_end == std::string::npos) {
    return std::nullopt;
  }
  if (header_end > kMaxHeaderSize) {
    Fail("frame header exceeds size limit");
  }

  std::optional<std::size_t> content_length;
  bool valid_content_type = false;
  std::size_t line_start = 0;
  while (line_start < header_end) {
    const auto line_end = buffer_.find("\r\n", line_start);
    const auto effective_end = std::min(line_end, header_end);
    const std::string_view line(buffer_.data() + line_start, effective_end - line_start);
    const auto colon = line.find(':');
    if (colon == std::string_view::npos) {
      Fail("malformed frame header");
    }

    const auto name = Trim(line.substr(0, colon));
    const auto value = Trim(line.substr(colon + 1));
    if (EqualsIgnoreCase(name, "Content-Length")) {
      if (content_length.has_value() || value.empty()) {
        Fail("invalid Content-Length header");
      }
      std::size_t parsed = 0;
      const auto [ptr, error] =
          std::from_chars(value.data(), value.data() + value.size(), parsed);
      if (error != std::errc{} || ptr != value.data() + value.size()) {
        Fail("invalid Content-Length header");
      }
      if (parsed > max_body_size_) {
        Fail("frame body exceeds size limit");
      }
      content_length = parsed;
    } else if (EqualsIgnoreCase(name, "Content-Type")) {
      valid_content_type = EqualsIgnoreCase(value, kContentType);
    }

    line_start = effective_end + 2;
  }

  if (!content_length.has_value()) {
    Fail("missing Content-Length header");
  }
  if (!valid_content_type) {
    Fail("missing or unsupported Content-Type header");
  }

  const std::size_t body_start = header_end + kHeaderSeparator.size();
  if (*content_length > std::numeric_limits<std::size_t>::max() - body_start) {
    Fail("frame size overflow");
  }
  const std::size_t frame_size = body_start + *content_length;
  if (buffer_.size() < frame_size) {
    return std::nullopt;
  }

  std::string body = buffer_.substr(body_start, *content_length);
  buffer_.erase(0, frame_size);
  return body;
}

std::size_t FrameDecoder::BufferedBytes() const noexcept { return buffer_.size(); }

[[noreturn]] void FrameDecoder::Fail(std::string_view message) {
  buffer_.clear();
  throw FrameError(std::string(message));
}

}  // namespace harness::ipc
