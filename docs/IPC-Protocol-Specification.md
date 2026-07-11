# Electron ↔ C++ Core IPC 协议规格

> 版本: 1.0 · 日期: 2026-07-11
> 参考: JSON-RPC 2.0 + LSP (Language Server Protocol) 线格式

---

## 1. 传输层

**通信方式**：stdio（stdin/stdout），NDJSON 帧协议。

Electron 主进程通过 `child_process.spawn()` 启动 C++ Core 进程，Core 在 stdout 上输出响应，从 stdin 读取请求。

stderr 保留给 Core 进程自身的日志输出（spdlog），不参与 IPC。

**线格式**（参考 LSP）：

```
Content-Length: <字节数>\r\n
Content-Type: application/vnd.harness+json; charset=utf-8\r\n
\r\n
<JSON body>
```

- `Content-Length`：必填，JSON 体的 UTF-8 字节数
- `Content-Type`：固定值，用于区分 Harness 协议与其他 stdio 协议
- 每帧以 `\r\n\r\n` 分隔头与体

**编码**：全部 UTF-8。

---

## 2. 消息模型

遵循 JSON-RPC 2.0 语义。每条消息都是单行 JSON，通过帧协议传输。

### 2.1 公共字段

```typescript
interface Message {
  jsonrpc: "2.0";
}

interface RequestMessage extends Message {
  id: number | string;       // 请求 ID，响应中回传
  method: string;             // 方法名
  params?: object;            // 方法参数
}

interface ResponseMessage extends Message {
  id: number | string | null; // 对应请求 ID；null = 无法解析
  result?: any;               // 成功时的返回值
  error?: ResponseError;      // 失败时的错误信息
}

interface NotificationMessage extends Message {
  method: string;             // 方法名（无 id，无响应）
  params?: object;
}

interface ResponseError {
  code: number;
  message: string;
  data?: any;
}
```

### 2.2 消息方向

```
Electron (client) ──Request──→  Core (server)
Electron (client) ←─Response──  Core (server)

Electron (client) ──Notification──→  Core (server)
Electron (client) ←─Notification───  Core (server)   ← 双向
```

Core 也可以向 Electron 发送 Request（如权限确认），此时 Core 充当 client 角色。

---

## 3. 方法定义

### 3.1 生命周期

| Method | 方向 | 类型 | 说明 |
|--------|------|------|------|
| `initialize` | E→C | Request | 握手：交换协议版本与能力 |
| `initialized` | E→C | Notification | 客户端就绪，可开始工作 |
| `shutdown` | E→C | Request | 优雅关闭，停止接受新任务 |
| `exit` | E→C | Notification | 立即退出进程 |

#### initialize

```jsonc
// 请求
{
  "jsonrpc": "2.0", "id": 1, "method": "initialize",
  "params": {
    "protocolVersion": "1.0",
    "clientInfo": { "name": "Harness Electron Shell", "version": "0.1.0" },
    "capabilities": {
      "streaming": true,
      "fileAccess": true
    }
  }
}

// 响应
{
  "jsonrpc": "2.0", "id": 1,
  "result": {
    "protocolVersion": "1.0",
    "serverInfo": { "name": "Harness Core", "version": "0.1.0" },
    "capabilities": {
      "sandboxEngine": true,
      "mcpClient": true,
      "agentLoop": true
    }
  }
}
```

### 3.2 Agent 执行

| Method | 方向 | 类型 | 说明 |
|--------|------|------|------|
| `agent/run` | E→C | Request | 发起 Agent 任务 |
| `agent/cancel` | E→C | Request | 取消当前任务 |
| `agent/stream` | C→E | Notification | LLM 流式 token 输出 |
| `agent/tool-start` | C→E | Notification | 工具调用开始 |
| `agent/tool-end` | C→E | Notification | 工具调用结束 |
| `agent/complete` | C→E | Notification | 任务完成 |
| `agent/error` | C→E | Notification | 任务异常 |

#### agent/run

```jsonc
// 请求
{
  "jsonrpc": "2.0", "id": 2, "method": "agent/run",
  "params": {
    "projectPath": "D:/MyUEProject",
    "engineType": "unreal",
    "prompt": "在场景中创建3个Cube并排成一行",
    "model": "claude-opus-4-8",
    "maxTurns": 30
  }
}

// 响应（同步确认任务已接收，实际结果通过 notification 推送）
{ "jsonrpc": "2.0", "id": 2, "result": { "taskId": "task-001", "accepted": true } }
```

#### agent/stream

```jsonc
// Core 推送 LLM token
{ "jsonrpc": "2.0", "method": "agent/stream", "params": { "taskId": "task-001", "delta": "好的，我来", "type": "text" } }
{ "jsonrpc": "2.0", "method": "agent/stream", "params": { "taskId": "task-001", "delta": "创建...", "type": "text" } }
```

#### agent/tool-start

```jsonc
{ "jsonrpc": "2.0", "method": "agent/tool-start", "params": { "taskId": "task-001", "toolName": "unreal.actor.spawn_from_class", "args": { "class_path": "/Script/Engine.StaticMeshActor" } } }
```

#### agent/tool-end

```jsonc
{ "jsonrpc": "2.0", "method": "agent/tool-end", "params": { "taskId": "task-001", "toolName": "unreal.actor.spawn_from_class", "result": { "success": true, "summary": "已创建 StaticMeshActor_3" } } }
```

#### agent/complete

```jsonc
{ "jsonrpc": "2.0", "method": "agent/complete", "params": { "taskId": "task-001", "summary": "已在场景中创建3个Cube", "turns": 4, "toolsCalled": 3 } }
```

#### agent/error

```jsonc
{ "jsonrpc": "2.0", "method": "agent/error", "params": { "taskId": "task-001", "code": -1, "message": "LLM API 调用返回 429 Too Many Requests" } }
```

### 3.3 工具管理

| Method | 方向 | 类型 | 说明 |
|--------|------|------|------|
| `tool/list` | E→C | Request | 获取注册的工具列表 |

```jsonc
// 请求
{ "jsonrpc": "2.0", "id": 3, "method": "tool/list", "params": { "category": "engine" } }

// 响应
{
  "jsonrpc": "2.0", "id": 3,
  "result": {
    "tools": [
      {
        "name": "unreal.actor.spawn_from_class",
        "description": "在关卡中根据类路径生成 Actor",
        "parameters": {
          "type": "object",
          "properties": {
            "class_path": { "type": "string", "description": "UClass 路径" },
            "location": { "type": "array", "items": { "type": "number" }, "minItems": 3, "maxItems": 3 }
          },
          "required": ["class_path"]
        },
        "category": "engine",
        "dangerLevel": 1
      }
    ]
  }
}
```

### 3.4 权限控制

| Method | 方向 | 类型 | 说明 |
|--------|------|------|------|
| `permission/ask` | C→E | Request | Core 请求 UI 展示授权确认 |
| `permission/grant` | E→C | Notification | 授予某工具的当前会话权限 |

#### permission/ask（Core → Electron 反向请求）

```jsonc
// Core 请求
{ "jsonrpc": "2.0", "id": 100, "method": "permission/ask", "params": { "toolName": "unreal.actor.spawn_from_class", "scope": "EngineModify", "dangerLevel": 1 } }

// Electron 响应
{ "jsonrpc": "2.0", "id": 100, "result": { "decision": "allow_once" } }
// decision: "allow_once" | "allow_session" | "deny"
```

### 3.5 Sandbox 管理

| Method | 方向 | 类型 | 说明 |
|--------|------|------|------|
| `sandbox/status` | E→C | Request | 查询沙箱状态 |
| `sandbox/commit` | E→C | Request | 将沙箱变更提交到项目 |
| `sandbox/revert` | E→C | Request | 回滚沙箱所有变更 |
| `sandbox/changelog` | E→C | Request | 获取变更文件列表 |

#### sandbox/status

```jsonc
// 响应
{ "jsonrpc": "2.0", "id": 5, "result": { "enabled": true, "changes": 3, "isolated": true } }
```

### 3.6 会话与项目

| Method | 方向 | 类型 | 说明 |
|--------|------|------|------|
| `session/create` | E→C | Request | 创建新会话 |
| `session/close` | E→C | Request | 关闭会话 |
| `project/open` | E→C | Request | 打开项目并检测引擎 |
| `project/close` | E→C | Notification | 关闭项目 |

#### project/open

```jsonc
// 请求
{ "jsonrpc": "2.0", "id": 4, "method": "project/open", "params": { "path": "D:/MyUEProject" } }

// 响应
{ "jsonrpc": "2.0", "id": 4, "result": { "engineType": "unreal", "engineVersion": "5.8.0", "mcpServers": ["epic-official", "genorca"] } }
```

---

## 4. 错误码

### 4.1 JSON-RPC 标准错误码

| Code | 含义 |
|------|------|
| -32700 | Parse error——无法解析 JSON |
| -32600 | Invalid Request——不是合法的 JSON-RPC |
| -32601 | Method not found——未知方法 |
| -32602 | Invalid params——参数类型/必填错误 |
| -32603 | Internal error——内部执行错误 |

### 4.2 Harness 自定义错误码（-32000 ~ -32099）

| Code | 含义 |
|------|------|
| -32000 | Agent 执行失败 |
| -32001 | Agent 达到 maxTurns 上限 |
| -32002 | 工具执行超时 |
| -32003 | 工具执行失败 |
| -32004 | 权限拒绝 |
| -32005 | 沙箱不可用 |
| -32006 | MCP Server 连接失败 |
| -32007 | LLM API 调用失败 |
| -32008 | 项目/引擎检测失败 |

---

## 5. 取消支持

使用 JSON-RPC 标准取消机制：发送 `$/cancelRequest` 通知。

```jsonc
{ "jsonrpc": "2.0", "method": "$/cancelRequest", "params": { "id": 2 } }
```

收到取消后，Core 应：
1. 中止当前 LLM API 调用（关闭 HTTP 连接或发送 abort）
2. 停止等待中的工具执行（通过 `ExecutionEngine::CancelAll()`）
3. 通过 SandboxEngine 回滚当前 Agent 任务产生的文件变更
4. 发送 `agent/error` 通知告知取消成功

---

## 6. 流控

- TCP-like 简单流控：Core 连续发送 agent/stream 通知时无需确认
- 单个请求超时时间：30 秒（初始化）/ 无上限（agent/run，持续到完成或取消）
- agent/run 内部 LLM 调用超时：60 秒（单次 HTTP 请求）

---

## 7. 实现约定

### C++ 侧（Core）

```cpp
// ipc_channel.h
// 注：json_value = glz::json_t（glaze 动态 JSON 类型）
using json_value = glz::json_t;

class IpcChannel {
public:
    // 从 stdin 读取消息（阻塞等待下一帧）
    json_value ReadMessage();
    
    // 向 stdout 写入消息（自动添加帧头）
    void WriteMessage(const json_value& msg);
    
    // 写入通知（无 id）
    void Notify(const std::string& method, const json_value& params = {});
    
    // 发送请求并等待响应
    json_value Request(const std::string& method, const json_value& params = {}, 
                       std::chrono::milliseconds timeout = 30s);
};
```

### TypeScript 侧（Electron）

```typescript
// ipc/IpcClient.ts
class HarnessIpcClient {
  private requestId = 0;
  private pendingRequests = new Map<number | string, { resolve, reject }>();
  private handlers = new Map<string, (params) => void>();

  // 启动 Core 进程并建立 stdio 连接
  async start(corePath: string): Promise<InitializeResult>;

  // 发送请求
  async request(method: string, params?: any): Promise<any>;

  // 注册 notification 处理器
  on(method: string, handler: (params: any) => void): void;

  // 发送通知
  notify(method: string, params?: any): void;

  // 关闭
  async shutdown(): void;
}
```

### 编辑器集成测试

用 expect-style 断言验证协议往返：

```cpp
// tests/ipc_protocol_test.cpp
TEST_CASE("Handshake") {
    IpcChannel channel;
    // 模拟 Electron 发送 initialize
    auto response = channel.SendAndReceive(R"({
        "jsonrpc": "2.0", "id": 1, "method": "initialize",
        "params": { "protocolVersion": "1.0" }
    })");
    CHECK(response["result"]["protocolVersion"] == "1.0");
    CHECK(response["result"]["capabilities"]["sandboxEngine"] == true);
}
```
