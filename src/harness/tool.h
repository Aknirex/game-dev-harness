#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "common/json.h"

namespace harness {

enum class PermissionScope {
  FileRead,
  FileWrite,
  ShellExecute,
  NetworkCall,
  EngineModify,
};

struct PermissionRequirement {
  PermissionScope scope;
  std::string resource_pattern;
};

struct ToolParameter {
  std::string name;
  std::string type;
  std::string description;
  bool required = false;
  std::optional<Json> default_value;
};

struct ToolSchema {
  std::string name;
  std::string description;
  std::vector<ToolParameter> parameters;
  std::string category;
  int danger_level = 0;
  std::vector<PermissionRequirement> required_permissions;
};

struct ToolResult {
  bool success = false;
  std::string summary;
  Json data = Json::object();
  std::vector<std::string> artifacts;
  std::string error_detail;
};

class ITool {
 public:
  virtual ~ITool() = default;
  [[nodiscard]] virtual ToolSchema GetSchema() const = 0;
  virtual ToolResult Execute(const Json& arguments) = 0;
  virtual void Cancel() = 0;
};

}  // namespace harness
