# aknirex's Game Dev Harness —— Phase 1 关键调研报告

> 调研日期：2026-07-11
> 覆盖：LLM 代码生成能力对比 · MCP C++ SDK 选型 · Electron 桌面分发

---

## 一、LLM 代码生成能力（UE C++ 专项）

### 核心结论：没有模型"真正擅长"UE C++

GameDevBench（ICML 2026）——当前唯一的游戏开发 Agent 基准——使用 Godot（比 UE 简单得多），最佳 Agent 仅达 **53.8%**。对 Unreal C++（宏系统、GC、UPROPERTY、Build.cs、UHT），预期成功率更低：

| 复杂度 | 预期成功率 |
|--------|-----------|
| 简单样板代码（新 Actor 类、基本 UPROPERTY） | 60-70% |
| 中等复杂度（组件配置、委托绑定） | 30-50% |
| 复杂多文件（网络、Slate、GAS） | 10-20% |

### 推荐模型栈

| 层级 | 模型 | 用途 | 价格(输出/1M) |
|------|------|------|-------------|
| **主力** | Claude Opus 4.8 / Fable 5 | 架构设计 + 复杂代码 + 多步工具调用 | $25-$50 |
| **性价比** | DeepSeek V4 Pro | LiveCodeBench #1、Codeforces 3206，MIT 开源 | $3.48 |
| **降级/经济** | DeepSeek V4 Flash | 样板代码批量生成 | $0.28 |
| **本地/离线** | Qwen3-Coder-Next (22B, 24GB VRAM) | 初稿和样板，开源免费 | 免费 |

### 关键发现

- **视觉反馈显著提升成功率**：GPT-5.4 + 截图从 41.1% → 52.0% (+11pp)，Claude Sonnet 4.5 + 截图+视频从 33.3% → 47.7% (+14pp)。Harness 应考虑集成截图→LLM 的反馈回路
- **通用模型天花板低**，国内无公开 UE 专项模型。中期可行路径：基于 DeepSeek V4（MIT）或 Qwen3-Coder（Apache 2.0）做 LoRA 微调 + RAG 注入 UE 文档
- **GameCoder-27B**（OpenGame）是唯一游戏专用模型，但目标为 Web 游戏，非 UE

---

## 二、MCP C++ SDK 选型

### 核心结论：用 cxxmcp，不做从零实现

| 库 | 协议覆盖 | 测试通过率 | 许可证 | 依赖 |
|----|---------|-----------|--------|------|
| **cxxmcp** (推荐) | 完整 | 客户端 448/448 (100%) | MIT | **零运行时依赖** |
| mcp-cpp | 完整 | 未公开 | Apache 2.0 | nlohmann_json + cpp-httplib |
| hkr04/cpp-mcp | 基本 | 无 | MIT | GoogleTest |

**cxxmcp 优势**：C++17、零运行时依赖（关键——UE 有自己的 HTTP/SSL，不依赖外部）、9 个正式发布版、605 commits、支持 stdio/Streamable HTTP/WebSocket。

### MCP 协议版本策略

- **立即目标**：2025-11-25 稳定版（所有现有 MCP Server 支持）
- **架构预留**：2026-07-28 将于 7 月 28 日发布——最大变化是**去除 initialize 握手，变为无状态核心**。当前代码架构应避免硬编码 session ID，为无状态过渡做准备

### 配置格式

采用 Claude Desktop 事实标准的 `.mcp.json` / `mcpServers` 格式：
```json
{
  "mcpServers": {
    "unreal": { "url": "http://localhost:8000/mcp" },
    "unity": { "url": "http://localhost:8080/mcp" }
  }
}
```

### 关键发现

- Epic 官方 UE 5.8 MCP 使用 Streamable HTTP（localhost:8000/mcp），无认证、仅 localhost
- 所有社区 Unreal MCP 方案都使用**非 C++ 的 MCP Server**（Node.js / .NET）+ C++ bridge plugin——Harness 只需实现 C++ MCP **Client**，这是更轻量的方向
- MCP 已是 Agent ↔ 工具的 **事实标准**（Linux Foundation 治理，Anthropic/OpenAI/Google/Microsoft/AWS 全部加入），无替代协议威胁

---

## 三、Electron 桌面分发

### 核心结论：electron-builder + GitHub Releases + Azure Trusted Signing

| 组件 | 推荐 |
|------|------|
| **打包工具** | electron-builder（Claude Desktop / VS Code / Discord / Slack 全部使用或其 fork） |
| **安装器 (Win)** | NSIS（支持差分更新）+ MSI（企业 IT 备选） |
| **安装器 (Mac)** | DMG + Apple 公证 |
| **安装器 (Linux)** | AppImage + deb |
| **自动更新** | electron-updater + GitHub Releases（NSIS 差分更新 ~10-25MB / 次） |
| **Win 签名** | Azure Trusted Signing (~$10/月) |
| **Mac 签名** | Apple Developer Program ($99/年) |
| **崩溃上报** | sentry-native + Crashpad (C++) | @sentry/electron (JS) |
| **CI/CD** | GitHub Actions 矩阵（windows/macos/ubuntu） |

### 预估指标

| 指标 | 数值 |
|------|------|
| **安装包大小** | 90-120 MB（已安装），50-70 MB（NSIS 下载） |
| **冷启动时间** | ~1-1.5 秒（优化后） |
| **完整发布 CI 耗时** | ~30 分钟（三平台矩阵并行） |

### 开发与发布流程

```yaml
CI 流程:
  PR push → build-native + build-electron → 打包（不发布）
  tag v* push → build-native + build-electron → 签名 + 公证 + 发布 GitHub Release
```

C++ 后端和 Electron 前端**统一版本发布**（electron-updater 整体替换 `resources/` 目录），避免版本不匹配。

### 关键发现

- 差分更新（BSDiff）仅 Windows NSIS 有效——macOS DMG 和 Linux AppImage 需全量下载
- C++ 后端的崩溃报告与 Electron 进程**独立**——Crashpad 直接上传 minidump 到 Sentry，不经过 Electron
- 90-120 MB 对游戏开发者工具完全可接受（UE 40+ GB，Unity 5+ GB）
