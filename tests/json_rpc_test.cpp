#include <doctest/doctest.h>

#include "ipc/json_rpc.h"

using namespace harness::ipc;

TEST_CASE("JSON-RPC parser distinguishes message kinds") {
  CHECK(ParseMessage(R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})").kind ==
        MessageKind::Request);
  CHECK(ParseMessage(R"({"jsonrpc":"2.0","method":"initialized"})").kind ==
        MessageKind::Notification);
  CHECK(ParseMessage(R"({"jsonrpc":"2.0","id":"a","result":{}})").kind ==
        MessageKind::SuccessResponse);
  CHECK(ParseMessage(
            R"({"jsonrpc":"2.0","id":1,"error":{"code":-32601,"message":"missing"}})")
            .kind == MessageKind::ErrorResponse);
}

TEST_CASE("JSON-RPC parser returns standard errors") {
  try {
    static_cast<void>(ParseMessage("{"));
    FAIL("expected parse error");
  } catch (const RpcProtocolError& error) {
    CHECK(error.Code() == kParseError);
    CHECK(error.Id().is_null());
  }

  try {
    static_cast<void>(ParseMessage(R"({"jsonrpc":"1.0","id":7,"method":"x"})"));
    FAIL("expected invalid request");
  } catch (const RpcProtocolError& error) {
    CHECK(error.Code() == kInvalidRequest);
    CHECK(error.Id() == 7);
  }
}

TEST_CASE("JSON-RPC response builders preserve string IDs") {
  const auto response = MakeSuccessResponse("request-a", {{"accepted", true}});
  CHECK(response["jsonrpc"] == "2.0");
  CHECK(response["id"] == "request-a");
  CHECK(response["result"]["accepted"] == true);

  const auto error = MakeErrorResponse(nullptr, kMethodNotFound, "Method not found");
  CHECK(error["error"]["code"] == kMethodNotFound);
  CHECK_FALSE(error["error"].contains("data"));
}
