#pragma once

#include <string>
#include <vector>

#include "harness/tool.h"

namespace harness {

/// Frozen snapshot of a tool's metadata, taken at registration time.
/// Once stored, this never changes — all queries, permissions checks,
/// and UI rendering use this immutable copy instead of calling ITool::GetSchema().
struct ToolDescriptor {
  std::string name;
  std::string description;
  std::string category;
  int danger_level = 0;
  std::vector<PermissionRequirement> required_permissions;
  std::vector<ToolParameter> parameters;

  static ToolDescriptor Freeze(const ToolSchema& schema);
};

}  // namespace harness
