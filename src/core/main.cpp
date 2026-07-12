#include <array>
#include <atomic>
#include <cstdio>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <string_view>

#include "core/runtime.h"
#include "harness/tool.h"
#include "ipc/frame_codec.h"
#include "ipc/json_rpc.h"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {

using harness::ipc::Json;

// ---------------------------------------------------------------------------
// Built-in read-only mock tool — the first tool wired through the full
// ExecutionEngine → PermissionManager chain.
// ---------------------------------------------------------------------------
class InspectTool final : public harness::ITool {
 public:
  harness::ToolSchema GetSchema() const override {
    return {
        .name = "harness.inspect",
        .description = "Read-only inspection of project metadata",
        .parameters =
            {
                {.name = "path",
                 .type = "string",
                 .description = "Relative path inside the project",
                 .required = true},
            },
        .category = "system",
        .danger_level = 0,
        .required_permissions =
            {
                {harness::PermissionScope::FileRead, "src"},
            },
    };
  }

  harness::ToolResult Execute(const harness::Json& arguments) override {
    return {.success = true,
            .summary = "Inspection completed",
            .data = {{"path", arguments.value("path", "")},
                     {"exists", true}}};
  }

  void Cancel() override {}
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
std::string GenerateInvocationId() {
  static std::mt19937_64 rng(std::random_device{}());
  static thread_local std::uniform_int_distribution<std::uint64_t> dist;
  const auto value = dist(rng);
  char buffer[17] = {};
  for (int i = 0; i < 16; ++i) {
    buffer[i] = "0123456789abcdef"[(value >> (i * 4)) & 0xf];
  }
  return std::string(buffer, 16);
}

Json BuildToolList(const core::HarnessRuntime& runtime) {
  auto schemas = runtime.Tools().Query();
  Json tools = Json::array();
  for (const auto& schema : schemas) {
    Json params = Json::array();
    for (const auto& param : schema.parameters) {
      Json entry{{"name", param.name},
                 {"type", param.type},
                 {"description", param.description},
                 {"required", param.required}};
      if (param.default_value.has_value()) {
        entry["default"] = *param.default_value;
      }
      params.push_back(std::move(entry));
    }
    tools.push_back({{"name", schema.name},
                     {"description", schema.description},
                     {"category", schema.category},
                     {"dangerLevel", schema.danger_level},
                     {"parameters", std::move(params)}});
  }
  return tools;
}

// ---------------------------------------------------------------------------
// Request router — wired to HarnessRuntime.
// ---------------------------------------------------------------------------
Json HandleRequest(core::HarnessRuntime& runtime, const harness::ipc::RpcMessage& message) {
  const auto& request = message.value;
  const auto& id = request["id"];
  const auto method = request["method"].get<std::string>();
  const auto state = runtime.State();

  // Reject everything after shutdown except shutdown itself.
  if (state == core::RuntimeState::ShuttingDown && method != "shutdown") {
    return harness::ipc::MakeErrorResponse(id, harness::ipc::kInvalidRequest,
                                           "Server is shutting down");
  }

  // Only initialize is allowed before Ready.
  if (state != core::RuntimeState::Ready && state != core::RuntimeState::ShuttingDown &&
      method != "initialize") {
    return harness::ipc::MakeErrorResponse(id, harness::ipc::kInvalidRequest,
                                           "Server is not initialized");
  }

  // ------------------------------------------------------------------
  // initialize
  // ------------------------------------------------------------------
  if (method == "initialize") {
    if (!request.contains("params") || !request["params"].is_object()) {
      return harness::ipc::MakeErrorResponse(id, harness::ipc::kInvalidParams,
                                             "Missing params");
    }
    const auto& params = request["params"];
    if (params.value("protocolVersion", "") != "1.0") {
      return harness::ipc::MakeErrorResponse(id, harness::ipc::kInvalidParams,
                                             "Unsupported or missing protocolVersion");
    }

    if (state == core::RuntimeState::Ready) {
      return harness::ipc::MakeErrorResponse(id, harness::ipc::kInvalidRequest,
                                             "Server is already initialized");
    }

    runtime.Initialize("1.0");
    return harness::ipc::MakeSuccessResponse(
        id, {{"protocolVersion", "1.0"},
             {"serverInfo", {{"name", "Harness Core"}, {"version", "0.2.0"}}},
             {"capabilities",
              {{"sandboxEngine", false},
               {"mcpClient", false},
               {"agentLoop", false},
               {"executionEngine", true}}}});
  }

  // ------------------------------------------------------------------
  // shutdown
  // ------------------------------------------------------------------
  if (method == "shutdown") {
    runtime.Shutdown();
    return harness::ipc::MakeSuccessResponse(id, nullptr);
  }

  // ------------------------------------------------------------------
  // tool/list
  // ------------------------------------------------------------------
  if (method == "tool/list") {
    return harness::ipc::MakeSuccessResponse(
        id, {{"tools", BuildToolList(runtime)}});
  }

  // ------------------------------------------------------------------
  // tool/call
  // ------------------------------------------------------------------
  if (method == "tool/call") {
    if (!request.contains("params") || !request["params"].is_object()) {
      return harness::ipc::MakeErrorResponse(id, harness::ipc::kInvalidParams,
                                             "Missing params");
    }
    const auto& params = request["params"];
    if (!params.contains("name") || !params["name"].is_string()) {
      return harness::ipc::MakeErrorResponse(id, harness::ipc::kInvalidParams,
                                             "Missing tool name");
    }
    const auto tool_name = params["name"].get<std::string>();
    const auto arguments = params.value("arguments", Json::object());

    harness::ExecutionInvocation invocation;
    invocation.invocation_id = GenerateInvocationId();
    invocation.tool_name = tool_name;
    invocation.arguments = arguments;

    auto result = runtime.Engine().Execute(invocation);

    Json response{{"invocationId", invocation.invocation_id},
                  {"success", result.success},
                  {"summary", result.summary}};
    if (!result.success) {
      response["errorDetail"] = result.error_detail;
    }
    if (!result.data.is_null()) {
      response["data"] = result.data;
    }
    if (!result.artifacts.empty()) {
      Json artifacts = Json::array();
      for (const auto& artifact : result.artifacts) {
        artifacts.push_back(artifact);
      }
      response["artifacts"] = std::move(artifacts);
    }
    return harness::ipc::MakeSuccessResponse(id, std::move(response));
  }

  return harness::ipc::MakeErrorResponse(id, harness::ipc::kMethodNotFound,
                                         "Method not found");
}

// ---------------------------------------------------------------------------
// Transport helpers
// ---------------------------------------------------------------------------
void Write(const Json& message) {
  const auto frame = harness::ipc::EncodeFrame(message.dump());
  std::cout.write(frame.data(), static_cast<std::streamsize>(frame.size()));
  std::cout.flush();
}

void ConfigureBinaryStdio() {
#ifdef _WIN32
  _setmode(_fileno(stdin), _O_BINARY);
  _setmode(_fileno(stdout), _O_BINARY);
#endif
}

std::ptrdiff_t ReadStdin(char* destination, std::size_t size) {
#ifdef _WIN32
  return _read(_fileno(stdin), destination,
               static_cast<unsigned int>(size));
#else
  return ::read(STDIN_FILENO, destination, size);
#endif
}

}  // namespace

// ===================================================================
int main() {
  ConfigureBinaryStdio();

  // Composition root — wires Registry → Permission → Execution.
  core::HarnessRuntime runtime(std::filesystem::current_path());

  // Register built-in read-only tools.
  runtime.Tools().Register(std::make_unique<InspectTool>());

  // Configure default permission rules.
  runtime.Permissions().AddRule(
      {"harness.*", harness::PermissionScope::FileRead, "src*",
       harness::PermissionDecision::Allow});

  harness::ipc::FrameDecoder decoder;
  std::array<char, 4096> chunk{};

  while (true) {
    const auto bytes_read = ReadStdin(chunk.data(), chunk.size());
    if (bytes_read <= 0) {
      break;
    }

    try {
      decoder.Feed(std::string_view(chunk.data(),
                                    static_cast<std::size_t>(bytes_read)));
      while (auto body = decoder.TryPop()) {
        try {
          auto message = harness::ipc::ParseMessage(*body);
          if (message.kind == harness::ipc::MessageKind::Request) {
            Write(HandleRequest(runtime, message));
          } else if (message.kind == harness::ipc::MessageKind::Notification &&
                     message.value["method"] == "exit") {
            return runtime.State() >= core::RuntimeState::ShuttingDown ? 0 : 1;
          }
        } catch (const harness::ipc::RpcProtocolError& error) {
          Write(harness::ipc::MakeErrorResponse(error.Id(), error.Code(),
                                                error.what()));
        }
      }
    } catch (const harness::ipc::FrameError& error) {
      std::cerr << "IPC frame error: " << error.what() << '\n';
      return 2;
    } catch (const std::exception& error) {
      Write(harness::ipc::MakeErrorResponse(nullptr,
                                            harness::ipc::kInternalError,
                                            "Internal error"));
      std::cerr << "Unhandled error: " << error.what() << '\n';
    }
  }
  return runtime.State() >= core::RuntimeState::ShuttingDown ? 0 : 1;
}
