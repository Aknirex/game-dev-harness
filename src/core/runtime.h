#pragma once

#include <filesystem>
#include <memory>

#include "harness/execution_engine.h"
#include "harness/permission_manager.h"
#include "harness/tool_registry.h"

namespace core {

enum class RuntimeState {
  Created,
  Initialized,
  Ready,
  ShuttingDown,
  Exited,
};

/// Composition root that wires together all Harness subsystems.
///
/// Owns the ToolRegistry, PermissionManager, and ExecutionEngine
/// and enforces the lifecycle state machine:
///   Created → Initialized → Ready → ShuttingDown → Exited
class HarnessRuntime final {
 public:
  explicit HarnessRuntime(std::filesystem::path project_root);

  // ---- lifecycle ----
  void Initialize(std::string_view protocol_version);
  void Shutdown();
  [[nodiscard]] RuntimeState State() const;
  [[nodiscard]] bool IsReady() const;

  // ---- accessors ----
  harness::ToolRegistry& Tools();
  harness::PermissionManager& Permissions();
  harness::ExecutionEngine& Engine();
  [[nodiscard]] const harness::ToolRegistry& Tools() const;
  [[nodiscard]] const harness::PermissionManager& Permissions() const;

 private:
  RuntimeState state_ = RuntimeState::Created;
  harness::ToolRegistry tools_;
  harness::PermissionManager permissions_;
  harness::ExecutionEngine engine_;
};

}  // namespace core
