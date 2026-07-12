#include <doctest/doctest.h>

#include <filesystem>
#include <memory>
#include <string>

#include "harness/execution_engine.h"
#include "harness/permission_manager.h"
#include "harness/tool_registry.h"

namespace {

class SpyTool final : public harness::ITool {
 public:
  explicit SpyTool(harness::ToolSchema schema) : schema_(std::move(schema)) {}

  harness::ToolSchema GetSchema() const override { return schema_; }
  harness::ToolResult Execute(const harness::Json& arguments) override {
    was_executed = true;
    return {.success = true,
            .summary = "spy executed",
            .data = {{"received", arguments}}};
  }
  void Cancel() override { was_cancelled = true; }

  harness::ToolSchema schema_;
  bool was_executed = false;
  bool was_cancelled = false;
};

harness::ToolSchema MultiPermSchema() {
  return {
      .name = "multi.perm.tool",
      .description = "Tool requiring multiple permissions",
      .parameters = {},
      .category = "system",
      .danger_level = 1,
      .required_permissions =
          {
              {harness::PermissionScope::FileRead, "src/main.cpp"},
              {harness::PermissionScope::FileWrite, "src/main.cpp"},
          },
  };
}

harness::ExecutionInvocation Invocation(std::string tool_name,
                                        std::string inv_id = {}) {
  if (inv_id.empty()) {
    static int counter = 0;
    inv_id = "e2e-inv-" + std::to_string(++counter);
  }
  return {std::move(inv_id), std::move(tool_name),
          harness::Json::object()};
}

}  // namespace

TEST_CASE("execution engine allows permitted tool") {
  auto* spy = new SpyTool({
      .name = "safe.read",
      .description = "Read-only safe tool",
      .parameters = {},
      .category = "system",
      .danger_level = 0,
      .required_permissions =
          {{harness::PermissionScope::FileRead, "src/main.cpp"}},
  });

  harness::ToolRegistry registry;
  registry.Register(std::unique_ptr<harness::ITool>(spy));

  harness::PermissionManager permissions(std::filesystem::current_path());
  // Rule uses a glob that covers the exact resource path.
  permissions.AddRule({"safe.*", harness::PermissionScope::FileRead,
                       "src/*", harness::PermissionDecision::Allow});

  harness::ExecutionEngine engine(registry, permissions);
  auto result = engine.Execute(Invocation("safe.read"));

  INFO("error:", result.error_detail);
  CHECK(result.success);
  CHECK(spy->was_executed);
}

TEST_CASE("execution engine blocks tool without matching permission rule") {
  auto* spy = new SpyTool({
      .name = "restricted.read",
      .description = "Needs permission but has none",
      .parameters = {},
      .category = "system",
      .danger_level = 0,
      .required_permissions =
          {{harness::PermissionScope::FileRead, "Secrets/passwords.txt"}},
  });

  harness::ToolRegistry registry;
  registry.Register(std::unique_ptr<harness::ITool>(spy));

  harness::PermissionManager permissions(std::filesystem::current_path());

  harness::ExecutionEngine engine(registry, permissions);
  auto result = engine.Execute(Invocation("restricted.read"));

  CHECK_FALSE(result.success);
  CHECK_FALSE(spy->was_executed);
}

TEST_CASE("execution engine blocks when a deny rule matches") {
  auto* spy = new SpyTool({
      .name = "blocked.write",
      .description = "Write tool with explicit deny",
      .parameters = {},
      .category = "system",
      .danger_level = 1,
      .required_permissions =
          {{harness::PermissionScope::FileWrite, "src/main.cpp"}},
  });

  harness::ToolRegistry registry;
  registry.Register(std::unique_ptr<harness::ITool>(spy));

  harness::PermissionManager permissions(std::filesystem::current_path());
  permissions.AddRule({"blocked.*", harness::PermissionScope::FileWrite,
                       "*", harness::PermissionDecision::Deny});

  harness::ExecutionEngine engine(registry, permissions);
  auto result = engine.Execute(Invocation("blocked.write"));

  CHECK_FALSE(result.success);
  CHECK_FALSE(spy->was_executed);
}

TEST_CASE(
    "execution engine blocks when one of multiple permissions is denied") {
  auto* spy = new SpyTool(MultiPermSchema());

  harness::ToolRegistry registry;
  registry.Register(std::unique_ptr<harness::ITool>(spy));

  harness::PermissionManager permissions(std::filesystem::current_path());
  permissions.AddRule({"multi.*", harness::PermissionScope::FileRead,
                       "src/*", harness::PermissionDecision::Allow});

  harness::ExecutionEngine engine(registry, permissions);
  auto result = engine.Execute(Invocation("multi.perm.tool"));

  INFO("error:", result.error_detail);
  CHECK_FALSE(result.success);
  CHECK_FALSE(spy->was_executed);
}

TEST_CASE("execution engine allows tool when all multi-permissions pass") {
  auto* spy = new SpyTool(MultiPermSchema());

  harness::ToolRegistry registry;
  registry.Register(std::unique_ptr<harness::ITool>(spy));

  harness::PermissionManager permissions(std::filesystem::current_path());
  permissions.AddRule({"multi.*", harness::PermissionScope::FileRead,
                       "src/*", harness::PermissionDecision::Allow});
  permissions.AddRule({"multi.*", harness::PermissionScope::FileWrite,
                       "src/*", harness::PermissionDecision::Allow});

  harness::ExecutionEngine engine(registry, permissions);
  auto result = engine.Execute(Invocation("multi.perm.tool"));
  INFO("error:", result.error_detail);
  CHECK(result.success);
  CHECK(spy->was_executed);
}

TEST_CASE("execution engine returns error for unknown tool") {
  harness::ToolRegistry registry;
  harness::PermissionManager permissions(std::filesystem::current_path());
  harness::ExecutionEngine engine(registry, permissions);

  auto result = engine.Execute(Invocation("nonexistent.tool"));
  CHECK_FALSE(result.success);
  CHECK_FALSE(result.error_detail.empty());
}
