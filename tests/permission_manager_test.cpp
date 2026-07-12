#include <doctest/doctest.h>

#include <atomic>
#include <filesystem>

#include "harness/permission_manager.h"

namespace {

using harness::PermissionDecision;
using harness::PermissionManager;
using harness::PermissionRequest;
using harness::PermissionRule;
using harness::PermissionScope;

PermissionManager Manager() { return PermissionManager(std::filesystem::current_path()); }

PermissionRequest Request(std::string tool_name, PermissionScope scope,
                          std::string resource, std::string invocation_id = {}) {
  static std::atomic_size_t next_invocation = 0;
  if (invocation_id.empty()) {
    invocation_id = "invocation-" + std::to_string(++next_invocation);
  }
  return {std::move(invocation_id), std::move(tool_name), scope, std::move(resource)};
}

}  // namespace

TEST_CASE("permission manager denies unmatched requests") {
  auto manager = Manager();
  CHECK(manager.Check(Request("project.file.read", PermissionScope::FileRead,
                              "Content/config.json")) == PermissionDecision::Deny);
}

TEST_CASE("permission manager selects the most specific glob rule") {
  auto manager = Manager();
  manager.AddRule({"mcp.unreal.scene.*", PermissionScope::EngineModify, "*",
                   PermissionDecision::Ask});
  manager.AddRule({"mcp.unreal.scene.get_*", PermissionScope::EngineModify, "*",
                   PermissionDecision::Allow});
  manager.AddRule({"mcp.unreal.scene.delete_*", PermissionScope::EngineModify, "Level/*",
                   PermissionDecision::Deny});

  CHECK(manager.Check(Request("mcp.unreal.scene.get_actor", PermissionScope::EngineModify,
                              "Level/Main")) == PermissionDecision::Allow);
  CHECK(manager.Check(Request("mcp.unreal.scene.spawn_actor", PermissionScope::EngineModify,
                              "Level/Main")) == PermissionDecision::Ask);
  CHECK(manager.Check(Request("mcp.unreal.scene.delete_actor", PermissionScope::EngineModify,
                              "Level/Main")) == PermissionDecision::Deny);
}

TEST_CASE("permission manager consumes one-time grants") {
  auto manager = Manager();
  const auto request =
      Request("project.file.write", PermissionScope::FileWrite, "Content/new.txt");
  manager.AddRule({"project.file.*", PermissionScope::FileWrite, "Content/*",
                   PermissionDecision::Ask});

  CHECK(manager.Check(request) == PermissionDecision::Ask);
  const auto granted = manager.GrantOnce(request);
  CHECK(granted == PermissionDecision::Allow);
  CHECK(manager.Check(request) == PermissionDecision::Deny);

  const auto next_request =
      Request("project.file.write", PermissionScope::FileWrite, "Content/new.txt");
  CHECK(manager.Check(next_request) == PermissionDecision::Ask);
}

TEST_CASE("permission manager maintains and clears session decisions") {
  auto manager = Manager();
  const auto request =
      Request("shell.execute", PermissionScope::ShellExecute, "git status");
  manager.AddRule(
      {"shell.*", PermissionScope::ShellExecute, "git *", PermissionDecision::Ask});

  CHECK(manager.Check(request) == PermissionDecision::Ask);
  CHECK(manager.GrantSession(request) == PermissionDecision::Allow);
  CHECK(manager.Check(request) == PermissionDecision::Deny);
  const auto second_request =
      Request("shell.execute", PermissionScope::ShellExecute, "git status");
  CHECK(manager.Check(second_request) == PermissionDecision::Allow);

  manager.DenySession(second_request);
  const auto third_request =
      Request("shell.execute", PermissionScope::ShellExecute, "git status");
  CHECK(manager.Check(third_request) == PermissionDecision::Deny);

  manager.ClearSession();
  const auto fourth_request =
      Request("shell.execute", PermissionScope::ShellExecute, "git status");
  CHECK(manager.Check(fourth_request) == PermissionDecision::Ask);
}

TEST_CASE("configured deny cannot be bypassed by a transient grant") {
  auto manager = Manager();
  const auto request =
      Request("shell.execute", PermissionScope::ShellExecute, "rm -rf Content");
  manager.AddRule(
      {"shell.*", PermissionScope::ShellExecute, "*", PermissionDecision::Ask});
  manager.AddRule(
      {"shell.*", PermissionScope::ShellExecute, "rm *", PermissionDecision::Deny});

  CHECK(manager.Check(request) == PermissionDecision::Deny);
  CHECK_THROWS_AS(static_cast<void>(manager.GrantOnce(request)), std::invalid_argument);
}

TEST_CASE("one-time grants are bound to an invocation") {
  auto manager = Manager();
  manager.AddRule({"project.file.*", PermissionScope::FileWrite, "Content/*",
                   PermissionDecision::Ask});
  const auto first = Request("project.file.write", PermissionScope::FileWrite,
                             "Content/new.txt", "invocation-a");
  const auto second = Request("project.file.write", PermissionScope::FileWrite,
                              "Content/new.txt", "invocation-b");

  CHECK(manager.Check(first) == PermissionDecision::Ask);
  CHECK(manager.GrantOnce(first) == PermissionDecision::Allow);
  CHECK(manager.Check(second) == PermissionDecision::Ask);
  CHECK(manager.Check(first) == PermissionDecision::Deny);
}

TEST_CASE("a deny rule dominates an exact broad allow") {
  auto manager = Manager();
  manager.AddRule(
      {"shell.execute", PermissionScope::ShellExecute, "*", PermissionDecision::Allow});
  manager.AddRule(
      {"shell.*", PermissionScope::ShellExecute, "rm *", PermissionDecision::Deny});

  CHECK(manager.Check(Request("shell.execute", PermissionScope::ShellExecute,
                              "rm -rf Content")) == PermissionDecision::Deny);
}

TEST_CASE("file permissions reject traversal and root escape") {
  auto manager = Manager();
  manager.AddRule(
      {"project.file.*", PermissionScope::FileRead, "Content/*", PermissionDecision::Allow});

  CHECK(manager.Check(Request("project.file.read", PermissionScope::FileRead,
                              "Content/Actors/hero.json")) == PermissionDecision::Allow);
  CHECK(manager.Check(Request("project.file.read", PermissionScope::FileRead,
                              "../secret.txt")) == PermissionDecision::Deny);
  CHECK_THROWS_AS(
      static_cast<void>(manager.GrantSession(
          Request("project.file.read", PermissionScope::FileRead, "Content/../../secret.txt"))),
      std::invalid_argument);
}

TEST_CASE("network rules normalize URLs to lowercase domains") {
  auto manager = Manager();
  manager.AddRule(
      {"network.fetch", PermissionScope::NetworkCall, "*.example.com",
       PermissionDecision::Allow});

  const auto allowed = manager.Check(Request("network.fetch", PermissionScope::NetworkCall,
                                             "HTTPS://API.EXAMPLE.COM:443/v1/models"));
  CHECK(allowed == PermissionDecision::Allow);
  CHECK(allowed.authorization_resource == "api.example.com");
  CHECK(allowed.execution_resource == "https://api.example.com:443/v1/models");
  CHECK(manager.Check(Request("network.fetch", PermissionScope::NetworkCall,
                              "https://example.com/v1")) == PermissionDecision::Deny);
}

TEST_CASE("permission scopes remain isolated") {
  auto manager = Manager();
  manager.AddRule(
      {"project.file.*", PermissionScope::FileRead, "Content/*", PermissionDecision::Allow});

  CHECK(manager.Check(Request("project.file.read", PermissionScope::FileWrite,
                              "Content/config.json")) == PermissionDecision::Deny);
}

TEST_CASE("permission checks return the normalized resource for execution") {
  auto manager = Manager();
  manager.AddRule(
      {"project.file.*", PermissionScope::FileRead, "Content/*", PermissionDecision::Allow});
  const auto result = manager.Check(
      Request("project.file.read", PermissionScope::FileRead, "Content/Actors/../hero.json"));

  CHECK(result == PermissionDecision::Deny);

  const auto allowed = manager.Check(
      Request("project.file.read", PermissionScope::FileRead, "Content/hero.json"));
  CHECK(allowed == PermissionDecision::Allow);
  CHECK(std::filesystem::path(allowed.execution_resource).is_absolute());
  CHECK(allowed.execution_resource.ends_with("/Content/hero.json"));
  CHECK(allowed.authorization_resource.ends_with("/CONTENT/HERO.JSON"));
}

TEST_CASE("shell allow rules still require per-invocation confirmation") {
  auto manager = Manager();
  const auto request =
      Request("shell.execute", PermissionScope::ShellExecute, "git status");
  manager.AddRule(
      {"shell.execute", PermissionScope::ShellExecute, "git *", PermissionDecision::Allow});

  CHECK(manager.Check(request) == PermissionDecision::Ask);
  CHECK(manager.GrantOnce(request) == PermissionDecision::Allow);
  CHECK(manager.Check(request) == PermissionDecision::Deny);
}

TEST_CASE("cancelled and completed invocation IDs cannot be replayed") {
  auto manager = Manager();
  const auto request = Request("project.file.write", PermissionScope::FileWrite,
                               "Content/new.txt", "fixed-invocation");
  manager.AddRule({"project.file.*", PermissionScope::FileWrite, "Content/*",
                   PermissionDecision::Ask});

  CHECK(manager.Check(request) == PermissionDecision::Ask);
  manager.Cancel(request.invocation_id);
  CHECK(manager.Check(request) == PermissionDecision::Deny);
  CHECK_THROWS_AS(static_cast<void>(manager.GrantOnce(request)), std::invalid_argument);
}

TEST_CASE("cancellation recorded before checking an invocation fails closed") {
  auto manager = Manager();
  const auto request = Request("project.file.write", PermissionScope::FileWrite,
                               "Content/new.txt", "cancel-before-check");
  manager.AddRule({"project.file.*", PermissionScope::FileWrite, "Content/*",
                   PermissionDecision::Ask});

  manager.Cancel(request.invocation_id);
  CHECK(manager.Check(request) == PermissionDecision::Deny);
}

TEST_CASE("session deny revokes matching pending requests") {
  auto manager = Manager();
  const auto first = Request("project.file.write", PermissionScope::FileWrite,
                             "Content/new.txt", "pending-before-deny");
  const auto denial = Request("project.file.write", PermissionScope::FileWrite,
                              "Content/new.txt", "deny-session");
  manager.AddRule({"project.file.*", PermissionScope::FileWrite, "Content/*",
                   PermissionDecision::Ask});

  CHECK(manager.Check(first) == PermissionDecision::Ask);
  manager.DenySession(denial);
  CHECK_THROWS_AS(static_cast<void>(manager.GrantOnce(first)), std::invalid_argument);
  CHECK(manager.Check(Request("project.file.write", PermissionScope::FileWrite,
                              "Content/new.txt")) == PermissionDecision::Deny);
}

TEST_CASE("reusing an invocation ID for another resource fails closed") {
  auto manager = Manager();
  const auto first = Request("project.file.write", PermissionScope::FileWrite,
                             "Content/first.txt", "duplicate-invocation");
  const auto conflicting = Request("project.file.write", PermissionScope::FileWrite,
                                   "Content/second.txt", "duplicate-invocation");
  manager.AddRule({"project.file.*", PermissionScope::FileWrite, "Content/*",
                   PermissionDecision::Ask});

  CHECK(manager.Check(first) == PermissionDecision::Ask);
  CHECK(manager.Check(conflicting) == PermissionDecision::Deny);
  CHECK(manager.Check(first) == PermissionDecision::Deny);
  CHECK_THROWS_AS(static_cast<void>(manager.GrantOnce(first)), std::invalid_argument);
}

TEST_CASE("pending permission requests have a fail-closed capacity limit") {
  auto manager = Manager();
  manager.AddRule({"engine.modify", PermissionScope::EngineModify, "Level/*",
                   PermissionDecision::Ask});

  for (std::size_t index = 0; index < 1024; ++index) {
    CHECK(manager.Check(Request("engine.modify", PermissionScope::EngineModify,
                                "Level/Main", "pending-" + std::to_string(index))) ==
          PermissionDecision::Ask);
  }
  CHECK(manager.Check(Request("engine.modify", PermissionScope::EngineModify,
                              "Level/Main", "pending-overflow")) ==
        PermissionDecision::Deny);
}

TEST_CASE("UNC and Windows device paths fail closed before authorization") {
  auto manager = Manager();
  manager.AddRule(
      {"project.file.*", PermissionScope::FileRead, "*", PermissionDecision::Allow});

  CHECK(manager.Check(Request("project.file.read", PermissionScope::FileRead,
                              R"(\\attacker\share\secret.txt)")) ==
        PermissionDecision::Deny);
  CHECK(manager.Check(Request("project.file.read", PermissionScope::FileRead,
                              R"(\\?\C:\secret.txt)")) == PermissionDecision::Deny);
}

TEST_CASE("clearing rules invalidates grants derived from those rules") {
  auto manager = Manager();
  const auto request =
      Request("shell.execute", PermissionScope::ShellExecute, "git status");
  manager.AddRule(
      {"shell.*", PermissionScope::ShellExecute, "git *", PermissionDecision::Ask});
  CHECK(manager.Check(request) == PermissionDecision::Ask);
  CHECK(manager.GrantSession(request) == PermissionDecision::Allow);
  CHECK(manager.Check(Request("shell.execute", PermissionScope::ShellExecute,
                              "git status")) == PermissionDecision::Allow);

  manager.ClearRules();
  CHECK(manager.Check(request) == PermissionDecision::Deny);
}

TEST_CASE("permission resources reject embedded nulls and shell composition") {
  auto manager = Manager();
  manager.AddRule(
      {"shell.*", PermissionScope::ShellExecute, "git *", PermissionDecision::Allow});
  manager.AddRule(
      {"project.file.*", PermissionScope::FileRead, "Content/*", PermissionDecision::Allow});

  std::string nul_path = "Content/config.json";
  nul_path.push_back('\0');
  nul_path += ".txt";
  CHECK(manager.Check(Request("project.file.read", PermissionScope::FileRead, nul_path)) ==
        PermissionDecision::Deny);
  CHECK(manager.Check(Request("shell.execute", PermissionScope::ShellExecute,
                              "git status & del Content\\secret.txt")) ==
        PermissionDecision::Deny);
}
