#include <doctest/doctest.h>

#include <memory>
#include <string>

#include "harness/tool_registry.h"
#include "harness/tool_descriptor.h"

namespace {

class MockTool final : public harness::ITool {
 public:
  explicit MockTool(harness::ToolSchema schema)
      : schema_(std::move(schema)) {}

  harness::ToolSchema GetSchema() const override { return schema_; }
  harness::ToolResult Execute(const harness::Json& arguments) override {
    ++execute_count;
    return {.success = true, .summary = "executed", .data = arguments};
  }
  void Cancel() override { ++cancel_count; }

  harness::ToolSchema schema_;
  mutable int execute_count = 0;
  mutable int cancel_count = 0;
};

harness::ToolSchema Schema(std::string name, std::string category,
                           int danger) {
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
        Schema("mock.tool_" + std::to_string(index),
               index % 2 == 0 ? "engine" : "asset", index % 3)));
  }

  CHECK(registry.Size() == 10);
  const auto safe_engine_tools = registry.Query("engine", 0);
  REQUIRE(safe_engine_tools.size() == 2);
  CHECK(safe_engine_tools[0].name == "mock.tool_0");
  CHECK(safe_engine_tools[1].name == "mock.tool_6");
}

TEST_CASE("tool registry rejects duplicate and invalid schemas") {
  harness::ToolRegistry registry;
  registry.Register(
      std::make_unique<MockTool>(Schema("mock.valid", "system", 0)));
  CHECK_THROWS_AS(
      registry.Register(
          std::make_unique<MockTool>(Schema("mock.valid", "system", 0))),
      std::invalid_argument);
  CHECK_THROWS_AS(
      registry.Register(std::make_unique<MockTool>(
          Schema("Not Namespaced", "system", 0))),
      std::invalid_argument);
  CHECK_THROWS_AS(
      registry.Register(std::make_unique<MockTool>(
          Schema("mock.dangerous", "system", 3))),
      std::invalid_argument);
}

TEST_CASE("GetDescriptor returns a frozen snapshot") {
  harness::ToolRegistry registry;
  registry.Register(
      std::make_unique<MockTool>(Schema("mock.echo", "system", 0)));

  const auto descriptor = registry.GetDescriptor("mock.echo");
  REQUIRE(descriptor.has_value());
  CHECK(descriptor->name == "mock.echo");
  CHECK(descriptor->category == "system");
  CHECK(descriptor->danger_level == 0);

  const auto missing = registry.GetDescriptor("mock.nonexistent");
  CHECK_FALSE(missing.has_value());
}

TEST_CASE("Find returns a valid pointer and null for missing tools") {
  harness::ToolRegistry registry;
  registry.Register(
      std::make_unique<MockTool>(Schema("mock.echo", "system", 0)));

  CHECK(registry.Find("mock.echo") != nullptr);
  CHECK(registry.Find("mock.nonexistent") == nullptr);
}
