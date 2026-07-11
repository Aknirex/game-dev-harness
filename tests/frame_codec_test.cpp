#include <doctest/doctest.h>

#include "ipc/frame_codec.h"

using harness::ipc::EncodeFrame;
using harness::ipc::FrameDecoder;
using harness::ipc::FrameError;

TEST_CASE("frame decoder handles fragmented UTF-8 bodies") {
  const std::string body = R"({"message":"你好, Harness"})";
  const std::string frame = EncodeFrame(body);
  FrameDecoder decoder;

  for (const char byte : frame) {
    decoder.Feed(std::string_view(&byte, 1));
  }

  const auto decoded = decoder.TryPop();
  REQUIRE(decoded.has_value());
  CHECK(*decoded == body);
  CHECK(decoder.BufferedBytes() == 0);
}

TEST_CASE("frame decoder emits multiple buffered frames") {
  FrameDecoder decoder;
  decoder.Feed(EncodeFrame("{}") + EncodeFrame(R"({"id":2})"));

  CHECK(decoder.TryPop() == std::optional<std::string>("{}"));
  CHECK(decoder.TryPop() == std::optional<std::string>(R"({"id":2})"));
  CHECK_FALSE(decoder.TryPop().has_value());
}

TEST_CASE("frame decoder rejects invalid headers") {
  FrameDecoder decoder;
  decoder.Feed("Content-Length: -1\r\nContent-Type: application/vnd.harness+json; charset=utf-8\r\n\r\n");
  CHECK_THROWS_AS(static_cast<void>(decoder.TryPop()), FrameError);

  decoder.Feed("Content-Length: 2\r\n\r\n{}");
  CHECK_THROWS_AS(static_cast<void>(decoder.TryPop()), FrameError);
}

TEST_CASE("frame decoder enforces body limit") {
  FrameDecoder decoder(4);
  decoder.Feed(EncodeFrame("12345"));
  CHECK_THROWS_AS(static_cast<void>(decoder.TryPop()), FrameError);
}
