#include <doctest/doctest.h>

#include <memory>
#include <string>

#include "harness/tool_registry.h"

namespace {

class MockTool final : public harness::ITool {
 public:
  explicit MockTool(harness::ToolSchema schema) : schema_(std::move(schema)) {}

  harness::ToolSchema GetSchema() const override { return schema_; }
  harness::ToolResult Execute(const harness::Json& arguments) override {
    return {.success = true, .summary = "executed", .data = arguments};
  }
  void Cancel() override {}

 private:
  harness::ToolSchema schema_;
};

harness::ToolSchema Schema(std::string name, std::string category, int danger) {
  return {.name = std::move(name),
          .description = "Mock tool for registry tests",
          .parameters = {},
          .category = std::move(category),
          .danger_level = danger,
          .required_permissions = {}};
}

}  // namespace

TEST_CASE("tool registry registers, filters and sorts tools") {
  harness::ToolRegistry registry;
  for (int index = 9; index >= 0; --index) {
    registry.Register(std::make_unique<MockTool>(
        Schema("mock.tool_" + std::to_string(index), index % 2 == 0 ? "engine" : "asset",
               index % 3)));
  }

  CHECK(registry.Size() == 10);
  const auto safe_engine_tools = registry.Query("engine", 0);
  REQUIRE(safe_engine_tools.size() == 2);
  CHECK(safe_engine_tools[0].name == "mock.tool_0");
  CHECK(safe_engine_tools[1].name == "mock.tool_6");
}

TEST_CASE("tool registry rejects duplicate and invalid schemas") {
  harness::ToolRegistry registry;
  registry.Register(std::make_unique<MockTool>(Schema("mock.valid", "system", 0)));
  CHECK_THROWS_AS(
      registry.Register(std::make_unique<MockTool>(Schema("mock.valid", "system", 0))),
      std::invalid_argument);
  CHECK_THROWS_AS(
      registry.Register(std::make_unique<MockTool>(Schema("Not Namespaced", "system", 0))),
      std::invalid_argument);
  CHECK_THROWS_AS(
      registry.Register(std::make_unique<MockTool>(Schema("mock.dangerous", "system", 3))),
      std::invalid_argument);
}

TEST_CASE("tool registry executes known tools without exposing ownership") {
  harness::ToolRegistry registry;
  registry.Register(std::make_unique<MockTool>(Schema("mock.echo", "system", 0)));

  const auto result = registry.Execute("mock.echo", {{"value", 42}});
  CHECK(result.success);
  CHECK(result.data["value"] == 42);

  const auto missing = registry.Execute("mock.missing", harness::Json::object());
  CHECK_FALSE(missing.success);
  CHECK_FALSE(missing.error_detail.empty());
}
