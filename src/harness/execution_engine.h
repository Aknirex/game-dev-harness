#pragma once

#include <string>

#include "harness/permission_manager.h"
#include "harness/tool_registry.h"

namespace harness {

struct ExecutionInvocation {
  std::string invocation_id;
  std::string tool_name;
  Json arguments;
};

/// The single enforcement point for every tool execution.
///
/// Execution order is fixed:
///   1. Look up the frozen ToolDescriptor from the registry.
///   2. Check every required permission against the PermissionManager —
///      each permission gets its own sub-id under the invocation_id so
///      multiple permissions per tool are independently tracked.
///   3. If any permission is Deny or Ask, the tool is NOT executed.
///   4. Execute the tool handler (only after all permissions pass).
///   5. Return the ToolResult.
class ExecutionEngine final {
 public:
  ExecutionEngine(ToolRegistry& registry, PermissionManager& permissions);

  [[nodiscard]] ToolResult Execute(const ExecutionInvocation& invocation);

 private:
  ToolRegistry& registry_;
  PermissionManager& permissions_;
};

}  // namespace harness
