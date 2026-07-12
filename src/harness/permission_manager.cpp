#include "harness/permission_manager.h"

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <stdexcept>
#include <string_view>
#include <tuple>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#undef ShellExecute
#endif

namespace harness {
namespace {

std::string Trim(std::string value) {
  const auto is_space = [](unsigned char character) { return std::isspace(character); };
  value.erase(value.begin(),
              std::find_if_not(value.begin(), value.end(), is_space));
  value.erase(std::find_if_not(value.rbegin(), value.rend(), is_space).base(),
              value.end());
  return value;
}

std::string Lowercase(std::string value) {
  std::ranges::transform(value, value.begin(), [](unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  return value;
}

bool HasWildcard(std::string_view pattern) {
  return pattern.find_first_of("*?") != std::string_view::npos;
}

std::size_t LiteralCount(std::string_view pattern) {
  return static_cast<std::size_t>(std::ranges::count_if(
      pattern, [](char character) { return character != '*' && character != '?'; }));
}

bool GlobMatch(std::string_view pattern, std::string_view value) {
  std::size_t pattern_index = 0;
  std::size_t value_index = 0;
  std::size_t star_index = std::string_view::npos;
  std::size_t retry_value_index = 0;

  while (value_index < value.size()) {
    if (pattern_index < pattern.size() &&
        (pattern[pattern_index] == '?' || pattern[pattern_index] == value[value_index])) {
      ++pattern_index;
      ++value_index;
    } else if (pattern_index < pattern.size() && pattern[pattern_index] == '*') {
      star_index = pattern_index++;
      retry_value_index = value_index;
    } else if (star_index != std::string_view::npos) {
      pattern_index = star_index + 1;
      value_index = ++retry_value_index;
    } else {
      return false;
    }
  }

  while (pattern_index < pattern.size() && pattern[pattern_index] == '*') {
    ++pattern_index;
  }
  return pattern_index == pattern.size();
}

bool HasTraversal(const std::filesystem::path& path) {
  return std::ranges::any_of(path, [](const auto& component) {
    return component == std::filesystem::path("..");
  });
}

void ValidateWindowsPath(const std::filesystem::path& path) {
#ifdef _WIN32
  for (const auto& component : path.relative_path()) {
    const auto value = component.native();
    if (value.empty() || value == L"." || value == L"..") {
      continue;
    }
    if (value.back() == L'.' || value.back() == L' ' || value.find(L':') != std::wstring::npos) {
      throw std::invalid_argument("file permission resource contains a Windows path alias");
    }
    const auto tilde = value.find(L'~');
    if (tilde != std::wstring::npos && tilde + 1 < value.size() &&
        value[tilde + 1] >= L'1' && value[tilde + 1] <= L'9') {
      throw std::invalid_argument("file permission resource contains an 8.3 path alias");
    }

    auto stem = value.substr(0, value.find(L'.'));
    std::ranges::transform(stem, stem.begin(), [](wchar_t character) {
      return static_cast<wchar_t>(std::towupper(character));
    });
    const bool numbered_device = stem.size() == 4 && stem[3] >= L'1' && stem[3] <= L'9' &&
                                 (stem.starts_with(L"COM") || stem.starts_with(L"LPT"));
    if (stem == L"CON" || stem == L"PRN" || stem == L"AUX" || stem == L"NUL" ||
        numbered_device) {
      throw std::invalid_argument("file permission resource contains a reserved device name");
    }
  }
#else
  static_cast<void>(path);
#endif
}

void RejectReparsePoints(const std::filesystem::path& root,
                         const std::filesystem::path& candidate) {
#ifdef _WIN32
  auto current = root;
  const auto relative = candidate.lexically_relative(root);
  for (const auto& component : relative) {
    current /= component;
    const DWORD attributes = GetFileAttributesW(current.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
      const DWORD error = GetLastError();
      if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
        break;
      }
      throw std::invalid_argument("file permission resource could not be inspected");
    }
    if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
      throw std::invalid_argument("file permission resource crosses a reparse point");
    }
  }
#else
  static_cast<void>(root);
  static_cast<void>(candidate);
#endif
}

std::string WideToUtf8(std::wstring_view value) {
#ifdef _WIN32
  if (value.empty()) {
    return {};
  }
  const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                                       static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
  if (size <= 0) {
    throw std::invalid_argument("file permission path is not valid Unicode");
  }
  std::string result(static_cast<std::size_t>(size), '\0');
  if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                          static_cast<int>(value.size()), result.data(), size, nullptr, nullptr) <= 0) {
    throw std::invalid_argument("file permission path is not valid Unicode");
  }
  return result;
#else
  return std::string(value.begin(), value.end());
#endif
}

std::string PathAuthorizationKey(const std::filesystem::path& path) {
#ifdef _WIN32
  auto value = path.generic_wstring();
  const int folded_size = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE,
                                        value.data(), static_cast<int>(value.size()),
                                        nullptr, 0, nullptr, nullptr, 0);
  if (folded_size <= 0) {
    throw std::invalid_argument("file permission path could not be case-normalized");
  }
  std::wstring folded(static_cast<std::size_t>(folded_size), L'\0');
  if (LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE, value.data(),
                    static_cast<int>(value.size()), folded.data(), folded_size,
                    nullptr, nullptr, 0) <= 0) {
    throw std::invalid_argument("file permission path could not be case-normalized");
  }
  auto normalized = WideToUtf8(folded);
#else
  auto normalized = path.generic_string();
#endif
  while (normalized.size() > 1 && normalized.back() == '/') {
    normalized.pop_back();
  }
  return normalized;
}

std::string PathExecutionResource(const std::filesystem::path& path) {
#ifdef _WIN32
  return WideToUtf8(path.generic_wstring());
#else
  return path.generic_string();
#endif
}

bool IsWithin(std::string_view root, std::string_view path) {
  return path == root ||
         (path.size() > root.size() && path.starts_with(root) && path[root.size()] == '/');
}

struct NetworkResource {
  std::string host;
  std::string execution;
};

NetworkResource NormalizeNetwork(std::string resource, bool pattern) {
  resource = Trim(std::move(resource));
  if (resource.empty() || resource.find_first_of("\r\n") != std::string::npos ||
      resource.find('\0') != std::string::npos) {
    throw std::invalid_argument("network resource must be a domain or URL");
  }

  const auto scheme_end = resource.find("://");
  if (pattern && scheme_end != std::string::npos) {
    throw std::invalid_argument("network permission patterns must contain only a host");
  }
  const auto authority_start = scheme_end == std::string::npos ? 0 : scheme_end + 3;
  const auto authority_end = resource.find_first_of("/?#", authority_start);
  auto authority = Lowercase(resource.substr(
      authority_start, authority_end == std::string::npos ? std::string::npos
                                                          : authority_end - authority_start));
  if (authority.empty() || authority.find('@') != std::string::npos) {
    throw std::invalid_argument("network resource contains an invalid authority");
  }

  std::string host;
  std::string port;
  if (authority.front() == '[') {
    const auto closing_bracket = authority.find(']');
    if (closing_bracket == std::string::npos) {
      throw std::invalid_argument("network resource contains an invalid IPv6 address");
    }
    host = authority.substr(0, closing_bracket + 1);
    port = authority.substr(closing_bracket + 1);
  } else {
    const auto colon = authority.rfind(':');
    if (colon != std::string::npos) {
      if (authority.find(':') != colon) {
        throw std::invalid_argument("IPv6 network resources must use brackets");
      }
      host = authority.substr(0, colon);
      port = authority.substr(colon);
    } else {
      host = authority;
    }
  }

  if (!port.empty()) {
    if (pattern || port.front() != ':' || port.size() == 1 ||
        !std::ranges::all_of(port.substr(1), [](unsigned char character) {
          return std::isdigit(character);
        })) {
      throw std::invalid_argument("network resource contains an invalid port");
    }
  }

  while (!host.empty() && host.back() == '.') {
    host.pop_back();
  }
  if (host.empty() || (!pattern && HasWildcard(host))) {
    throw std::invalid_argument("network resource must contain a host");
  }

  if (pattern || scheme_end == std::string::npos) {
    return {host, host};
  }
  const auto scheme = Lowercase(resource.substr(0, scheme_end));
  if (scheme != "http" && scheme != "https") {
    throw std::invalid_argument("network resource uses an unsupported URL scheme");
  }
  std::string suffix = authority_end == std::string::npos ? std::string{} : resource.substr(authority_end);
  if (const auto fragment = suffix.find('#'); fragment != std::string::npos) {
    suffix.erase(fragment);
  }
  return {host, scheme + "://" + host + port + suffix};
}

}  // namespace

PermissionManager::PermissionManager(std::filesystem::path project_root) {
  if (project_root.empty()) {
    throw std::invalid_argument("project_root must not be empty");
  }
  std::error_code error;
  const auto absolute_root = std::filesystem::absolute(std::move(project_root), error);
  if (error) {
    throw std::invalid_argument("project_root could not be normalized");
  }
  project_root_ = std::filesystem::weakly_canonical(absolute_root, error);
  if (error) {
    throw std::invalid_argument("project_root could not be normalized");
  }
  project_root_string_ = PathAuthorizationKey(project_root_);
}

void PermissionManager::AddRule(PermissionRule rule) {
  rule.tool_pattern = Trim(std::move(rule.tool_pattern));
  if (rule.tool_pattern.empty() || rule.tool_pattern.find_first_of("\r\n") != std::string::npos ||
      rule.tool_pattern.find('\0') != std::string::npos) {
    throw std::invalid_argument("permission rule tool pattern must not be empty");
  }
  rule.resource_pattern =
      NormalizeResource(rule.scope, rule.resource_pattern, true).authorization;

  std::scoped_lock lock(mutex_);
  rules_.push_back(std::move(rule));
}

void PermissionManager::ClearRules() {
  std::scoped_lock lock(mutex_);
  rules_.clear();
  allow_session_.clear();
  deny_session_.clear();
  pending_.clear();
  completed_invocations_.clear();
  completed_order_.clear();
}

PermissionCheckResult PermissionManager::Check(const PermissionRequest& request) {
  NormalizedRequest normalized;
  try {
    normalized = Normalize(request);
  } catch (const std::invalid_argument&) {
    return {};
  } catch (const std::filesystem::filesystem_error&) {
    return {};
  }
  const auto& key = normalized.key;

  std::scoped_lock lock(mutex_);
  PurgeExpired(std::chrono::steady_clock::now());
  if (completed_invocations_.contains(key.invocation_id)) {
    TouchCompleted(key.invocation_id);
    return {PermissionDecision::Deny, key.resource, normalized.execution_resource};
  }

  const auto configured = MatchRule(key);
  const auto session_key = SessionKey(key);
  if (configured == PermissionDecision::Deny || deny_session_.contains(session_key)) {
    MarkCompleted(key.invocation_id);
    return {PermissionDecision::Deny, key.resource, normalized.execution_resource};
  }
  if (const auto pending = pending_.find(key.invocation_id); pending != pending_.end()) {
    if (pending->second.key == key &&
        pending->second.execution_resource == normalized.execution_resource) {
      return {PermissionDecision::Ask, key.resource, normalized.execution_resource};
    }
    MarkCompleted(key.invocation_id);
    return {PermissionDecision::Deny, key.resource, normalized.execution_resource};
  }
  if (allow_session_.contains(session_key)) {
    MarkCompleted(key.invocation_id);
    return {PermissionDecision::Allow, key.resource, normalized.execution_resource};
  }

  auto decision = configured.value_or(PermissionDecision::Deny);
  if (key.scope == PermissionScope::ShellExecute && decision == PermissionDecision::Allow) {
    decision = PermissionDecision::Ask;
  }
  if (decision == PermissionDecision::Ask) {
    if (pending_.size() >= kMaxPendingRequests) {
      MarkCompleted(key.invocation_id);
      return {PermissionDecision::Deny, key.resource, normalized.execution_resource};
    }
    pending_.emplace(
        key.invocation_id,
        PendingRequest{key, normalized.execution_resource,
                       std::chrono::steady_clock::now() + kPendingTimeout});
  } else {
    MarkCompleted(key.invocation_id);
  }
  return {decision, key.resource, normalized.execution_resource};
}

PermissionCheckResult PermissionManager::GrantOnce(const PermissionRequest& request) {
  auto normalized = Normalize(request);
  const auto& key = normalized.key;
  std::scoped_lock lock(mutex_);
  PurgeExpired(std::chrono::steady_clock::now());
  const auto pending = pending_.find(key.invocation_id);
  const auto configured = MatchRule(key);
  const auto session_key = SessionKey(key);
  const bool promptable = configured == PermissionDecision::Ask ||
                          (key.scope == PermissionScope::ShellExecute &&
                           configured == PermissionDecision::Allow);
  if (!promptable || deny_session_.contains(session_key) || pending == pending_.end() ||
      !(pending->second.key == key) ||
      pending->second.execution_resource != normalized.execution_resource) {
    if (configured == PermissionDecision::Deny || deny_session_.contains(session_key)) {
      MarkCompleted(key.invocation_id);
    }
    throw std::invalid_argument("one-time grant does not match a pending request");
  }
  const auto result = PermissionCheckResult{PermissionDecision::Allow, key.resource,
                                            pending->second.execution_resource};
  MarkCompleted(key.invocation_id);
  return result;
}

PermissionCheckResult PermissionManager::GrantSession(const PermissionRequest& request) {
  auto normalized = Normalize(request);
  auto key = normalized.key;
  std::scoped_lock lock(mutex_);
  PurgeExpired(std::chrono::steady_clock::now());
  const auto pending = pending_.find(key.invocation_id);
  const auto configured = MatchRule(key);
  const auto session_key = SessionKey(key);
  const bool promptable = configured == PermissionDecision::Ask ||
                          (key.scope == PermissionScope::ShellExecute &&
                           configured == PermissionDecision::Allow);
  if (!promptable || deny_session_.contains(session_key) || pending == pending_.end() ||
      !(pending->second.key == key) ||
      pending->second.execution_resource != normalized.execution_resource) {
    if (configured == PermissionDecision::Deny || deny_session_.contains(session_key)) {
      MarkCompleted(key.invocation_id);
    }
    throw std::invalid_argument("session grant does not match a pending request");
  }
  const auto result = PermissionCheckResult{PermissionDecision::Allow, key.resource,
                                            pending->second.execution_resource};
  const auto invocation_id = key.invocation_id;
  key = SessionKey(std::move(key));
  allow_session_.insert(std::move(key));
  MarkCompleted(invocation_id);
  return result;
}

void PermissionManager::DenyOnce(const PermissionRequest& request) {
  auto normalized = Normalize(request);
  const auto& key = normalized.key;
  std::scoped_lock lock(mutex_);
  PurgeExpired(std::chrono::steady_clock::now());
  const auto pending = pending_.find(key.invocation_id);
  if (pending == pending_.end() || !(pending->second.key == key) ||
      pending->second.execution_resource != normalized.execution_resource) {
    throw std::invalid_argument("denial does not match a pending request");
  }
  pending_.erase(pending);
  MarkCompleted(key.invocation_id);
}

void PermissionManager::DenySession(const PermissionRequest& request) {
  auto request_key = Normalize(request).key;
  auto session_key = SessionKey(request_key);
  std::scoped_lock lock(mutex_);
  std::vector<std::string> revoked_invocations;
  for (const auto& [invocation_id, pending] : pending_) {
    if (SessionKey(pending.key) == session_key) {
      revoked_invocations.push_back(invocation_id);
    }
  }
  for (const auto& invocation_id : revoked_invocations) {
    MarkCompleted(invocation_id);
  }
  allow_session_.erase(session_key);
  deny_session_.insert(std::move(session_key));
  MarkCompleted(request_key.invocation_id);
}

void PermissionManager::Cancel(std::string_view invocation_id) {
  const auto normalized_id = Trim(std::string(invocation_id));
  if (normalized_id.empty()) {
    throw std::invalid_argument("invocation id must not be empty");
  }
  std::scoped_lock lock(mutex_);
  MarkCompleted(normalized_id);
}

void PermissionManager::ClearSession() {
  std::scoped_lock lock(mutex_);
  allow_session_.clear();
  deny_session_.clear();
  pending_.clear();
  completed_invocations_.clear();
  completed_order_.clear();
}

std::size_t PermissionManager::RuleCount() const {
  std::scoped_lock lock(mutex_);
  return rules_.size();
}

std::size_t PermissionManager::RequestKeyHash::operator()(const RequestKey& key) const
    noexcept {
  std::size_t value = std::hash<std::string>{}(key.tool_name);
  value ^= std::hash<std::string>{}(key.invocation_id) + 0x9e3779b9 + (value << 6) +
           (value >> 2);
  value ^= std::hash<int>{}(static_cast<int>(key.scope)) + 0x9e3779b9 + (value << 6) +
           (value >> 2);
  value ^= std::hash<std::string>{}(key.resource) + 0x9e3779b9 + (value << 6) +
           (value >> 2);
  return value;
}

PermissionManager::NormalizedRequest PermissionManager::Normalize(
    const PermissionRequest& request) const {
  const auto invocation_id = Trim(request.invocation_id);
  const auto tool_name = Trim(request.tool_name);
  if (invocation_id.empty() || invocation_id.find_first_of("*?\r\n") != std::string::npos ||
      invocation_id.find('\0') != std::string::npos) {
    throw std::invalid_argument("permission request invocation id must be exact");
  }
  if (tool_name.empty() || HasWildcard(tool_name) ||
      tool_name.find_first_of("\r\n") != std::string::npos ||
      tool_name.find('\0') != std::string::npos) {
    throw std::invalid_argument("permission request tool name must be exact");
  }
  try {
    auto resource = NormalizeResource(request.scope, request.resource, false);
    return {{invocation_id, tool_name, request.scope, std::move(resource.authorization)},
            std::move(resource.execution)};
  } catch (const std::filesystem::filesystem_error& error) {
    throw std::invalid_argument(std::string("permission resource could not be normalized: ") +
                                error.what());
  }
}

PermissionManager::NormalizedResource PermissionManager::NormalizeResource(
    PermissionScope scope, const std::string& resource, bool pattern) const {
  if (resource.empty()) {
    throw std::invalid_argument("permission resource must not be empty");
  }
  if (resource.find('\0') != std::string::npos) {
    throw std::invalid_argument("permission resource contains a null byte");
  }

  if (scope == PermissionScope::FileRead || scope == PermissionScope::FileWrite) {
    if (!pattern && HasWildcard(resource)) {
      throw std::invalid_argument("file permission request resource must be exact");
    }
    const std::filesystem::path raw(resource);
    if (HasTraversal(raw)) {
      throw std::invalid_argument("file permission resource contains traversal");
    }
    ValidateWindowsPath(raw);
    const auto combined = (raw.is_absolute() ? raw : project_root_ / raw).lexically_normal();
    const auto lexical = PathAuthorizationKey(combined);
    if (!IsWithin(project_root_string_, lexical)) {
      throw std::invalid_argument("file permission resource escapes project root");
    }
#ifdef _WIN32
    const auto native = raw.native();
    if (native.starts_with(L"\\\\") || native.starts_with(L"\\?\\") ||
        native.starts_with(L"\\.\\")) {
      throw std::invalid_argument("file permission resource uses a network or device path");
    }
#endif
    std::error_code error;
    const auto resolved = pattern ? combined : std::filesystem::weakly_canonical(combined, error);
    if (error) {
      throw std::invalid_argument("file permission resource could not be resolved");
    }
    const auto authorization = PathAuthorizationKey(resolved);
    if (!IsWithin(project_root_string_, authorization)) {
      throw std::invalid_argument("file permission resource escapes project root");
    }
    if (!pattern) {
      RejectReparsePoints(project_root_, combined);
    }
    return {authorization, PathExecutionResource(resolved)};
  }

  if (scope == PermissionScope::NetworkCall) {
    auto normalized = NormalizeNetwork(resource, pattern);
    return {std::move(normalized.host), std::move(normalized.execution)};
  }

  auto normalized = Trim(resource);
  if (normalized.empty() || normalized.find_first_of("\r\n") != std::string::npos ||
      normalized.find('\0') != std::string::npos) {
    throw std::invalid_argument("permission resource contains invalid characters");
  }
  if (scope == PermissionScope::ShellExecute &&
      (normalized.find_first_of("&|<>;`^") != std::string::npos ||
       normalized.find("$(") != std::string::npos)) {
    throw std::invalid_argument("shell permission resource contains command composition");
  }
  if (!pattern && HasWildcard(normalized)) {
    throw std::invalid_argument("permission request resource must be exact");
  }
  return {normalized, normalized};
}

std::optional<PermissionDecision> PermissionManager::MatchRule(
    const RequestKey& request) const {
  std::optional<PermissionDecision> result;
  std::tuple<bool, bool, std::size_t> best_specificity{false, false, 0};
  bool has_match = false;

  for (const auto& rule : rules_) {
    if (rule.scope != request.scope || !GlobMatch(rule.tool_pattern, request.tool_name) ||
        !GlobMatch(rule.resource_pattern, request.resource)) {
      continue;
    }
    if (rule.decision == PermissionDecision::Deny) {
      return PermissionDecision::Deny;
    }
    const auto specificity = std::tuple{
        !HasWildcard(rule.tool_pattern), !HasWildcard(rule.resource_pattern),
        LiteralCount(rule.tool_pattern) + LiteralCount(rule.resource_pattern)};
    if (!has_match || specificity >= best_specificity) {
      result = rule.decision;
      best_specificity = specificity;
      has_match = true;
    }
  }
  return result;
}

PermissionManager::RequestKey PermissionManager::SessionKey(RequestKey key) {
  key.invocation_id.clear();
  return key;
}

void PermissionManager::PurgeExpired(std::chrono::steady_clock::time_point now) {
  for (auto iterator = pending_.begin(); iterator != pending_.end();) {
    if (iterator->second.expires_at > now) {
      ++iterator;
      continue;
    }
    const auto invocation_id = iterator->first;
    iterator = pending_.erase(iterator);
    MarkCompleted(invocation_id);
  }
}

void PermissionManager::MarkCompleted(const std::string& invocation_id) {
  pending_.erase(invocation_id);
  if (completed_invocations_.contains(invocation_id)) {
    return;
  }
  if (completed_invocations_.size() >= kMaxCompletedInvocations) {
    completed_invocations_.erase(completed_order_.front());
    completed_order_.pop_front();
  }
  completed_invocations_.insert(invocation_id);
  completed_order_.push_back(invocation_id);
}

void PermissionManager::TouchCompleted(const std::string& invocation_id) {
  const auto iterator = std::ranges::find(completed_order_, invocation_id);
  if (iterator != completed_order_.end()) {
    completed_order_.erase(iterator);
    completed_order_.push_back(invocation_id);
  }
}

}  // namespace harness
