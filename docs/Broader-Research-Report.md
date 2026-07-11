# aknirex's Game Dev Harness —— 广泛调研报告

> 调研日期：2026-07-11
> 覆盖：Agent 架构模式 · Unreal Editor IPC · AI Agent 安全 · Electron+C++ 集成 · 已知漏洞与防护

---

## 一、Agent 架构模式（借鉴 Claude Code / Cursor）

### 1.1 Agent Loop 核心结构

```
用户输入 → 构建上下文 → LLM 流式调用 → 返回结果:
  ├─ 纯文本 → 展示给用户 → 结束
  └─ tool_calls[] → 执行工具 → 结果注入对话 → 回到 LLM 调用（循环）
                       ↑
                  max_turns=30 防止死循环
```

Claude Code 的分层请求结构：
| 层级 | 内容 | 变化频率 |
|------|------|----------|
| System Prompt | 核心指令 + 工具定义 | 版本升级时 |
| Project Context | CLAUDE.md + 项目规则 | 会话开始时 |
| Conversation | 消息 + 工具结果 | 每轮 |

**关键机制**：
- System Prompt + 工具定义使用 Prompt Caching（缓存命中后 token 费用降低 90%）
- 只读工具并行执行，写工具顺序执行
- MCP 工具默认延迟加载——只有工具名在上下文中，完整 schema 按需获取（节省 46.9% token）

### 1.2 上下文窗口管理

**预防为主**：
- 大工具输出写入临时文件，Agent 用 `tail`/`grep` 搜索而非全部加载
- Observation Masking（JetBrains Junie）：旧工具输出标记为 `[masked]`，Agent 可重新读取
- ACON 分层压缩：原始工具输出积极压缩；错误信息和用户指令永不压缩

**当窗口必须压缩时（Claude Code Auto-Compaction）**：
1. 清除旧工具输出
2. LLM 摘要对话历史（有损）
3. 保留最近对话完整，旧对话替换为摘要
4. 压缩防抖：如果压缩后立即又填满，停止自动压缩并报错

### 1.3 成本优化

| 手段 | 节省幅度 | 说明 |
|------|---------|------|
| **模型路由** | 40-70% | 简单任务用小模型 (Haiku)，复杂任务用大模型 (Opus) |
| **上下文压缩** | 50-70% | 删除冗余 token |
| **Prompt Caching** | 60-90% | System Prompt + 工具定义缓存 |
| **子 Agent** | — | 独立上下文执行重任务，主 Agent 只收摘要 |

Claude Code 典型成本：$0.34/会话，$13/开发者/活跃天。

### 1.4 工具 Schema 设计最佳实践

- **描述中包含"何时不用"**："Do NOT use this for X"——比正向描述更能减少误用
- **标记只读工具**：`readOnlyHint: true` → 允许并行执行
- **动态工具加载**：引擎类型检测后只加载对应引擎的工具集（5-10 个/Agent）
- **工具数量控制**：每个 Agent 暴露 5-10 个工具，过多会降低 LLM 选择准确率

### 1.5 错误恢复

- **错误 = 数据**：错误信息作为普通工具结果返回给 LLM，让它自行推理下一步
- **不做自动重试**——Agent 收到错误输出后自己做决策
- **maxToolRetries=2**——同一工具同种错误连续失败 2 次 → 需要用户干预
- **PreToolUse Hook**：执行前验证（如检查 Unity 项目是否已打开）

---

## 二、Unreal Engine Editor Plugin IPC 实现

### 2.1 推荐方案：UEditorSubsystem + WebRemoteControl API

```cpp
// 编辑器中运行的通信端点
UCLASS()
class UMyEditorSubsystem : public UEditorSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override
    {
        // GEditor 此时已可用
        // 启动 HTTP/TCP/Named Pipe 监听
    }
    virtual void Deinitialize() override
    {
        // 清理监听
    }
};
```

UE 内置的 **Remote Control API**（HTTP 端口 30010，WebSocket 端口 30020）已经提供了：
- 调用任何 `BlueprintCallable` 函数
- 读写对象属性
- 搜索 Asset Registry
- 批量操作

**局限**：
- 不能直接创建新资产/导入文件——需要用 `InterchangeManager` 或 `AssetTools` 自己做
- 不能触发 Cooking/Packaging——需要 spawn UAT 子进程
- 不能修改 `.uproject` 或插件配置
- Beta 状态，未来 API 可能变化

### 2.2 资产导入（程序化）

```cpp
// 使用 Interchange Framework (UE 5.3+)
UInterchangeSourceData* SourceData = UInterchangeSourceData::CreateSourceData(FilePath);
FImportAssetParameters ImportParams;
ImportParams.bIsAutomated = true;  // 抑制对话框
UInterchangeManager::GetInterchangeManagerScripted()
    .ImportAsset(DestPath, SourceData, ImportParams);
```

### 2.3 关卡操作

```cpp
UWorld* World = GEditor->GetEditorWorldContext().World();
AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(...);
Actor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
Actor->MarkPackageDirty();
```

### 2.4 构建打包（从 Editor Plugin）

```cpp
// 不能在同进程 run=cook，必须 spawn UAT
FString UATPath = FPaths::EngineDir() / TEXT("Build/BatchFiles/RunUAT.bat");
FString Args = TEXT("BuildCookRun -project=... -platform=Win64 -cook -stage -pak");
FPlatformProcess::CreateProc(*UATPath, *Args, ...);
```

### 2.5 关键注意事项

| 问题 | 对策 |
|------|------|
| **UObject 操作必须在 Game Thread** | 网络 I/O 用工作线程，收到消息后 `AsyncTask(ENamedThreads::GameThread, ...)` 调回主线程 |
| **GC 保护** | 非 UObject 类持有 UObject 指针 → 继承 `FGCObject` 注册引用 |
| **Named Pipe 仅 Windows** | 跨平台用 TCP 127.0.0.1 |
| **Editor Subsystem 默认不 Tick** | UE 5.4+ 支持，旧版用 `FTSTicker::GetCoreTicker()` |
| **Source Control** | 修改资产前 checkout：`ISourceControlModule::Get().GetProvider().Execute(FCheckOut, Package)` |

---

## 三、AI Agent 安全（基于 2025-2026 CVE 和学术研究）

### 3.1 已知攻击向量（成功率 41-84%）

| 向量 | 机制 | 最高成功率 |
|------|------|-----------|
| **Rules 文件注入** | `.cursorrules` / `CLAUDE.md` 中的恶意指令 | 84% (Cursor) |
| **工具描述投毒** | MCP Server 的 tool description 嵌指令 | 85%+ |
| **配置篡改** | Agent 被诱导写入 `autoApprove: true` | — |
| **仓库文件投毒** | README/issues/PR 中隐藏指令 | — |
| **Prompt 注入（通用）** | 项目文件内容被解读为指令 | 41-84% |

### 3.2 防御措施（按有效性排序）

| 防御 | 有效性 | 说明 |
|------|--------|------|
| **StruQ（数据/指令通道分离）** | <2% 攻击成功率 | 结构性分离 prompt 和数据；**最强但实现最复杂** |
| **SecAlign（偏好优化）** | 96%→2% | 训练时优化对抗注入 |
| **能力范围限制** | — | 沙箱 + 显式权限——目前最实用 |
| **指令层级** | Claude 自报 88% | System > User > External 优先级 |
| **输入净化（正则过滤）** | 基本无效 | 任何过滤都可被 Base64/Unicode/分词绕过 |

### 3.3 Claude Code 的安全设计（行业最高标准）

- **无 auto-approve 模式**（auto 模式有独立分类器审查每个操作）
- WebFetch 使用**独立上下文窗口**防止注入
- `curl`/`wget` 默认禁止
- 复杂 Bash 命令在执行前展示自然语言描述
- MCP Server 允许列表在组织级别管理

### 3.4 已知 CVE 一览（2025-2026 关键事件）

| CVE/事件 | 产品 | 严重度 | 攻击方式 |
|----------|------|--------|----------|
| CVE-2025-49150 | Cursor | Critical | MCP → RCE |
| CVE-2025-53773 | GitHub Copilot | High | 配置注入 → 权限提升 |
| CVE-2025-68143 | mcp-server-git | High | 路径遍历 → 任意位置创建 repo |
| CVE-2025-68144 | mcp-server-git | High | CLI 参数注入 → 文件覆盖 |
| CVE-2026-26030 | Semantic Kernel | Critical | `eval()` on AI 控制输入 → RCE |
| CVE-2026-25592 | Semantic Kernel | Critical | 暴露的文件写入工具 → 沙箱逃逸 |
| PocketOS 事件 | Cursor+Claude | 运维事故 | 过度授权 token → 删除生产数据库（9秒） |
| MCP stdio 设计缺陷 | MCP 协议 | 系统性 | 启动命令作为信任边界 |

**共性模式**：工具描述投毒、Rules 文件注入、配置自篡改、路径遍历、未验证的 AI 参数命中 `eval()`/`exec()`。

### 3.5 对 aknirex's Game Dev Harness 的建议

**核心原则**：
1. **LLM 不是安全边界**——模型能控制的每个工具参数都是攻击者的输入
2. **默认拒绝**——所有操作初始 blocked，逐工具、逐路径、逐域名授权
3. **分层防御**——权限规则 + OS 沙箱 + 运行时 Hook + Auto 模式分类器 + 人工确认

**UE 引擎特定风险**：
- Blueprint 节点注入：Agent 可能写入 `Execute Console Command` 等危险节点
- 配置文件篡改：`DefaultEngine.ini` 含 Steam/Oculus/签名密钥
- C++ 代码注入：Agent 生成的 `.h`/`.cpp` 可能含恶意代码
- **必须**：生成的资产变更通过 Source Control changelist，编译在 CI 隔离验证

---

## 四、Electron + C++ 后端进程集成

### 4.1 推荐 IPC 模式：Child Process + stdio NDJSON

所有主流 AI 桌面应用都采用此模式：
- **Claude Desktop**：Electron spawns Rust/C++ 后端，stdio JSON 通信
- **Cursor**：继承 VS Code 架构，AI 后端通过类似机制
- **VS Code**：核心逻辑 JS/TS，C++ 仅用于性能热点（node-pty、native-keymap）——N-API addon 模式不适用于大型 C++ 后端

```js
// Electron main process
const backend = spawn(binPath, [], { stdio: ['pipe', 'pipe', 'inherit'] });
backend.stdout.on('data', (data) => {
    // newline-delimited JSON
    for (const line of buffer.split('\n')) {
        handleResponse(JSON.parse(line));
    }
});
function send(msg) { backend.stdin.write(JSON.stringify(msg) + '\n'); }
```

**推荐理由**：简单、崩溃隔离、C++ 可独立调试、无需管理 HTTP 服务器生命周期。

### 4.2 进程生命周期

- **启动**：`app.on('ready', () => spawn(...))`
- **崩溃重启**：监听 `exit` 事件，`code !== 0` → 1s 后重试，指数退避（1s→2s→4s→max 30s）
- **优雅关闭**：先发 `{type:'shutdown'}` JSON 消息，等待 5s，超时则 `kill()`
- **Windows 注意**：`kill()` 不等同于 SIGTERM——需走应用层 shutdown 协议

### 4.3 打包

```yaml
# electron-builder.yml
extraResources:
  - from: "backend/build/${platform}/${arch}/"
    to: "bin"
```

C++ 编译每个平台，CI matrix 并行构建，electron-builder 按平台打包对应二进制。更新用 `electron-updater` 统一更新（Electron + C++ 一起发布，避免版本不匹配）。

### 4.4 开发工作流

- C++ 后端独立启动：`./backend --debug`
- Electron 前端 Vite HMR 独立热更新
- C++ 变更手动重启后端，Electron 前端变更秒级生效
- 两个进程互不阻塞

### 4.5 性能

| 场景 | stdio JSON 表现 |
|------|----------------|
| LLM API 调用（IO bound，低频） | 完美 |
| 模型管理（文件操作，偶尔大 payload） | 完美——>10MB 时传文件路径而非内联数据 |
| 引擎实时遥测（60fps 高频） | 瓶颈——需额外 Named Pipe + 二进制协议通道 |

---

## 五、补充调研发现（已在技术选型报告基础上新增）

| 领域 | 发现 |
|------|------|
| **System Prompt 结构** | 3 层：核心指令（不变）+ 引擎上下文（会话级缓存）+ 动态状态（每轮注入） |
| **Prompt Caching** | Anthropic 缓存命中的 token 费用降低 90%，需 ≥1024 token 前缀；System Prompt + 工具定义放在前缀 |
| **工具 Schema** | 每个工具描述中必须写"何时 **不** 用"，比正向描述更能防止误调用 |
| **子 Agent 模式** | 3D 资产生成任务 spawn 独立 sub-agent（只暴露 TRELLIS/MoMask 工具），主 Agent 只收摘要 |
| **UE IPC 端点** | UEditorSubsystem 是理想宿主（`Initialize` 时 `GEditor` 已就绪）；Remote Control API 可覆盖 70% 操作 |
| **UE 资产导入** | Interchange Framework (UE5.3+) 是现代化的程序化导入方案，`bIsAutomated=true` 抑制对话框 |
| **UE 构建打包** | 无法在同进程 run=cook——必须 spawn UAT 子进程 |
| **安全红线** | CVE-2025-68143（mcp-server-git 路径遍历）被 Anthropic 直接移除整个工具——宁可删功能不留漏洞 |
| **安全红线** | `eval()`/`exec()` 绝对禁止出现在 LLM 可控参数路径上 |
| **安全红线** | Agent 的 auto-approve 模式必须有独立分类器二次把关——不能仅靠规则 |
| **C++ 后端调试** | 开发模式下 C++ 独立进程运行，VS/LLDB attach 到 PID 调试，与 Electron 完全解耦 |
