# aknirex's Game Dev Harness —— Phase 2 补充调研报告

> 调研日期：2026-07-11
> 覆盖：UE5 深度特性可编程性 · Agent 记忆持久化 · 游戏开发工具与中间件生态

---

## 一、UE5 深度特性可编程性

### 核心结论：大多数 UE5 系统可通过 C++ Editor 代码操控，但 MCP 覆盖参差不齐

### Blueprint 系统——几乎全覆盖

| 操作 | 可达性 | MCP 覆盖 |
|------|--------|----------|
| 创建/修改/编译 Blueprint（含完整图节点编辑） | ✅ | GenOrca 19 actions |
| UMG Widget 创建/修改 | ✅ | GenOrca 15 actions |
| Animation BP 基础操作 | ✅ | GenOrca 4 actions |
| DataTable / DataAsset / Curve | ✅ | GenOrca 8+ actions |
| C++ 类生成（.h/.cpp + UHT 编译） | ❌ | **不可能**——需要 UBT 外部进程 |
| Blueprint diff/merge | ❌ | FBlueprintDiff 是编辑器 UI 专用，无公开 API |

### 各 UE5 子系统状态

| 系统 | 程序化可达 | MCP 覆盖 | 说明 |
|------|-----------|----------|------|
| **Control Rig / IK Rig** | ✅ | ✅ GenOrca 12 actions | 创建骨骼、RigVM 节点、重定向 |
| **GAS (Gameplay Ability System)** | ✅ | ✅ GenOrca 11 actions | Ability/Effect 创建、GameplayTag、Cost/CD |
| **Behavior Tree** | ✅ | ✅ GenOrca 12 actions | 完整的 BT + Blackboard 层级 |
| **Level Sequence** | ✅ | ✅ GenOrca 13 actions | 摄像机、关键帧 |
| **World Partition Data Layers** | ✅ | ✅ GenOrca 6 actions | 创建/删除/分配 Layer |
| **Material Graph** | ✅ | ✅ GenOrca 20 actions | 完整材质图创建 |
| **Nanite 开关** | ✅（单属性） | ❌ 无 MCP 封装 | `UStaticMesh::bEnableNanite` 一个 bool |
| **Lumen 配置** | ✅（ini 修改） | ❌ 无 MCP 封装 | `GConfig->SetBool()` |
| **Niagara VFX** | ⚠️ 部分 | ❌ | 图 API 不公开，文档稀疏 |
| **PCG** | ⚠️ 部分 | ❌ | 节点连接 API 不公开 |
| **MetaSounds** | ⚠️ 部分 | ❌ | Frontend::IDocumentBuilder 极复杂 |
| **World Partition HLOD 构建** | ✅（Commandlet） | ❌ | 需 spawn UAT 子进程 |
| **C++ 类生成** | ❌ | ❌ | UBT 必须外部运行 |

### 硬性限制（不可自动化）

| 类型 | 操作 |
|------|------|
| **需编辑器重启** | 插件启用/禁用、Build.cs 模块变更、.uproject 插件列表修改 |
| **需外部进程** | Cooking/打包（UAT）、项目文件生成（UBT） |
| **需人工判断** | 关卡设计审美、动画手感、VFX 质量、物理调参、数值平衡 |

### 社区 MCP 服务器深度对比

| 能力 | GenOrca/unreal-mcp | Epic 官方 UE 5.8 MCP |
|------|-------------------|---------------------|
| **总工具数** | **253** (21 域) | ~40-60 (8-10 域) |
| **Blueprint 图编辑** | ✅ 完整 | ❌ 无 |
| **Material 图编辑** | ✅ 20 actions | ⚠️ 仅参数 |
| **UMG Widget** | ✅ 15 actions | ❌ |
| **Control Rig** | ✅ | ❌ |
| **IK Rig/Retarget** | ✅ | ❌ |
| **Behavior Tree** | ✅ | ❌ |
| **GAS** | ✅ | ❌ |
| **视口截图** | ✅ | ❌ |
| **Actor 操作/属性** | ✅ | ✅ |

**Harness 策略建议**：优先连接 GenOrca/unreal-mcp（社区最强覆盖），Epic 官方 MCP 作为稳定基础兜底。两者可同时连接（命名空间隔离：`genorca.*` vs `unreal.*`）。

---

## 二、Agent 记忆与持久化

### 推荐架构：SQLite + 三层记忆 + 三级共享作用域

已写入独立文档 `docs/Agent-Memory-Architecture-Research.md`。核心设计：

### 存储层

- **SQLite** 主存储（可查询、可事务）+ **sqlite-vec** 扩展（向量语义搜索）+ **FTS5**（关键词 BM25 搜索）
- **JSONL** 审计日志（人类可读、可 git）
- 不引入 Chroma/Pinecone/Qdrant——游戏开发文档体量不需要专用向量数据库

### 三层记忆模型（借鉴 Claude Code）

| 层级 | 加载时机 | 内容 | 容量限制 |
|------|---------|------|----------|
| **启动注入层** | 会话开始，Prompt Cache | 项目架构、引擎类型、用户偏好、关键文件摘要 | 200 行 / 25KB |
| **按需检索层** | Agent 调用 `memory.search` 工具 | 历史对话片段、错误模式、代码模式 | 向量搜索 Top-K |
| **懒加载知识层** | 首次访问时加载 | UE 文档索引、引擎 API 签名、MCP 工具 Schema | 全文索引 |

### 多 Agent 共享作用域

| 作用域 | 持久化 | 共享者 | 内容示例 |
|--------|--------|--------|----------|
| **全局共享** | ✅ | 所有 Agent | 项目架构、错误模式库、成功操作记录 |
| **角色共享** | ✅ | 同角色 Agent 间 | Coder 代码片段库、Asset 资产生成 prompt 优化 |
| **Agent 私有** | ❌ | 单人 | 当前会话临时上下文、工具调用中间结果 |

### 跨会话恢复

- 需**用户确认**后恢复上次上下文
- 附带文件变更检测（"上次会话后这些文件被外部修改了"）
- 偏好学习经过**人工审核管道**（避免 Agent 从错误中学习到坏模式）

---

## 三、游戏开发工具与中间件生态

### 核心发现：MCP 是高价值但尚未被充分覆盖的领域

### 已有 MCP Server 但 Harness 可封装的

| 工具 | API 可用性 | Harness 封装成本 | 价值 |
|------|-----------|-----------------|------|
| **Houdini Engine** | HAPI (C API) | 中等 | 程序化生成标杆，AI 控制参数化建模 |
| **Substance Automation** | Python Toolkit | **低** | 批量材质/纹理生成，AI 描述→PBR 材质 |
| **Cascadeur** | Python API（嵌入式） | 中等 | AI 辅助动画+自动绑定 |
| **FMOD** | C++/C# Studio API | 中等 | Wwise 已有 MCP 但 FMOD 没有 |
| **GameDriver** | C# API | 中等 | Selenium 风格自动化游戏测试 |
| **Perforce Helix Core** | P4Python + REST API | **低** | 游戏 VCS 操作 |
| **Inworld AI** | REST API | **低** | AI NPC 对话/行为 |

### 已有 MCP Server 可直接连接的

| MCP Server | 工具数 | Harness 接入方式 |
|-----------|--------|-----------------|
| **Wwise-MCP** | 46 | 直接连接，Harness 作为 MCP Client |
| **Blender MCP** | — | 同上层 |
| **Unreal MCP** (Epic 官方 + GenOrca) | 40-253 | 同上层 |
| **Unity MCP** (GladeKit/akiojin) | 70-235 | 同上层 |
| **Godot MCP** (GladeKit) | 33 | 同上层 |

### 完全空白、Harness 可填补的

| 缺口 | 说明 |
|------|------|
| **Shader 图自动生成** | 目前无工具能从自然语言描述生成引擎原生 Shader 图资产——这是 AI Agent 的重大空白 |
| **Fab/Asset Store 自动化** | Fab（Epic 统一资源市场）无公开 API，资产发现和下载完全无法程序化 |
| **跨工具管线编排** | 无 Agent 能将 Houdini → Substance → Cascadeur → UE → CI/CD 串成连贯管线 |
| **游戏 VCS 智能** | 游戏资产的语义化 diff/merge（蓝图合并、关卡冲突解决）无解 |
| **自动化关卡生成** | UE5 PCG + Houdini 有 API 但无 MCP——AI 无法驱动约束式关卡生成 |

### 竞品动态（2025-2026 新兴公司）

| 公司 | 方向 | 融资/状态 |
|------|------|----------|
| **Rosebud AI** | 文本→可玩游戏 | 活跃 |
| **Bitmagic** | 文本→3D 游戏 | 活跃 |
| **PrometheanAI** | 自然语言环境搭建 | 已发布 |
| **Meshy AI** | 文/图→3D 模型 API | 活跃 |
| **Scenario.com** | 风格一致资产批量生成 API | 活跃 |
| **Kinetix** | AI 表情动画 | 活跃 |

---

## 总结：对 Harness 的影响

### Phase 2 应优先集成的 MCP Server

1. **GenOrca/unreal-mcp** —— 253 actions，UE 社区最完整覆盖。Phase 1 就可连接
2. **Wwise-MCP** —— 46 tools，音频中间件标杆。Phase 2 连接即可用
3. **Blender MCP** —— AI 3D 管线最后一步（模型→Blender→引擎）的关键桥接

### Phase 3 应考虑自建的 MCP Server

- **Substance MCP** —— 投入最低（Python API 现成），价值高（AI 描述→PBR 材质）
- **Houdini MCP** —— 程序化建模标杆，HAPI 是 C API 可封装
- **Fab 资产搜索** —— 无 API 需爬取，但用户体验价值极大（"帮我找到合适的 Low-Poly 树模型"）

### 不应追逐的方向

- **Niagara/PCG/MetaSounds 图编辑** —— API 不公开或极复杂，等 Epic 官方开放
- **C++ 类生成** —— UBT 必须外部运行，短期无解
- **自定义 Shader 图生成** —— 全行业空白，需引擎底层支持，不是应用层问题
