#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "harness/tool.h"

namespace harness {

class ToolRegistry final {
 public:
  void Register(std::unique_ptr<ITool> tool);
  [[nodiscard]] std::vector<ToolSchema> Query(
      std::optional<std::string_view> category = std::nullopt,
      int max_danger_level = 2) const;
  [[nodiscard]] const ITool* Find(std::string_view name) const;
  [[nodiscard]] ToolResult Execute(std::string_view name, const Json& arguments);
  [[nodiscard]] std::size_t Size() const;

 private:
  static void ValidateSchema(const ToolSchema& schema);

  mutable std::mutex mutex_;
  std::unordered_map<std::string, std::unique_ptr<ITool>> tools_;
};

}  // namespace harness
