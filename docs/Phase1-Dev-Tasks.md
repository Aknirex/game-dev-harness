# Phase 1 开发任务拆解

> 版本: 1.0 · 日期: 2026-07-11
> 目标: 单 Agent + MCP Client + Sandbox + 基础 Electron UI
> 预估: 10 周 (2 开发者——1 C++/1 前端)

---

## 1. 依赖关系总览

```
W1: CMake 骨架 (1) ──┬── Electron 骨架 (2) ── IPC 帧协议 (3)
                      │
W2:                  Tool 接口 (4)
                      └── ToolRegistry (5)
                           │
W3:              PermissionManager (6)    SandboxEngine (7) ── 可逆回滚 (8)
                           │                    │
W4-5:                  MCP Client (9)          │
                      └── MCP 映射 (10)        │
                           └── 连接配方 (11)   │
                                │              │
W6-7:                     ILLMClient (13) ─────┤
                      ┌── OpenAI (14)           │
                      ├── Anthropic (15)        │
                      └── 集成测试 (16)          │
                           │                   │
W8:                   AgentLoop (17) ←─────────┘
                      └── ContextBuilder (18)
                           └── E2E Agent 测试 (19)
                                │
W9-10:                     IPC 完整实现 (20)
                           └── Chat UI (21) ── Permission UI (22)
                                └── E2E 联调 (23)
```

---

## 2. WBS (Work Breakdown Structure)

### Week 1 — 项目骨架

| ID | 任务 | 估时 | 依赖 | 产出 | 验证标准 |
|----|------|------|------|------|----------|
| 1 | CMake 项目骨架 + vcpkg manifest | 2d | — | `CMakeLists.txt`, `CMakePresets.json`, `vcpkg.json` | `cmake --build .` 编译成功，Hello World 可运行 |
| 2 | Electron 骨架 | 1d | — | Vite + React + TypeScript 项目，`child_process.spawn()` 启动 Core | `npm run dev` 启动窗口，Core 进程日志可显示 |
| 3 | stdio IPC 帧协议实现 | 2d | 1, 2 | C++ 端 `ipc_channel.h/cpp`，TS 端 `IpcClient.ts` | 双向发送/接收单条消息测试通过 |

### Week 2 — 核心抽象

| ID | 任务 | 估时 | 依赖 | 产出 | 验证标准 |
|----|------|------|------|------|----------|
| 4 | Tool 接口定义 | 1d | 1 | `tool.h`——ToolSchema / ToolResult / ITool 纯虚类 | 编译通过，glaze 反射结构体可序列化 |
| 5 | ToolRegistry | 2d | 4 | `tool_registry.h/cpp`——注册/查询/过滤/生成 LLM schema | 单元测试：注册 10 个 mock tool，按 category/danger 过滤 |
| 6 | PermissionManager | 1d | 1 | `permission_manager.h/cpp`——逐工具/逐 scope/会话级授权 | 单元测试：默认拒绝、单次允许、会话级允许 |

### Week 3 — Sandbox 引擎 (P0)

| ID | 任务 | 估时 | 依赖 | 产出 | 验证标准 |
|----|------|------|------|------|----------|
| 7 | SandboxEngine——项目副本 + 文件操作拦截 | 3d | 1 | `sandbox_engine.h/cpp`——创建隔离副本，文件写操作重定向 | 集成测试：在 Sandbox 中创建文件，原项目不受影响 |
| 8 | 可逆回滚机制 | 2d | 7 | 操作日志（JSONL）+ commit/revert | 集成测试：执行 5 次文件修改后一键 revert，文件内容恢复到初始状态 |

### Week 4-5 — MCP 接入

| ID | 任务 | 估时 | 依赖 | 产出 | 验证标准 |
|----|------|------|------|------|----------|
| 9 | MCP Client 核心 | 3d | 1 | `mcp_client.h/cpp`——集成 cxxmcp，支持 stdio + Streamable HTTP | can list tools from a real MCP server |
| 10 | MCP→Harness Tool 自动映射 | 2d | 5, 9 | `mcp_tool_adapter.h/cpp`——MCP tools/list → ToolRegistry | mock MCP server 返回 5 个 tool，ToolRegistry 中可查 |
| 11 | Unreal MCP 连接配方 | 2d | 9 | `mcp_server_presets.json`——Epic 官方 + GenOrca 配置 | 启动 UE 编辑器后，Core 自动检测并连接 MCP Server |
| 12 | MCP 集成测试 | 2d | 10, 11 | 测试套件 | mock MCP server → tools/list → ToolRegistry 全流程通过 |

### Week 6-7 — LLM 接入

| ID | 任务 | 估时 | 依赖 | 产出 | 验证标准 |
|----|------|------|------|------|----------|
| 13 | ILLMClient 接口 + SSE 流式解析 | 2d | 1 | `llm_client.h`，cpp-httplib SSE 解析器 | 可解析 OpenAI 格式 SSE 流 |
| 14 | OpenAIClient（含 tool calls + streaming） | 2d | 13 | `openai_client.h/cpp` | mock OpenAI API，验证 tool call 往返 + 流式文本 |
| 15 | AnthropicClient（Messages API + tool use + prompt caching） | 2d | 13 | `anthropic_client.h/cpp` | mock Anthropic API，验证 tool_use 格式 + cache_control |
| 16 | LLM Provider 集成测试 | 1d | 14, 15 | 测试套件 | 所有 Provider 的 mock 测试通过 |

### Week 8 — Agent Loop

| ID | 任务 | 估时 | 依赖 | 产出 | 验证标准 |
|----|------|------|------|------|----------|
| 17 | AgentLoop（思考→调工具→拿结果→循环，max_turns=30） | 3d | 5, 6, 13 | `agent_loop.h/cpp` | mock LLM + mock tool，验证多轮循环 |
| 18 | ContextBuilder（System Prompt + 项目摘要 + 历史压缩） | 2d | 17 | `context_builder.h/cpp` | 输入 100 轮历史，输出压缩后 <64000 tokens |
| 19 | 端到端 Agent 测试 | 2d | 17, 18 | 测试套件 | mock LLM 返回 3 次 tool call，验证完整 Agent Loop 正确执行 |

### Week 9-10 — Electron 集成 + 联调

| ID | 任务 | 估时 | 依赖 | 产出 | 验证标准 |
|----|------|------|------|------|----------|
| 20 | IPC 协议完整实现 | 3d | 3, 17 | `initialize` + `agent/run` + `agent/stream` 等全部 18 个方法实现 | 两端 IPC 方法调用测试通过 |
| 21 | Chat UI | 3d | 20 | react-markdown + Shiki 代码高亮 + 流式字幕 + Monaco diff 视图 | 输入 prompt 后实时显示 Agent 回复 |
| 22 | Permission 确认 UI | 1d | 6, 20 | 弹窗组件——显示工具名、danger level、允许/拒绝/记住 | UI 弹窗正确响应 Core 的 permission/ask 请求 |
| 23 | 端到端联调 | 3d | 11, 19, 21 | Electron → Core → Unreal MCP → Agent Loop 全链路 | 在 UE 编辑器中用自然语言创建 Actor 成功 |

---

## 3. 关键路径 (Critical Path)

```
1 → 4 → 5 → 9 → 10 → 11 → 17 → 18 → 19 → 20 → 21 → 23
```

总长: 1 + 1 + 2 + 3 + 2 + 2 + 3 + 2 + 2 + 3 + 3 + 3 ≈ 27 个工作日 ≈ 5.4 周（理想情况）
实际乘以缓冲系数 1.5x ≈ 8 周核心路径 + 2 周缓冲 = 10 周

### 风险路径与缓解

| 风险 | 影响任务 | 缓解措施 |
|------|----------|----------|
| Sandbox (7-8) 未完成 | 依赖 17 但非必需——Agent 测试可用测试项目 | Sandbox 仅影响"隔离执行"，Agent Loop 的开发可用独立测试项目 |
| cxxmcp 集成问题 (9) | 阻塞 10-12 | 备选：直接调用 MCP Server HTTP/stdio 端点，不依赖 cxxmcp |
| Unreal MCP 连接故障 (11) | 阻塞 23 | mock Unreal MCP Server 做联调，不依赖真实 UE 实例 |

---

## 4. 里程碑

| 里程碑 | 周 | 交付物 |
|--------|-----|--------|
| M0: 骨架可跑 | W1 末 | CMake 编译 + Electron 窗口 + IPC 往返 |
| M1: 核心抽象就绪 | W2 末 | ToolRegistry + PermissionManager + 单元测试 |
| M2: Sandbox 可用 | W3 末 | 隔离执行 + 可逆回滚 |
| M3: MCP 联调通过 | W5 末 | 连接 UE MCP Server 并获取工具列表 |
| M4: LLM 全接入 | W7 末 | OpenAI / Anthropic / DeepSeek 三 Provider 可用 |
| M5: Agent 跑通 | W8 末 | 单 Agent 端到端测试通过 |
| M6: MVP 可演示 | W10 末 | Electron + Core + UE MCP 全链路，可用自然语言操作 Unreal |

---

## 5. 团队配置建议

| 角色 | 人数 | 负责 |
|------|------|------|
| C++ Core 开发 | 1 | 任务 1, 3-11, 13-19 |
| 前端开发 | 1 | 任务 2, 3(TS侧), 20-23 |
| 交叉 | — | C++ 开发负责 12, 16 测试；前端负责 23 联调后半段 |

### 并行开发策略

- W1-2：两人并行——C++ 做骨架 + 核心抽象；前端做 Electron 骨架 + IPC TS 端
- W3-5：C++ 并行推进 Sandbox 和 MCP Client（Sandbox 独立性高）
- W6-8：C++ 专注 Agent Layer；前端开始设计 UI 原型
- W9-10：两人合作联调
