#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

#include "common/json.h"

namespace harness::ipc {

using Json = harness::Json;

enum class MessageKind { Request, Notification, SuccessResponse, ErrorResponse };

struct RpcMessage {
  MessageKind kind;
  Json value;
};

class RpcProtocolError final : public std::runtime_error {
 public:
  RpcProtocolError(int code, std::string message, Json id = nullptr);

  [[nodiscard]] int Code() const noexcept;
  [[nodiscard]] const Json& Id() const noexcept;

 private:
  int code_;
  Json id_;
};

inline constexpr int kParseError = -32700;
inline constexpr int kInvalidRequest = -32600;
inline constexpr int kMethodNotFound = -32601;
inline constexpr int kInvalidParams = -32602;
inline constexpr int kInternalError = -32603;

[[nodiscard]] RpcMessage ParseMessage(std::string_view body);
[[nodiscard]] Json MakeRequest(Json id, std::string method, Json params = Json::object());
[[nodiscard]] Json MakeNotification(std::string method, Json params = Json::object());
[[nodiscard]] Json MakeSuccessResponse(Json id, Json result);
[[nodiscard]] Json MakeErrorResponse(Json id, int code, std::string message,
                                     Json data = nullptr);

}  // namespace harness::ipc
