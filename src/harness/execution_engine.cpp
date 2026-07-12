#include "harness/execution_engine.h"

#include <sstream>
#include <stdexcept>

namespace harness {

ExecutionEngine::ExecutionEngine(ToolRegistry& registry,
                                 PermissionManager& permissions)
    : registry_(registry), permissions_(permissions) {}

ToolResult ExecutionEngine::Execute(const ExecutionInvocation& invocation) {
  // 1. Look up the frozen descriptor.
  const auto descriptor = registry_.GetDescriptor(invocation.tool_name);
  if (!descriptor) {
    return {.success = false,
            .summary = "Tool not found",
            .error_detail =
                "No tool is registered with name: " + invocation.tool_name};
  }

  // 2. Check every required permission.
  //    Each permission gets its own sub-id so multiple checks per
  //    invocation are independently tracked.
  for (std::size_t index = 0; index < descriptor->required_permissions.size();
       ++index) {
    const auto& requirement = descriptor->required_permissions[index];

    PermissionRequest perm_req;
    perm_req.invocation_id =
        invocation.invocation_id + "::" + std::to_string(index);
    perm_req.tool_name = invocation.tool_name;
    perm_req.scope = requirement.scope;
    perm_req.resource = requirement.resource_pattern;

    PermissionCheckResult result;
    try {
      result = permissions_.Check(perm_req);
    } catch (const std::exception& ex) {
      // Normalization failure → deny the whole invocation.
      permissions_.Cancel(invocation.invocation_id);
      return {.success = false,
              .summary = "Permission check failed",
              .error_detail = ex.what()};
    }

    if (result.decision == PermissionDecision::Deny) {
      permissions_.Cancel(invocation.invocation_id);
      std::ostringstream oss;
      oss << "Permission denied: " << requirement.resource_pattern;
      return {.success = false,
              .summary = "Permission denied",
              .error_detail = oss.str()};
    }

    if (result.decision == PermissionDecision::Ask) {
      permissions_.Cancel(invocation.invocation_id);
      return {.success = false,
              .summary = "Permission requires user approval",
              .error_detail =
                  "Awaiting user approval for: " + requirement.resource_pattern};
    }
  }

  // 3. All permissions passed — execute the tool.
  ITool* tool = registry_.Find(invocation.tool_name);
  if (!tool) {
    return {.success = false,
            .summary = "Tool not found",
            .error_detail =
                "Tool disappeared during permission check: " +
                invocation.tool_name};
  }

  try {
    return tool->Execute(invocation.arguments);
  } catch (const std::exception& ex) {
    return {.success = false,
            .summary = "Tool execution failed",
            .error_detail = ex.what()};
  }
}

}  // namespace harness
