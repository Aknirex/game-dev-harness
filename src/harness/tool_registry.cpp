#include "harness/tool_registry.h"

#include <algorithm>
#include <regex>
#include <stdexcept>
#include <unordered_set>

namespace harness {
namespace {

const std::regex kToolNamePattern(
    R"(^[a-z][a-z0-9_]*(\.[a-z][a-z0-9_-]*)+$)",
    std::regex::ECMAScript | std::regex::optimize);
const std::regex kParameterNamePattern(R"(^[A-Za-z_][A-Za-z0-9_]*$)",
                                       std::regex::ECMAScript | std::regex::optimize);

}  // namespace

void ToolRegistry::Register(std::unique_ptr<ITool> tool) {
  if (!tool) {
    throw std::invalid_argument("tool must not be null");
  }
  ToolSchema schema = tool->GetSchema();
  ValidateSchema(schema);

  std::scoped_lock lock(mutex_);
  if (tools_.contains(schema.name)) {
    throw std::invalid_argument("tool already registered: " + schema.name);
  }
  tools_.emplace(std::move(schema.name), std::move(tool));
}

std::vector<ToolSchema> ToolRegistry::Query(std::optional<std::string_view> category,
                                            int max_danger_level) const {
  std::vector<ToolSchema> schemas;
  std::scoped_lock lock(mutex_);
  schemas.reserve(tools_.size());
  for (const auto& [name, tool] : tools_) {
    auto schema = tool->GetSchema();
    if ((!category.has_value() || schema.category == *category) &&
        schema.danger_level <= max_danger_level) {
      schemas.push_back(std::move(schema));
    }
  }
  std::ranges::sort(schemas, {}, &ToolSchema::name);
  return schemas;
}

const ITool* ToolRegistry::Find(std::string_view name) const {
  std::scoped_lock lock(mutex_);
  const auto iterator = tools_.find(std::string(name));
  return iterator == tools_.end() ? nullptr : iterator->second.get();
}

ToolResult ToolRegistry::Execute(std::string_view name, const Json& arguments) {
  ITool* tool = nullptr;
  {
    std::scoped_lock lock(mutex_);
    const auto iterator = tools_.find(std::string(name));
    if (iterator == tools_.end()) {
      return {.success = false,
              .summary = "Tool not found",
              .error_detail = "No tool is registered with name: " + std::string(name)};
    }
    tool = iterator->second.get();
  }
  return tool->Execute(arguments);
}

std::size_t ToolRegistry::Size() const {
  std::scoped_lock lock(mutex_);
  return tools_.size();
}

void ToolRegistry::ValidateSchema(const ToolSchema& schema) {
  if (!std::regex_match(schema.name, kToolNamePattern)) {
    throw std::invalid_argument("tool name must be a namespaced lowercase identifier");
  }
  if (schema.description.empty()) {
    throw std::invalid_argument("tool description must not be empty");
  }
  if (schema.category.empty()) {
    throw std::invalid_argument("tool category must not be empty");
  }
  if (schema.danger_level < 0 || schema.danger_level > 2) {
    throw std::invalid_argument("tool danger_level must be between 0 and 2");
  }

  std::unordered_set<std::string> parameter_names;
  for (const auto& parameter : schema.parameters) {
    if (!std::regex_match(parameter.name, kParameterNamePattern) ||
        parameter.type.empty() || parameter.description.empty()) {
      throw std::invalid_argument("tool parameter schema is invalid");
    }
    if (!parameter_names.emplace(parameter.name).second) {
      throw std::invalid_argument("tool parameter names must be unique");
    }
    if (parameter.required && parameter.default_value.has_value()) {
      throw std::invalid_argument("required parameters cannot define a default value");
    }
  }

  for (const auto& permission : schema.required_permissions) {
    if (permission.resource_pattern.empty()) {
      throw std::invalid_argument("permission resource pattern must not be empty");
    }
  }
}

}  // namespace harness
