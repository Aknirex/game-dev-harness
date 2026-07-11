# aknirex's Game Dev Harness —— 深度特性与生成能力调研

> 调研日期：2026-07-11
> 覆盖：UE5 深度特性 · LLM 生态深入 · Unity/Godot 可编程性 · AI 游戏生成能力

---

## 一、UE5 深度特性可编程性

### Chaos Physics — 全可控

| 子系统 | 创建 | 修改 | 运行时控制 | 说明 |
|--------|------|------|-----------|------|
| **物理资产 (Ragdoll)** | ✅ | ✅ | ✅ | `UPhysicsAsset` + Bodies/Constraints 完整 API |
| **Cloth 模拟** | ✅ | ✅ | ✅ | `FCollectionClothFacade` 完整管线 |
| **载具物理** | ✅ | ✅ | ✅ | `UChaosVehicleMovementComponent` 配置 |
| **破坏系统** | ⚠️ | ⚠️ | ✅ | 实验性插件，需 `GeometryCollectionPlugin` |

### Mutable 角色定制 — 运行时可用，创建需模板

- **运行时驱动**：`UCustomizableObjectInstance` 设置参数（float/color/bool/texture） ✅
- **图构建**：Mutable Graph 编译是编辑器内专有，无公开 API ❌
- **策略**：预建参数化 Mutable Graph 模板 → AI 只修改运行时参数

### Enhanced Input — 全可控

`UInputAction`/`UInputMappingContext` 是 `UDataAsset`，可通过 `NewObject<>()` 完全程序化创建。支持生成 FPS/TPS/Top-Down 输入模板。

### 跨引擎资产迁移 — 无直接方案

- **中间格式**：FBX（几何+动画）、glTF 2.0（PBR 材质近似）、USD（场景层级）
- **无 Unity↔Unreal 直转工具**——材质和逻辑必须手动重建
- Blender 可作为中间站

---

## 二、LLM 生态深入

### 国产模型深度对比

| 模型 | 编码 (SWE-bench) | 工具调用 (BFCL v3) | 多轮工具 (tau-bench) | 输入/1M | 输出/1M | 开源 |
|------|-----------------|-------------------|---------------------|---------|---------|------|
| **DeepSeek V4 Pro** | ~75% | — | — | $0.43 | $0.83 | ✅ |
| **DeepSeek V4 Flash** | ~72% | — | — | $0.14 | $0.28 | ✅ |
| **GLM-5.2** | 62.1% (Pro) | **76.7%** (#1) | 0.797 (#3 零售) | $0.95 | $3.00 | ✅ |
| **Qwen3 Coder 480B** | ~77% | 0.600 (tau-bench) | 0.775 (#4 零售) | 免费 | 免费 | ✅ |
| **Kimi K2.5** | 76.8% | 64.5% | — | $0.60 | $2.40 | ❌ |
| **ERNIE 4.5** | ~60% | — | — | $0.90 | $0.90 | ✅ |
| **MiniMax M1** | ~55% | 47.8% | 0.620 (#2 航空) | $0.50 | $1.50 | ❌ |

**关键发现**：
- BFCL v3 和 tau-bench 排名不一致——GLM-4.5 单次调用最强，Claude 多轮最强，MiniMax M1 意外在 tau-bench 航空领域排第二
- **无国产模型针对游戏开发或 UE C++ 做专项微调**
- Qwen3 Coder 480B 作为可本地部署的免费方案，性价比最优

### 本地部署

**推荐：llama.cpp 作为 C++ 原生库链接**
- 零网络延迟（直接内存→内存）
- 可编译进桌面应用
- 支持 CUDA/Metal/Vulkan 后端
- Ollama 作为伴生进程是备选（更易设置）

| 量化级别 | 质量损失 | UE C++ 代码生成建议 |
|----------|---------|-------------------|
| Q5_K_M | ~2-3% | **推荐**——最佳质量/大小平衡 |
| Q4_K_M | ~4-6% | 可用，但 UE 宏可能增幻觉率 |
| Q3_K_M | ~8-12% | **不推荐**——代码生成不可靠 |

### 混元路由策略——5x 成本节省

```
简单任务 (80%) → DeepSeek V4 Flash ($0.0007/任务)
复杂任务 (20%) → Claude Sonnet 4.5 ($0.15-0.25/任务)
结果：平均 $0.03-0.05/任务 vs 纯 Claude $0.15-0.25/任务
```

### Prompt Caching——最高 ROI 优化

- **Claude**：缓存命中 90% 折扣，System Prompt + UE 规范放在缓存前缀
- 典型会话中 80% 输入 token 可缓存 → 月成本降低 70-90%

---

## 三、Unity/Godot 深度特性可编程性

### 核心结论：API 都存在，但 MCP 覆盖率不足

| 特性 | 引擎 API | MCP 覆盖 | 差距 |
|------|---------|----------|------|
| **Unity DOTS/ECS** | ✅ EntityManager + Baking | ❌ 零覆盖 | 无任何 MCP Server 支持 |
| **Unity Addressables** | ✅ 完整 Editor API | ❌ 零覆盖 | `AddressableAssetSettings` API 可用但无封装 |
| **Unity Shader Graph** | ⚠️ 属性可读写，图创建无公开 API | 🔸 只读 | 可通过文本 .shader 文件生成绕过 |
| **Unity Burst/Jobs** | ✅ Job struct + [BurstCompile] | ❌ 零覆盖 | AI 可生成 .cs 文件但不能触发编译 |
| **Unity Package Manager** | ✅ Client.Add/Remove/Search | ✅ Unity-MCP | — |
| **Unity Timeline** | ✅ 完整 C# API | ✅ 扩展包 | — |
| **Unity Prefab** | ✅ PrefabUtility 完整 | ✅ | — |
| **Godot GDScript AST** | ❌ C++ 内部，无公开 API | ❌ | 只能文本替换，脆弱；可用 C# + Roslyn 代替 |
| **Godot AnimationTree** | ✅ 运行时创建 | 🔸 GladeKit | 完整 blend tree 构建有限 |
| **Godot TileMap** | ✅ 完整 API | ✅ GladeKit | — |
| **Godot Navigation** | ✅ 完整 API | ✅ GladeKit | — |

### 推荐 MCP Server 组合

| 引擎 | 主 MCP Server | 工具数 | 弥补缺口 |
|------|-------------|--------|----------|
| **Unity** | IvanMurzak/Unity-MCP | 70+ | 需自建 Addressables + DOTS + Burst MCP 工具 |
| **Unity** | GladeKit MCP | 260+ | 同上，但覆盖更广 |
| **Godot** | GladeKit Godot Bridge | 105 | 覆盖最好，AnimationTree 有限 |
| **Godot** | coding-solo/godot-mcp | 少 | 外部 CLI 模式，适合简单场景 |

---

## 四、AI 游戏生成能力现状

### 共同架构模式：LLM → DSL/JSON → 引擎插件

这是跨 VFX/Shader/UI/关卡/AI 行为 所有领域的标准化模式。LLM 不直接操控引擎 API——而是生成中间 DSL/JSON，由引擎插件解析并创建资产。

### 各领域现状

| 领域 | 生产就绪 | 研究/PoC | 关键工具 |
|------|---------|----------|----------|
| **PCG 关卡** | UE5.7 PCG (手动) | UnrealLLM (ACL 2025), MCP workflows | AI 设 PCG 参数 |
| **Houdini** | Bria 原生 AI | Houdini-Agent v2.0 | LLM→Python→HAPI |
| **游戏平衡** | Chaos Dynamics (RL) | **RuleSmith** (LLM 多 Agent) | 提议→模拟→批评→优化 |
| **NPC 对话** | Inworld, Convai (云) | LLM 动态对话 | 混合：人工骨架 + LLM 填空 |
| **NVIDIA ACE** | v5.0 GA (gRPC 微服务) | Nemotron SLM (本地) | Chat/A2F/TTS/ASR 全管线 |
| **Text→Shader** | ShaderGPT, AI Shader Studio | AINiagara v0.16 | WebGL 成熟，引擎内联仍在早期 |
| **UI 生成** | Widget Semantic Bridge, CreateLex | UMG-MCP, UMGBuilder | JSON→UMG 是可行路径 |
| **VFX 生成** | **AINiagara**(95% 测试覆盖), Aura VFX | AI Flipbook Generator | **成熟度最高** |
| **行为树生成** | 无 | LLM→BT 论文 (2024) | XML/JSON 中间格式可行 |

### 对 Harness 的策略影响

1. **VFX 和 UI 已经可通过现有 MCP + DSL 模式实现**——AINiagara 和 Widget Semantic Bridge 证明了这一点。Harness 可直接连接这些 MCP Server
2. **游戏平衡/NPC 对话/NPC 行为**需要云 API（Inworld/Convai/ACE），Harness 可作为 MCP Client 封装
3. **Shader 生成**仍在早期——引擎集成的 Text→Shader 无成熟方案，Harness 暂不应投入
4. **关卡生成**——UE5 PCG + LLM 参数驱动是正确方向，但需等 UnrealLLM 或类似工具成熟
