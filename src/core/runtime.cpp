#include "core/runtime.h"

#include <stdexcept>
#include <utility>

namespace core {

HarnessRuntime::HarnessRuntime(std::filesystem::path project_root)
    : permissions_(std::move(project_root)),
      engine_(tools_, permissions_) {}

void HarnessRuntime::Initialize(std::string_view protocol_version) {
  if (state_ != RuntimeState::Created) {
    throw std::logic_error("HarnessRuntime: already initialized");
  }
  if (protocol_version != "1.0") {
    throw std::invalid_argument("Unsupported protocolVersion");
  }
  state_ = RuntimeState::Ready;
}

void HarnessRuntime::Shutdown() {
  state_ = RuntimeState::ShuttingDown;
}

RuntimeState HarnessRuntime::State() const {
  return state_;
}

bool HarnessRuntime::IsReady() const {
  return state_ == RuntimeState::Ready;
}

harness::ToolRegistry& HarnessRuntime::Tools() {
  return tools_;
}

harness::PermissionManager& HarnessRuntime::Permissions() {
  return permissions_;
}

harness::ExecutionEngine& HarnessRuntime::Engine() {
  return engine_;
}

const harness::ToolRegistry& HarnessRuntime::Tools() const {
  return tools_;
}

const harness::PermissionManager& HarnessRuntime::Permissions() const {
  return permissions_;
}

}  // namespace core
