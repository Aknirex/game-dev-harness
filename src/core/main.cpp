#include <array>
#include <cstdio>
#include <iostream>
#include <string>

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

Json HandleRequest(const harness::ipc::RpcMessage& message, bool& shutting_down) {
  const auto& request = message.value;
  const auto& id = request["id"];
  const auto method = request["method"].get<std::string>();

  if (shutting_down && method != "shutdown") {
    return harness::ipc::MakeErrorResponse(id, harness::ipc::kInvalidRequest,
                                           "Server is shutting down");
  }

  if (method == "initialize") {
    if (!request.contains("params") || !request["params"].is_object() ||
        request["params"].value("protocolVersion", "") != "1.0") {
      return harness::ipc::MakeErrorResponse(id, harness::ipc::kInvalidParams,
                                             "Unsupported or missing protocolVersion");
    }
    return harness::ipc::MakeSuccessResponse(
        id, {{"protocolVersion", "1.0"},
             {"serverInfo", {{"name", "Harness Core"}, {"version", "0.1.0"}}},
             {"capabilities",
              {{"sandboxEngine", false}, {"mcpClient", false}, {"agentLoop", false}}}});
  }
  if (method == "tool/list") {
    return harness::ipc::MakeSuccessResponse(id, {{"tools", Json::array()}});
  }
  if (method == "shutdown") {
    shutting_down = true;
    return harness::ipc::MakeSuccessResponse(id, nullptr);
  }
  return harness::ipc::MakeErrorResponse(id, harness::ipc::kMethodNotFound,
                                         "Method not found");
}

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
  return _read(_fileno(stdin), destination, static_cast<unsigned int>(size));
#else
  return ::read(STDIN_FILENO, destination, size);
#endif
}

}  // namespace

int main() {
  ConfigureBinaryStdio();
  harness::ipc::FrameDecoder decoder;
  std::array<char, 4096> chunk{};
  bool shutting_down = false;

  while (true) {
    const auto bytes_read = ReadStdin(chunk.data(), chunk.size());
    if (bytes_read <= 0) {
      break;
    }

    try {
      decoder.Feed(std::string_view(chunk.data(), static_cast<std::size_t>(bytes_read)));
      while (auto body = decoder.TryPop()) {
        try {
          auto message = harness::ipc::ParseMessage(*body);
          if (message.kind == harness::ipc::MessageKind::Request) {
            Write(HandleRequest(message, shutting_down));
          } else if (message.kind == harness::ipc::MessageKind::Notification &&
                     message.value["method"] == "exit") {
            return shutting_down ? 0 : 1;
          }
        } catch (const harness::ipc::RpcProtocolError& error) {
          Write(harness::ipc::MakeErrorResponse(error.Id(), error.Code(), error.what()));
        }
      }
    } catch (const harness::ipc::FrameError& error) {
      std::cerr << "IPC frame error: " << error.what() << '\n';
      return 2;
    } catch (const std::exception& error) {
      Write(harness::ipc::MakeErrorResponse(nullptr, harness::ipc::kInternalError,
                                            "Internal error"));
      std::cerr << "Unhandled error: " << error.what() << '\n';
    }
  }
  return shutting_down ? 0 : 1;
}
