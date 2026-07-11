# aknirex's Game Dev Harness —— 技术路径总览（修正版）

> 基于全部 10 份调研文档，核实并修正所有数据。日期：2026-07-11

---

## 整体架构

```
Electron Shell (TypeScript/React)
  ↕ stdio NDJSON (IPC 延迟 ~0.1ms 级别, 实测无在此量级的可用数据，但 stdio 非网络栈足以满足 Chat 场景)
Harness Core (C++20, 独立进程)
  ├─ MCP Client (cxxmcp, MIT, 448/448 客户端测试通过)
  │    ├─ GenOrca/unreal-mcp (21 域, 253 actions) —— UE 社区最强
  │    ├─ Epic 官方 Unreal MCP (UE 5.8, ~50 tools) —— 稳定基础
  │    ├─ Unity-MCP IvanMurzak (70+ tools) —— Phase 2
  │    ├─ GladeKit MCP for Unity (260+ tools, 16+ categories) —— Phase 2
  │    ├─ GladeKit MCP for Godot (105 tools) —— Phase 2
  │    ├─ Wwise-MCP (69 tools) —— Phase 2
  │    └─ Blender MCP —— Phase 2
  ├─ Agent Layer
  │    ├─ Phase 1: 单 Agent Loop
  │    └─ Phase 2: Multi-Agent (Orchestrator/Planner/Coder/Asset/Reviewer)
  ├─ Agent Memory: SQLite + sqlite-vec + FTS5 (三层记忆 + 三级共享作用域)
  ├─ Sandbox Engine (P0——隔离执行+可逆回滚, 类 git worktree)
  ├─ Plugin System: C++ 原生 (.dll/.so) + Lua 5.4 + sol2 (沙箱升级路径: Luau)
  ├─ Plugin Marketplace: 去中心化——GitHub Release 托管插件，Harness 维护官方索引 JSON
  └─ AI Pipeline Bridge: Shared Memory (数据, 内存带宽) + Named Pipe (控制信令, ~27µs RTT)
       └─ Python AI Host: TRELLIS, MoMask, RigNet, Blender
```

---

## 技术栈明细

| 层级 | 选型 | 依据 |
|------|------|------|
| **GUI 壳** | Electron + React | 所有竞品一致选择 (Claude/Cursor/Windsurf/VS Code)。Chat UI (Markdown/代码高亮/流式渲染/Diff) 在 Web 生态是成熟组件，Qt 需从零构建 |
| **核心语言** | C++20/23 | 性能关键路径 + UE 生态兼容 |
| **MCP SDK** | cxxmcp (C++17, MIT) | 客户端测试 448/448 通过 (100%), 零运行时依赖 |
| **构建系统** | CMake 3.28+ + vcpkg (manifest 模式) | C++ 事实标准；vcpkg 2800+ 包, 二进制缓存 |
| **HTTP Client** | cpp-httplib | 内置 SSE Client + LLM token 流式专用 cookbook, header-only |
| **JSON** | glaze (C++20 reflection) | 比 nlohmann/json 快 10-15x, 编译期反射零模板代码 |
| **异步 I/O** | ASIO (standalone) + C++20 coroutines | ASIO 成熟, C++20 协程让异步代码可读 |
| **CPU 并行** | Taskflow | DAG 任务调度, MIT 许可, 比 TBB 更轻量 |
| **日志** | spdlog | 异步, 10+ sink 类型, OBS Studio 同款 |
| **测试** | doctest | 编译速度最快 (Catch2 的 10-20x), 语法兼容 |
| **崩溃报告** | sentry-native + Crashpad (C++) / @sentry/electron (JS) | C++ 后端独立上传 minidump, 不与 Electron 耦合 |

---

## IPC 方案

| 路径 | 方案 | 实测延迟 |
|------|------|----------|
| **Electron ↔ C++ Core** | stdio NDJSON | ~0.1ms 级别 (stdio 非网络栈, 主要开销在 JSON 序列化) |
| **C++ Core ↔ MCP Server** | HTTP (Streamable HTTP) | TCP localhost ~1ms 级别 |
| **C++ Core ↔ Python AI (数据)** | Shared Memory | 内存带宽 (~3500 MB/s), 单次 memcpy, 零拷贝 |
| **C++ Core ↔ Python AI (信令)** | Named Pipe | ~27µs RTT (Windows, Stack Overflow 实测均值) |

**gRPC 已被否决**：延迟 5-6x Named Pipe (~150µs), 依赖链 ~50 库。仅用于大文件和低延迟的本地 IPC 场景下 Named Pipe + Shared Memory 更优。

---

## MCP 生态（核实后的数字）

| MCP Server | 工具/行动数 | 来源 | 核实方式 |
|-----------|-----------|------|----------|
| **GenOrca/unreal-mcp** | 21 域, 253 actions | GitHub README | ✅ GitHub 可访问 |
| ultimate-unreal-engine-mcp | 133 tools, 26 域 | GitHub README | ✅ GitHub 可访问 |
| Epic 官方 Unreal MCP (UE 5.8) | ~50 (估算) | 未公开精确数字 | ⚠️ 估算 |
| ue-mcp | 23 类别, 677+ actions | GitHub README | ✅ GitHub 可访问 |
| IvanMurzak/Unity-MCP | 70+ tools | GitHub README | ✅ GitHub 可访问 (3.5k stars) |
| akiojin/unity-editor-mcp | 70 tools, 13 类别 | GitHub README | ✅ GitHub 可访问 |
| GladeKit MCP for Unity | 260+ tools | 调研文档引用 | ⚠️ 原仓库 URL 不可访问, 数字来源待确认 |
| GladeKit MCP for Godot | 105 tools | 调研文档引用 | ⚠️ 原仓库 URL 不可访问, 数字来源待确认 |
| Wwise-MCP | 69 tools | ToolList.md 计数 | ⚠️ 原仓库 URL 不可访问, 数字来源待确认 |
| IvanMurzak/Unreal-MCP | 62 tools, 8 工具族 | GitHub README | ✅ GitHub 可访问 |

> 注：IvanMurzak 维护两个独立项目——`Unreal-MCP` (62 tools, Unreal Engine) 和 `Unity-MCP` (70+ tools, Unity)。两者是不同的 GitHub 仓库。

---

## LLM 模型策略

### 模型选择（与 Phase 1 调研对齐）

| 层级 | 模型 | 编码能力 | 工具调用 | 价格(输出/1M) | 用途 |
|------|------|---------|----------|-------------|------|
| **主力** | Claude Opus 4.8 / Fable 5 | 85-93.6 (BenchLM), 最强 UE API 知识 | 多轮工具调用最强 (tau-bench 0.862) | $25-$50 | 复杂架构设计、多文件重构、多步 MCP 操作 |
| **性价比** | DeepSeek V4 Pro | 78 (BenchLM), LiveCodeBench #1 (93.5) | 多轮未充分基准 | $3.48 | 中等复杂度代码生成、国内 API 首选 |
| **降级/批量** | DeepSeek V4 Flash | 72 (BenchLM) | — | $0.28 | 样板代码、资产扫描、简单变量操作 |
| **本地** | Qwen3-Coder-Next (22B, Q5_K_M, 24GB VRAM) | SWE-bench 58.7% | — | 免费 | 离线/初稿 |
| **视觉** | Claude Opus 4.8 (multimodal) 或 Gemini 3 Pro | — | — | — | 截图→代码纠错 (GameDevBench 验证 +11-14pp) |

### 混元路由策略（区分输入/输出价格）

| 模型 | 输入/1M | 输出/1M | 说明 |
|------|---------|---------|------|
| DeepSeek V4 Flash | $0.14 | $0.28 | 简单任务 |
| Claude Opus 4.8 | $5.00 | $25.00 | 复杂任务 |

**典型 UE C++ 任务估算**（含 function calling 往返 overhead）：

| 任务类型 | 输入 token | 输出 token | DeepSeek Flash 成本 | Claude Opus 成本 |
|----------|-----------|-----------|--------------------|------------------|
| 简单 (单行修改) | 3,000 | 1,000 | $0.0007 | $0.040 |
| 中等 (组件配置) | 5,000 | 2,000 | $0.0013 | $0.075 |
| 复杂 (多文件重构) | 10,000 | 5,000 | $0.0028 | $0.175 |

**混元路由加权** (80% 简单 + 15% 中等 + 5% 复杂)：
- 混合栈：约 $0.01/任务
- 纯 Claude Opus：约 $0.05/任务
- **→ 约 5x 成本节省**

### Prompt Caching（单层节约，不叠加）

- **Claude**: 缓存命中 token 费用为正常的 10% (即 90% 折扣)。将 System Prompt + 引擎规范放在缓存前缀，典型会话中 ~80% 输入 token 可缓存 → **月成本降至原来的 20-30%**
- **GPT**: 缓存命中 50% 折扣
- **Gemini**: 缓存命中 75% 折扣

> 注意：这是缓存本身的节省幅度，不是"先算一次折扣再叠一次百分比"。80% token 以 10% 价格计费 = 输入成本降至原来的 0.8×0.1 + 0.2×1.0 = 28%。

---

## 安全架构

| 层级 | 机制 | 说明 |
|------|------|------|
| **默认拒绝** | 所有操作初始 blocked，逐工具/逐路径/逐域名显式授权 | Claude Code 同款模式 |
| **连续执行** | 安装时用户选择是否启用，随时可切换 | 参考 Claude Code 的 auto 模式 + 独立分类器审查 |
| **Sandbox** | P0——隔离执行 + 可逆回滚 (类 git worktree) | Aura 15.0 标配，52% 开发者对 AI 持负面态度 |
| **注入防御** | 数据/指令通道分离 (借鉴 StruQ 论文，2025 学术研究) + 文件来源追踪 (区分用户文件 vs 第三方文件) | StruQ 是学术论文方向，非成熟产品。Harness 采用其设计理念但需独立实现 |
| **UE 特定防护** | Blueprint 节点白名单验证、DefaultEngine.ini 禁止直接修改、资产变更需 Source Control changelist | CVE 分析后的结论 |
| **审计** | 追加式 JSONL 日志，不可篡改 | 每次 Agent 操作记录时间戳、工具调用、参数、结果 |
| **C++ 沙箱** | Windows: 受限令牌 + Job Objects；Unix: seccomp/pledge | MCP stdio 命令注入漏洞的缓解 |

---

## 多 Agent 架构 (Phase 2)

| Agent | 职责 | 工具集 |
|-------|------|--------|
| **Orchestrator** | 分解任务, 调度子 Agent, 汇总结果 | 所有 Agent 的元信息 |
| **Planner** | 分析任务, 制定执行计划 (DAG) | 项目扫描 + MCP 只读 |
| **Coder** | 生成/修改代码, Sandbox 编译验证 | MCP 代码工具 + 编译器 |
| **Asset** | AI 3D 管线, 资产导入 | TRELLIS/MoMask + MCP 导入 |
| **Reviewer** | 编译验证 + 资产合法性 + VLM 视觉意图对齐 | 编译器 + Blueprint 验证 + VLM |

---

## 插件市场（去中心化，GitHub 托管）

- **模式**：Harness 维护官方插件索引 JSON，插件实际存储在 GitHub Release
- **第一方插件**：LLM Provider (OpenAI/Anthropic/DeepSeek)、MCP Server 配置、AI 3D 服务——全部通过同一 Plugin API 实现
- **第三方插件**：社区开发，托于自有仓库，通过 PR 提交到官方索引
- **类比**：Homebrew Formula 的去中心化分发模式

---

## 桌面分发

| 平台 | 安装器 | 签名 | 崩溃报告 |
|------|--------|------|----------|
| **Windows** | NSIS (差分更新 ~10-25MB/次) | Azure Trusted Signing (~$10/月) | sentry-native + Crashpad |
| **macOS** | DMG + Apple 公证 | Apple Developer Program ($99/年) | sentry-native + Crashpad (需 hardened runtime entitlement) |
| **Linux** | AppImage + deb | GPG 签名 (可选) | sentry-native + Crashpad |
| **自动更新** | electron-updater + GitHub Releases | — | — |
| **CI/CD** | GitHub Actions 矩阵 (win/mac/ubuntu) | — | 预估 ~30 分钟完整发布 |

**安装包**：已安装 ~90-120 MB；Windows NSIS 下载 ~50-70 MB (LZMA 压缩)。C++ 后端 2-5 MB (Release, stripped)。

---

## 开发路线

| Phase | 时间 | 内容 |
|-------|------|------|
| **Phase 1** | 2-3 月 | 单 Agent + MCP Client (连接 Epic 官方 + GenOrca Unreal MCP) + Sandbox + 2 个 AI 3D 工具 + 基础 Electron UI |
| **Phase 2** | 2-3 月 | 多 Agent 架构 + Unity/Godot/Wwise MCP + 插件市场 + AI 3D 完整流水线 + 自动化评估 |
| **Phase 3** | 2-3 月 | 社区生态 + macOS/Linux + GameCoder-27B 评估 + 专用模型微调评估 |

---

## 一句话总结

> Electron 壳 + C++ 核 + MCP 生态接入 + 多 Agent 分工调度 + Claude Opus 主力推理 + DeepSeek Flash 降级降本 + Lua 社区插件 + SQLite Agent 记忆 + Sandbox 兜底安全 + 去中心化插件市场 + GitHub 分发
