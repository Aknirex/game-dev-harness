#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "harness/tool.h"
#include "harness/tool_descriptor.h"

namespace harness {

class ExecutionEngine;

class ToolRegistry final {
 public:
  void Register(std::unique_ptr<ITool> tool);

  [[nodiscard]] std::vector<ToolSchema> Query(
      std::optional<std::string_view> category = std::nullopt,
      int max_danger_level = 2) const;

  /// Returns the frozen descriptor that was snapshotted at registration time.
  /// This is the preferred way to inspect tool metadata — the descriptor
  /// never changes and does not call back into plugin code.
  [[nodiscard]] std::optional<ToolDescriptor> GetDescriptor(
      std::string_view name) const;

  /// Returns a raw pointer to the tool instance.  Only ExecutionEngine
  /// should call Execute() on the returned pointer; all other callers must
  /// go through the ExecutionEngine enforcement chain.
  [[nodiscard]] ITool* Find(std::string_view name);

  [[nodiscard]] std::size_t Size() const;

 private:
  friend class ExecutionEngine;

  static void ValidateSchema(const ToolSchema& schema);

  mutable std::mutex mutex_;
  std::unordered_map<std::string, std::unique_ptr<ITool>> tools_;
  std::unordered_map<std::string, ToolDescriptor> descriptors_;
};

}  // namespace harness
