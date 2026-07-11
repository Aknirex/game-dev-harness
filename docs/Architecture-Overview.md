# aknirex's Game Dev Harness —— 架构概要

> 核心理念：LLM + Harness = Agent
> 技术选型状态：**已确定**（详见 `Tech-Selection-Report.md`）
> 引擎接入策略：**MCP 生态优先**——Harness 是 MCP Client，连接已有的 Unreal/Unity/Godot MCP Server
> Agent 架构：**多 Agent 协作**——Planner / Coder / Asset Agent / Reviewer 分工
> 引擎优先级：Unreal Engine (UE 5.8 内置 MCP) → Unity → Godot
> JSON 库：glaze (C++20 编译期反射), 代码示例中 `nlohmann::json` 代表 `glz::json_t`（动态 JSON 值类型）或 glaze 反射结构体

---

## 1. 系统分层

```
┌────────────────────────────────────────────────────────────────┐
│                     Presentation Layer                          │
│  GUI: ChatView · PipelineView · SandboxReviewView ·            │
│       PluginMarketView · DiffView · FileBrowser                │
│  CLI: agdh run "prompt" · agdh serve                     │
├────────────────────────────────────────────────────────────────┤
│                      Application Layer                          │
│  SessionManager · ProjectManager · SettingsManager             │
│  PluginIndexManager · UpdateManager                             │
├────────────────────────────────────────────────────────────────┤
│                  Multi-Agent Orchestration                      │
│  Orchestrator · PlannerAgent · CoderAgent ·                    │
│  AssetAgent · ReviewerAgent                                    │
│  ┌──────────────────────────────────────────────────┐         │
│  │ AgentLoop · LLMClient · ContextBuilder · Prompt  │         │
│  └──────────────────────────────────────────────────┘         │
├────────────────────────────────────────────────────────────────┤
│                      Harness Layer  ← 核心                      │
│  ToolRegistry · ExecutionEngine · PermissionManager ·          │
│  WorkflowEngine · PluginManager · SandboxEngine                │
├────────────────────────────────────────────────────────────────┤
│                     Integration Layer                            │
│  ┌──────────────┬──────────────┬──────────────────┐           │
│  │ MCP Client   │ AI Pipeline  │ Plugin Host      │           │
│  │ (stdio/HTTP) │ Bridge (IPC) │ (Lua 5.4 + sol2) │           │
│  └──────┬───────┴──────────────┴──────────────────┘           │
├─────────┼──────────────────────────────────────────────────────┤
│         │              External MCP Servers                     │
│  ┌──────┴───────┬──────────────┬──────────────┐               │
│  │ Unreal MCP   │ Unity MCP    │ Godot MCP    │ Wwise MCP     │
│  │ (Epic官方)   │ (GladeKit等) │ (GladeKit)   │               │
│  │ 62-133工具   │ 70-260+工具  │ 105工具      │ 69工具        │
│  └──────────────┴──────────────┴──────────────┴───────────────┘
│         │
│  ┌──────┴──────────────────────────────────────────────────────┐
│  │              AI Pipeline Host (Python)                       │
│  │  TRELLIS · MoMask · RigNet · Blender (Python Process)      │
│  └─────────────────────────────────────────────────────────────┘
```

---

## 2. 模块详细设计

### 2.1 Harness Core（核心框架）

Harness 是整个系统的核心抽象，定义了"LLM 如何获得行动能力"。

#### 2.1.1 Tool 接口

```cpp
// 每个 Tool 的最小接口定义
struct ToolParameter {
    std::string name;
    std::string type;        // "string" | "int" | "float" | "bool" | "enum" | "path"
    std::string description;
    bool required;
    nlohmann::json default_value;
};

struct ToolSchema {
    std::string name;                    // 唯一标识，如 "unity.scene.create_object"
    std::string description;             // LLM 可读的功能描述
    std::vector<ToolParameter> parameters;
    std::string category;                // "engine" | "asset" | "code" | "system" | "ai_3d"
    int danger_level;                    // 0=只读 1=写入项目 2=系统级
};

struct ToolResult {
    bool success;
    std::string summary;                 // 给 LLM 看的文本摘要
    nlohmann::json data;                 // 结构化数据
    std::vector<std::string> artifacts;  // 产出的文件路径列表
    std::string error_detail;            // 失败时的详细信息
};

// 工具处理函数签名
using ToolHandler = std::function<ToolResult(const nlohmann::json& args)>;

class ITool {
public:
    virtual ~ITool() = default;
    virtual ToolSchema GetSchema() const = 0;
    virtual ToolResult Execute(const nlohmann::json& args) = 0;
    virtual void Cancel() = 0;
};
```

#### 2.1.2 ToolRegistry

```cpp
class ToolRegistry {
public:
    // 注册工具
    void RegisterTool(std::unique_ptr<ITool> tool);
    void RegisterLuaTool(const std::string& script_path);

    // 按 category/enabled/danger_level 过滤
    std::vector<const ITool*> QueryTools(
        std::optional<std::string> category = std::nullopt,
        int max_danger = 2
    ) const;

    // 生成 LLM function-calling 格式的 JSON Schema
    std::string GenerateLLMToolsSchema() const;

    // 按名称查找并执行
    ToolResult ExecuteTool(const std::string& name, const nlohmann::json& args);

    // 热加载：从插件目录扫描新工具
    void ScanPluginDirectory(const std::filesystem::path& dir);

private:
    std::unordered_map<std::string, std::unique_ptr<ITool>> tools_;
};
```

#### 2.1.3 ExecutionEngine

```cpp
// 单个工具调用记录
struct ToolInvocation {
    std::string tool_name;
    nlohmann::json arguments;
    std::chrono::milliseconds timeout;
    int max_retries;
};

// 批量执行结果
struct BatchExecutionResult {
    std::vector<ToolResult> results;
    std::vector<std::string> error_tools;  // 失败的工具名
    std::chrono::milliseconds total_time;
};

class ExecutionEngine {
public:
    // 单工具同步调用（带超时+重试）
    ToolResult Execute(const ToolInvocation& invocation);

    // 批量并行调用（自动识别无依赖的工具并行执行）
    BatchExecutionResult ExecuteParallel(
        const std::vector<ToolInvocation>& invocations,
        int max_concurrency = 4
    );

    // Pipeline 执行（按 DAG 依赖顺序）
    BatchExecutionResult ExecutePipeline(
        const std::vector<ToolInvocation>& invocations,
        const std::vector<std::pair<int, int>>& dependencies  // from -> to
    );

    // 取消所有运行中的工具
    void CancelAll();

private:
    std::unique_ptr<ThreadPool> pool_;
    std::shared_ptr<ResourceLockManager> lock_manager_;
};
```

#### 2.1.4 PermissionManager

```cpp
enum class Permission { Allow, Deny, Ask };

enum class PermissionScope {
    FileRead,     // 读取项目文件
    FileWrite,    // 修改项目文件
    ShellExecute, // 执行外部命令
    NetworkCall,  // 发送网络请求
    EngineModify, // 修改引擎中的对象
};

class PermissionManager {
public:
    // 批量策略
    void SetPolicy(PermissionScope scope, Permission policy);

    // 逐工具授权
    void GrantTool(const std::string& tool_name);
    void RevokeTool(const std::string& tool_name);

    // 执行前检查；返回 false 则需用户交互确认
    bool CheckPermission(const std::string& tool_name, PermissionScope scope);

    // "本次会话始终允许"
    void GrantSession(const std::string& tool_name);

private:
    std::unordered_map<PermissionScope, Permission> policies_;
    std::unordered_set<std::string> granted_tools_;
    std::unordered_set<std::string> session_granted_;
};
```

---

### 2.2 Agent Layer（智能层）

#### 2.2.1 AgentLoop

```cpp
class AgentLoop {
public:
    // 主循环：持续对话直到任务完成或用户中断
    void Run(const std::string& user_message);

    // 单步：LLM 思考 → 决定是否调 tool → 执行 → 拿结果
    AgentStepResult Step();

    // 回调：通知 UI 更新
    void SetCallbacks(
        std::function<void(const std::string& thinking)> on_thinking,
        std::function<void(const ToolInvocation&)> on_tool_start,
        std::function<void(const ToolResult&)> on_tool_done,
        std::function<void(const std::string& response)> on_response
    );

private:
    std::unique_ptr<ILLMClient> llm_;
    std::unique_ptr<ContextBuilder> context_;
    std::shared_ptr<ExecutionEngine> engine_;
    std::shared_ptr<PermissionManager> permissions_;

    ConversationHistory history_;
    int max_turns_ = 50;  // 防止死循环
};
```

#### 2.2.2 LLMClient 接口

```cpp
struct LLMMessage {
    enum Role { System, User, Assistant, Tool };
    Role role;
    std::string content;
    std::optional<std::string> tool_call_id;
    std::optional<std::string> tool_name;
};

struct LLMResponse {
    std::optional<std::string> text;                       // 纯文本回复
    std::vector<ToolInvocation> tool_calls;                // 要求调用的工具
    std::optional<std::string> finish_reason;              // "stop" | "tool_calls" | "length"
};

class ILLMClient {
public:
    virtual ~ILLMClient() = default;

    // 流式调用（UI 实时显示思考过程）
    virtual void ChatStream(
        const std::vector<LLMMessage>& messages,
        const std::string& tools_schema,
        std::function<void(const std::string& delta)> on_text,     // 逐字输出
        std::function<void(const ToolInvocation&)> on_tool_call,   // tool call 出现时
        std::function<void(const LLMResponse&)> on_complete
    ) = 0;

    // 非流式调用（用于不需要展示思考过程的场景）
    virtual LLMResponse Chat(const std::vector<LLMMessage>& messages,
                             const std::string& tools_schema) = 0;

    virtual std::string ProviderName() const = 0;
};

// 具体实现
class OpenAIClient : public ILLMClient { /* ... */ };
class AnthropicClient : public ILLMClient { /* ... */ };
class OllamaClient : public ILLMClient { /* ... */ };
```

#### 2.2.3 ContextBuilder

```cpp
class ContextBuilder {
public:
    // 构建 system prompt + 项目上下文
    std::string BuildSystemPrompt(
        const std::string& engine_type,      // "unity" | "unreal" | "godot"
        const std::filesystem::path& project_root,
        const std::vector<std::string>& user_preferences
    );

    // 项目摘要：文件树（截断）、关键文件摘要、最近变更
    std::string BuildProjectSummary(const std::filesystem::path& project_root);

    // 智能截断：保留最近 N 轮对话 + 工具高价值结果，丢弃中间冗余
    std::vector<LLMMessage> CompressHistory(
        const std::vector<LLMMessage>& full_history,
        int max_tokens = 64000
    );

private:
    std::unique_ptr<FileIndex> index_;       // 轻量文件索引
    std::unique_ptr<ITokenCounter> counter_; // Token 计数
};
```

---

### 2.3 MCP Client —— 引擎与工具接入（替代原 Engine Adapter）

> **战略决策**：Harness 不建自定义引擎 IPC，作为 MCP Client 连接已有的 MCP Server 生态。

#### 2.3.1 MCP 协议客户端

```cpp
// MCP 传输类型
enum class MCPTransport { Stdio, HTTP_Sse };

// MCP Server 连接配置
struct MCPServerConfig {
    std::string name;                    // "unreal-mcp"
    std::string engine_type;            // "unreal" | "unity" | "godot"
    MCPTransport transport;
    std::string command_or_url;         // stdio: 启动命令；HTTP: localhost URL
    std::vector<std::string> args;      // stdio 模式的命令行参数
    bool auto_connect;                  // 是否自动连接
    int connect_timeout_ms;
};

// MCP Client —— 连接一个 MCP Server
class IMCPClient {
public:
    virtual ~IMCPClient() = default;

    // 连接/断连
    virtual bool Connect(const MCPServerConfig& config) = 0;
    virtual void Disconnect() = 0;
    virtual bool IsConnected() const = 0;

    // MCP 协议方法
    virtual std::vector<ToolSchema> ListTools() = 0;       // tools/list
    virtual std::vector<MCPResource> ListResources() = 0;  // resources/list
    virtual std::string ListPrompts() = 0;                  // prompts/list
    virtual ToolResult CallTool(const std::string& tool_name,
                                const nlohmann::json& args) = 0;  // tools/call

    // 将 MCP tools 注册为 Harness Tools
    virtual void RegisterTools(ToolRegistry& registry,
                               const std::string& namespace_prefix = "") = 0;
};

// MCP Client Manager —— 管理多个 MCP Server 连接
class MCPClientManager {
public:
    void AddServer(const MCPServerConfig& config);
    void RemoveServer(const std::string& name);
    void ConnectAll();
    void DisconnectAll();

    // 按引擎类型获取工具
    std::vector<const ITool*> GetToolsForEngine(const std::string& engine_type);

    // 根据项目根目录自动检测引擎类型，加载对应的 MCP Server
    std::string DetectAndConnect(const std::filesystem::path& project_root);

private:
    std::unordered_map<std::string, std::unique_ptr<IMCPClient>> clients_;
    std::unordered_map<std::string, MCPServerConfig> configs_;
};
```

#### 2.3.2 预置 MCP Server 配置

Harness 内置以下 MCP Server 的"连接配方"（配置模板），用户只需确认引擎路径：

| MCP Server | 引擎 | 传输 | 默认端点 |
|-----------|------|------|----------|
| **Epic Unreal MCP** (UE 5.8 内置) | Unreal | HTTP | localhost:8000/mcp |
| **IvanMurzak/Unreal-MCP** | Unreal | stdio | .NET sidecar 进程 |
| **ultimate-unreal-engine-mcp** | Unreal | stdio | C++ Editor Plugin |
| **GladeKit MCP for Unity** | Unity | HTTP | localhost:8080/mcp |
| **akiojin/unity-editor-mcp** | Unity | stdio | — |
| **GladeKit MCP for Godot** | Godot | HTTP | localhost:8090/mcp |
| **Wwise-MCP** | 中间件 | HTTP | WAAPI bridge |

#### 2.3.3 MCP 工具命名空间

```cpp
// 工具注册时自动添加命名空间前缀
// unreal.scene.spawn_actor     (来自 Unreal MCP)
// unity.prefab.create           (来自 Unity MCP)
// godot.node.add_child          (来自 Godot MCP)
// wwise.sound.create_object     (来自 Wwise MCP)
```

Agent 根据当前项目引擎类型，只加载对应命名空间的工具集（每个 Agent 5-10 个工具），避免工具列表过长导致 LLM 选择准确率下降。

---

### 2.4 Multi-Agent Orchestration（多 Agent 协作调度）

#### 2.4.1 Agent 角色与接口

```cpp
enum class AgentRole {
    Orchestrator,  // 接收用户意图，分解任务，调度子 Agent
    Planner,       // 分析任务，制定执行计划（DAG）
    Coder,         // 生成/修改代码，编译验证
    Asset,         // AI 3D 管线编排，资产导入
    Reviewer       // 验证变更：编译、资产合法性、意图对齐
};

// 子任务定义
struct SubTask {
    std::string task_id;
    std::string description;
    AgentRole assigned_role;
    std::vector<std::string> depends_on;   // 依赖的 task_id
    nlohmann::json input_context;          // 任务入参
    nlohmann::json output_result;          // 执行结果
    enum Status { Pending, Running, Done, Failed } status;
};

// Agent 基类
class IAgent {
public:
    virtual ~IAgent() = default;
    virtual AgentRole Role() const = 0;

    // 每个 Agent 有自己的工具集和 LLM 配置
    virtual void Configure(
        std::vector<const ITool*> tools,   // 该 Agent 可用的工具
        std::shared_ptr<ILLMClient> llm,   // 可配置不同模型
        std::string system_prompt
    ) = 0;

    // 执行子任务，返回结果
    virtual SubTask Execute(const SubTask& task) = 0;
};

// Orchestrator —— 主调度器
class Orchestrator {
public:
    void RegisterAgent(std::unique_ptr<IAgent> agent);

    // 接收用户意图，分解为子任务 DAG，调度执行
    std::vector<SubTask> ExecuteUserIntent(const std::string& user_prompt);

private:
    std::unordered_map<AgentRole, std::unique_ptr<IAgent>> agents_;

    // 用 LLM 分解用户意图为子任务 DAG
    std::vector<SubTask> DecomposeIntent(const std::string& user_prompt);

    // 按 DAG 依赖顺序调度执行
    void ScheduleAndExecute(std::vector<SubTask>& tasks);
};
```

#### 2.4.2 Agent 间通信

```
User: "生成一个骑士角色并放入场景"
    ↓
Orchestrator → Planner: 分析任务 → 输出 DAG:
    [生成3D模型] ──┐
    [生成动画]   ──┤→ [导入引擎] → [放入场景] → [验证]
    ↓
Orchestrator → Asset Agent: 并行执行 (生成模型 + 生成动画)
Orchestrator → Coder Agent: 导入引擎 + 放入场景 (等 Asset 完成)
Orchestrator → Reviewer Agent: 验证 (等 Coder 完成)
    ↓
Orchestrator → User: "已完成。骑士模型 (2342面, PBR贴图)，
     走路动画 (30fps, 2s)，已放入 Unreal 关卡。"
```

#### 2.4.3 上下文隔离与共享

- 每个子 Agent 有**独立的 LLM 上下文窗口**（通过 Prompt Caching 降低重复 Token 费用）
- System Prompt 分三层：核心指令（所有 Agent 共享）→ 角色特定指令（如 Coder 的 UE C++ 规范）→ 任务上下文（每个子任务独立）
- Agent 间通过 Orchestrator 的结构化 JSON 传递子任务和结果——**不共享原始对话上下文**

C++ ↔ Python 的桥接方案：

```
C++ Harness ←→ Shared Memory (data) + Named Pipe (control) ←→ Python Pipeline Host
                             ├─ TRELLIS Service (图/文→3D)
                             ├─ MoMask Service (文→动画)
                             ├─ RigNet Service (自动绑定)
                             ├─ Texturing Service (贴图生成)
                             └─ Blender Service (格式转换/渲染)
```

```cpp
struct AIPipelineRequest {
    std::string task_type;          // "text_to_3d" | "image_to_3d" | "text_to_anim" | "auto_rig"
    nlohmann::json parameters;      // 任务特定参数
    std::filesystem::path output_dir;
};

struct AIPipelineProgress {
    std::string stage;              // "downloading_model" | "generating" | "post_processing" | "done"
    float progress;                 // 0.0 - 1.0
    std::optional<std::string> preview_image;  // Base64 预览图
};

class AIPipelineClient {
public:
    // 异步请求 AI 管线任务
    std::future<AIPipelineResult> SubmitTask(const AIPipelineRequest& request);

    // 查询任务进度
    AIPipelineProgress GetProgress(const std::string& task_id);

    // 取消任务
    void CancelTask(const std::string& task_id);

    // 检查 Python 环境和 GPU 状态
    struct GPUStatus {
        bool available;
        std::string device_name;
        int vram_total_mb;
        int vram_free_mb;
    };
    GPUStatus CheckGPUStatus();

private:
    std::unique_ptr<ipc::SharedMemoryChannel> data_channel_;
    std::unique_ptr<ipc::NamedPipeChannel> control_channel_;
    std::unordered_map<std::string, std::future<AIPipelineResult>> tasks_;
};
```

---

### 2.5 Plugin System（插件系统）

支持两种插件类型：

**A. 原生编译插件（.dll / .so / .dylib）**
- 实现 `IPlugin` 接口
- 可以注册新 Tool、新 Engine Adapter、新 LLM Provider、新 MCP Server
- 最高性能和能力

**B. 脚本插件**
- 通过脚本语言绑定 Harness API：**Lua 5.4 + sol2**（已选定）
- 沙箱升级路径：Luau（Roblox 的 Lua 分支，提供渐进类型 + 细粒度沙箱）
- 热加载、无编译、安全沙箱
- 适合社区贡献简单工具

```cpp
class IPlugin {
public:
    virtual ~IPlugin() = default;
    virtual std::string Name() const = 0;
    virtual std::string Version() const = 0;
    virtual std::string Category() const = 0;  // "engine" | "llm_provider" | "mcp" | "ai_service" | "tool" | "theme"

    // 向 Harness 注册能力
    virtual void Register(PluginContext& ctx) = 0;

    // 插件配置 schema（JSON Schema 格式），Harness 设置面板据此生成配置 UI
    virtual std::string ConfigSchema() const = 0;

    // 卸载时清理
    virtual void Unregister() = 0;
};

struct PluginContext {
    ToolRegistry* tool_registry;
    EngineAdapterManager* engine_adapters;
    // 插件可访问的 Harness 能力
};
```

### 2.6 Plugin Marketplace（插件市场）

Harness 本身不托管插件二进制文件，采用去中心化模式。

```cpp
// 插件索引条目（一个插件在索引中的描述）
struct PluginIndexEntry {
    std::string name;              // 唯一标识
    std::string version;           // SemVer
    std::string author;
    std::string description;
    std::string category;          // "engine" | "llm_provider" | "mcp" | "ai_service" | "tool" | "theme"
    std::string license;           // SPDX 标识
    std::vector<std::string> dependencies;  // 依赖的其他插件名
    std::string download_url;      // 外部托管地址（GitHub Release URL / Git 仓库）
    std::string checksum;          // SHA256
    std::string min_harness_version;  // 最低 Harness Core API 版本
    bool is_official;              // 是否为官方（第一方）插件
    int downloads;                 // 下载量（由索引服务统计）
    float rating;                  // 评分
};

// 插件索引文件（JSON/YAML）
// Harness 启动时从远程拉取，缓存在本地
struct PluginIndex {
    std::string index_version;
    std::string update_url;        // 索引文件的获取地址
    std::vector<PluginIndexEntry> plugins;
};

class PluginIndexManager {
public:
    // 从远程拉取插件索引
    void FetchIndex(const std::string& index_url);

    // 搜索/浏览插件
    std::vector<PluginIndexEntry> Search(
        std::optional<std::string> category = std::nullopt,
        std::optional<std::string> keyword = std::nullopt
    );

    // 安装插件：从 download_url 下载 → 校验 SHA256 → 解压到插件目录 → 加载
    std::future<bool> InstallPlugin(const std::string& plugin_name);

    // 更新/卸载
    std::future<bool> UpdatePlugin(const std::string& plugin_name);
    bool UninstallPlugin(const std::string& plugin_name);

    // 检查已安装插件的更新
    std::vector<PluginIndexEntry> CheckUpdates();

    // 添加私有索引源（企业内网使用）
    void AddPrivateIndex(const std::string& index_url);

private:
    std::vector<std::string> index_urls_;  // 支持多个索引源
    std::unique_ptr<PluginIndex> cached_index_;
};
```

**工作原理**：
1. Harness 内置一个或多个"插件索引 URL"（官方索引 + 用户可添加的私有索引）
2. 启动时或定时从这些 URL 拉取 JSON/YAML 索引文件（官方索引托管于 GitHub 仓库）
3. 用户在应用内浏览、搜索、安装插件
4. 安装时从 `download_url`（**GitHub Release** 为主要托管平台）下载插件包
5. 校验 SHA256，解压到本地插件目录，调用 PluginManager 加载

### 2.7 AI 3D Pipeline Bridge（AI 资产管线桥接）

Harness 通过 IPC 桥接 Python 进程中的 AI 模型（TRELLIS、MoMask 等），封装为 Harness Tool 供 Asset Agent 调用。详见 `Tech-Selection-Report.md` 的 IPC 方案选型（Shared Memory + Named Pipe 混合）。

---

## 3. 数据流示例（多 Agent 架构）

### 场景："生成一个 Low-Poly 骑士，带走路动画，导入 Unreal"

```
User Input: "生成一个 Low-Poly 骑士，带走路动画，导入 Unreal"
    │
    ▼
┌─────────────────────────────────────────────────────┐
│ Orchestrator                                        │
│                                                      │
│  接收用户意图 → 调 Planner 分解任务:                  │
│                                                      │
│  输出 DAG:                                           │
│                                                      │
│  [Asset: 生成3D模型] ──────────────────────┐        │
│         │                                  │        │
│         ├── [Asset: 绑定骨骼] ─────────────┤        │
│         │                                  │        │
│  [Asset: 生成走路动画] ────────────────────┤        │
│                                            │        │
│                                    [Coder: 导入+场景] │
│                                            │        │
│                                    [Reviewer: 验证]  │
│                                                      │
│  调度执行:                                           │
│    Step 1 (并行):                                     │
│      Asset Agent → TRELLIS → knight.glb      (16s)    │
│      Asset Agent → MoMask → walk.bvh          (2s)    │
│    Step 2 (串行):                                     │
│      Asset Agent → RigNet → knight_rigged.glb (3s)    │
│      Asset Agent → Blender retarget           (1s)    │
│    Step 3 (串行):                                     │
│      Coder Agent → MCP Unreal → ImportAsset           │
│      Coder Agent → MCP Unreal → SpawnActorInLevel     │
│    Step 4:                                            │
│      Reviewer Agent → 编译检查 ✓                       │
│      Reviewer Agent → 资产可加载 ✓                     │
│                                                      │
│  Orchestrator → User: "已完成。骑士模型 (2342面),     │
│    走路动画 (30fps, 2s)，已放入 Unreal 关卡。"        │
└─────────────────────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────────────────────┐
│ ExecutionEngine                                      │
│                                                      │
│  Pipeline DAG:                                       │
│                                                      │
│  [generate_3d_model] ──────────────────────┐         │
│         │                                  │         │
│         ├── [auto_rig] ── [import_to_unreal]         │
│         │                                  │         │
│  [text_to_animation] ─────────────────────┘          │
│                                                      │
│  Step 1 (并行, ~16s):                                │
│    A: Core → IPC → Python TRELLIS → knight.glb      │
│    B: Core → IPC → Python MoMask → walk.bvh          │
│                                                      │
│  Step 2 (串行, ~5s):                                 │
│    Core → IPC → Python RigNet(knight.glb)            │
│           → knight_rigged.glb                        │
│           → Blender: BVH retarget → knight_anim.fbx  │
│                                                      │
│  Step 3 (串行, ~3s):                                 │
│    Core → IPC → Unreal Editor Plugin                 │
│           → ImportAsset("knight_anim.fbx")           │
│           → CreateAnimationBlueprint(...)             │
│                                                      │
│  总计: ~24s                                          │
└─────────────────────────────────────────────────────┘
```

---

## 4. 技术选型（已确定，详见 `Tech-Selection-Report.md`）

| 层级 | 选型 | 依据 |
|------|------|------|
| **GUI 框架** | Electron + React | Claude/Cursor/Windsurf 全部采用；Chat UI 组件在 Web 生态成熟 |
| **核心语言** | C++20/23 | 性能 + UE 生态兼容 |
| **MCP SDK** | cxxmcp (C++17, MIT) | 448/448 客户端测试通过, 零运行时依赖 |
| **HTTP Client** | cpp-httplib | 内置 SSE Client, header-only |
| **JSON** | glaze (C++20 reflection) | 比 nlohmann/json 快 10-15x, 编译期反射 |
| **IPC 方案** | Shared Memory + Named Pipe | Core↔Python: Shared Memory (大数据) + Named Pipe (信令, ~27µs RTT) |
| **脚本语言** | Lua 5.4 + sol2 | 专为 C/C++ 嵌入设计, 16µs VM 启动 (沙箱升级路径: Luau) |
| **插件市场** | GitHub Release 托管 + 官方 JSON 索引 | 去中心化, 类 Homebrew 模式 |
| **构建系统** | xmake 3.0+ (xrepo 包管理) | 现代 C++ 构建系统，内置包管理 |
| **测试** | doctest | 编译最快 (Catch2 的 10-20x) |
| **日志** | spdlog | 异步, OBS Studio 同款 |
| **异步 I/O** | ASIO + C++20 coroutines | 成熟异步框架 |
| **CPU 并行** | Taskflow | DAG 任务调度, MIT 许可 |
| **崩溃报告** | sentry-native + Crashpad (C++) / @sentry/electron (JS) | 独立进程崩溃隔离 |
| **自动更新** | electron-updater + GitHub Releases | NSIS 差分更新 ~10-25MB/次 |

---

## 5. 目录结构建议

```
aknirex-game-dev-harness/
├── xmake.lua
├── src/
│   ├── app/                    # 应用入口 + GUI
│   │   ├── main.cpp
│   │   ├── MainWindow.h/cpp
│   │   └── ui/                 # UI 资源（框架特定）
│   ├── harness/                # Harness Core
│   │   ├── tool_registry.h/cpp
│   │   ├── execution_engine.h/cpp
│   │   ├── permission_manager.h/cpp
│   │   ├── sandbox_engine.h/cpp       # ★ P0 Sandbox 隔离执行
│   │   ├── workflow_engine.h/cpp
│   │   └── plugin_manager.h/cpp
│   ├── mcp/                    # MCP Client（引擎接入核心）
│   │   ├── mcp_client.h/cpp          # MCP 协议客户端
│   │   ├── mcp_client_manager.h/cpp  # 多 MCP Server 管理
│   │   ├── mcp_tool_adapter.h/cpp    # MCP Tool → Harness Tool 映射
│   │   └── mcp_server_presets.json   # 预置 MCP Server 配置模板
│   ├── agent/                  # Agent Layer
│   │   ├── orchestrator.h/cpp        # ★ 多 Agent 调度器
│   │   ├── agents/
│   │   │   ├── planner_agent.h/cpp
│   │   │   ├── coder_agent.h/cpp
│   │   │   ├── asset_agent.h/cpp
│   │   │   └── reviewer_agent.h/cpp
│   │   ├── agent_loop.h/cpp
│   │   ├── llm_client.h             # ILLMClient 接口
│   │   ├── context_builder.h/cpp
│   │   └── prompt_templates.h
│   ├── plugin_marketplace/     # 插件市场
│   │   ├── plugin_index.h/cpp
│   │   ├── index_manager.h/cpp
│   │   ├── install_manager.h/cpp
│   │   └── official_index.json
│   ├── ai_pipeline/            # AI 3D Pipeline Bridge
│   │   ├── pipeline_bridge.h/cpp
│   │   └── ipc/                # Shared Memory + Named Pipe 实现
│   └── common/                 # 公共基础库
│       ├── json_utils.h
│       ├── ipc.h
│       └── logging.h
├── plugins/                    # 第一方 + 第三方插件安装目录
│   ├── llm-provider-openai/
│   ├── llm-provider-anthropic/
│   ├── llm-provider-ollama/
│   ├── mcp-server-configs/     # MCP Server 连接配置模板
│   └── ai-trellis-service/
├── python_pipeline/            # Python AI Pipeline Host
│   ├── server.py
│   ├── trellis_service.py
│   ├── momask_service.py
│   └── requirements.txt
├── plugin_index/               # 插件索引文件（发布用）
│   └── official_index.json
├── tests/                      # 单元测试 + 集成测试 + Agent 评估
└── docs/                       # 文档
```
