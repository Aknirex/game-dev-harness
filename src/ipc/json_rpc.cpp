#include "ipc/json_rpc.h"

#include <utility>

namespace harness::ipc {
namespace {

bool IsValidId(const Json& id, bool allow_null) {
  return (allow_null && id.is_null()) || id.is_string() ||
         id.is_number_integer() || id.is_number_unsigned();
}

void ValidateParams(const Json& value, const Json& id) {
  if (value.contains("params") && !value["params"].is_object() &&
      !value["params"].is_array()) {
    throw RpcProtocolError(kInvalidRequest, "Invalid Request", id);
  }
}

}  // namespace

RpcProtocolError::RpcProtocolError(int code, std::string message, Json id)
    : std::runtime_error(std::move(message)), code_(code), id_(std::move(id)) {}

int RpcProtocolError::Code() const noexcept { return code_; }

const Json& RpcProtocolError::Id() const noexcept { return id_; }

RpcMessage ParseMessage(std::string_view body) {
  Json value;
  try {
    value = Json::parse(body);
  } catch (const Json::parse_error&) {
    throw RpcProtocolError(kParseError, "Parse error");
  }

  Json response_id = nullptr;
  if (value.is_object() && value.contains("id") && IsValidId(value["id"], true)) {
    response_id = value["id"];
  }
  if (!value.is_object() || !value.contains("jsonrpc") ||
      value["jsonrpc"] != "2.0") {
    throw RpcProtocolError(kInvalidRequest, "Invalid Request", response_id);
  }

  const bool has_method = value.contains("method");
  const bool has_id = value.contains("id");
  const bool has_result = value.contains("result");
  const bool has_error = value.contains("error");

  if (has_method) {
    if (!value["method"].is_string() || value["method"].get_ref<const std::string&>().empty() ||
        has_result || has_error) {
      throw RpcProtocolError(kInvalidRequest, "Invalid Request", response_id);
    }
    ValidateParams(value, response_id);
    if (!has_id) {
      return {MessageKind::Notification, std::move(value)};
    }
    if (!IsValidId(value["id"], false)) {
      throw RpcProtocolError(kInvalidRequest, "Invalid Request");
    }
    return {MessageKind::Request, std::move(value)};
  }

  if (!has_id || !IsValidId(value["id"], true) || has_result == has_error) {
    throw RpcProtocolError(kInvalidRequest, "Invalid Request", response_id);
  }
  if (has_error) {
    const auto& error = value["error"];
    if (!error.is_object() || !error.contains("code") || !error["code"].is_number_integer() ||
        !error.contains("message") || !error["message"].is_string()) {
      throw RpcProtocolError(kInvalidRequest, "Invalid Request", response_id);
    }
    return {MessageKind::ErrorResponse, std::move(value)};
  }
  return {MessageKind::SuccessResponse, std::move(value)};
}

Json MakeRequest(Json id, std::string method, Json params) {
  return Json{{"jsonrpc", "2.0"}, {"id", std::move(id)},
              {"method", std::move(method)}, {"params", std::move(params)}};
}

Json MakeNotification(std::string method, Json params) {
  return Json{{"jsonrpc", "2.0"}, {"method", std::move(method)},
              {"params", std::move(params)}};
}

Json MakeSuccessResponse(Json id, Json result) {
  return Json{{"jsonrpc", "2.0"}, {"id", std::move(id)},
              {"result", std::move(result)}};
}

Json MakeErrorResponse(Json id, int code, std::string message, Json data) {
  Json error{{"code", code}, {"message", std::move(message)}};
  if (!data.is_null()) {
    error["data"] = std::move(data);
  }
  return Json{{"jsonrpc", "2.0"}, {"id", std::move(id)}, {"error", std::move(error)}};
}

}  // namespace harness::ipc
