# aknirex's Game Dev Harness —— Kilo Code 借鉴分析

> 分析 Kilo Code (github.com/Kilo-Org/kilocode) 的界面与面向用户功能，提取可应用于 aknirex's Game Dev Harness 的设计模式

---

## 一、Agent 系统：Markdown 定义 + 角色分类

### Kilo Code 做法

Agent 通过 **Markdown 文件 + YAML 头** 定义，放在 `.kilo/agent/` 目录下。内置 6 个主要 Agent (Code / Plan / Ask / Debug / Review / Orchestrator) 和 3 个子 Agent (General / Explore / Scout)。每个 Agent 声明：
- `mode: primary | subagent`——是否可作为主 Agent 或被委派调用
- `permission` 规则集——该 Agent 能用什么工具
- `model`——可选模型覆盖（不同 Agent 用不同模型）
- `steps`——最大迭代次数
- `prompt`——自定义 System Prompt 文件

### 对 Harness 的借鉴

Harness 的多 Agent 系统（Planner / Coder / Asset / Reviewer）可以用同样模式：

```
.gamedev/agents/
├── planner.md      # mode: primary, model: claude-opus, 只读 MCP + 项目扫描
├── coder.md        # mode: subagent, model: deepseek-flash, 写入 MCP + 编译
├── asset.md        # mode: subagent, AI 3D 管线工具
├── reviewer.md     # mode: subagent, 只读 + 验证工具
├── debug.md        # mode: primary, 全工具, 专门排查 Bug
└── blueprint.md    # mode: subagent, UE Blueprint 图编辑工具
```

用户可以自定义游戏专用 Agent（如 `level-designer.md`、`performance-profiler.md`、`ui-builder.md`），Harness 自动从 `.gamedev/agents/` 加载。

---

## 二、权限系统：全局模式 + 分层覆盖

### Kilo Code 做法

```
权限规则 = [{ action: 工具名, pattern: glob模式, permission: allow | ask | deny }]
```

四层合并：System 默认 → Kilo 补丁 → 用户配置 → Agent 专属覆盖。**后层覆盖前层**。

支持通配符：`bash: { "*": "ask", "git push *": "deny", "cat *": "allow" }`

### 对 Harness 的借鉴

Harness 已有的"默认拒绝 + 逐工具授权"可与 Kilo 的 glob 模式结合：

```json
{
  "permission": {
    "mcp.unreal.scene.*": "ask",
    "mcp.unreal.scene.get_*": "allow",
    "mcp.unreal.asset.delete_*": "deny",
    "mcp.unreal.build.*": "ask",
    "bash": { "git *": "allow", "rm *": "deny", "*": "ask" }
  }
}
```

分层覆盖——项目级 `.gamedev/config.json` 中的权限规则覆盖全局默认：
- 项目主管锁定危险操作（`asset.delete_*: deny`）
- 个人开发者放宽个人偏好（`bash.*: allow`）

---

## 三、MCP 集成：本地/远程统一 + 市场发现

### Kilo Code 做法

```json
{
  "mcp": {
    "unreal-editor": {
      "type": "local",
      "command": ["unreal-mcp-server"],
      "environment": { "UE_PROJECT": "/path/to/project" },
      "enabled": true
    },
    "remote-service": {
      "type": "remote",
      "url": "https://mcp.example.com",
      "headers": { "Authorization": "Bearer xxx" },
      "oauth": { "scope": "read write" }
    }
  }
}
```

`kilo mcp` 命令管理 MCP Server 列表。MCP 工具自动发现后按 `serverName_*` 模式生成权限占位。

### 对 Harness 的借鉴

Harness 已有的 MCP Server 管理器可以直接采纳此配置格式。额外做：

- **游戏引擎自动检测**：Harness 打开 UE 项目时自动配置 `mcp.unreal-editor` 连接
- **一键安装**：从官方插件索引安装 MCP Server 配置（如 "安装 Wwise MCP"）→ 自动写入 `mcp.wwise` 配置块
- **引擎状态感知**：当 MCP Server 连接断开时（UE 编辑器关闭），Harness 灰色标记对应工具并提示

---

## 四、CLI 体验：流式渲染 + 模式切换

### Kilo Code 做法

| 命令 | 用途 |
|------|------|
| `kilo` | 启动交互式 TUI |
| `kilo run "message"` | 一次性任务 + 流式输出 |
| `kilo run --auto "message"` | 全自主，无确认弹窗（CI/CD） |
| `kilo serve` | 启动 HTTP+SSE 服务 |
| `kilo session list/delete` | 会话管理 |

TUI 用 **SolidJS + OpenTUI**（JSX 渲染到终端）——工具调用显示为内联图标（⚙ 运行中 / ✓ 完成 / ✗ 错误），推理过程以斜体灰字展示。

### 对 Harness 的借鉴

Harness 的 CLI 模式直接对标：

```bash
# 交互式
agdh                    # 启动 TUI（游戏项目检测 + 自动连接引擎 MCP）

# 一次性
agdh run "为所有敌人添加巡逻动画"
agdh run --auto "重新导入所有 FBX 并修复合集错误"  # CI/CD 夜间任务

# 会话
agdh session list
agdh session continue <id>   # 恢复上次上下文

# 服务
agdh serve                   # 启动 HTTP 服务，Electron GUI 连接之
```

TUI 中的游戏特色显示：
- 3D 资产生成进度条（"TRELLIS: 生成中... 16s"）
- 引擎编译输出流式滚动
- 工具调用 → 内联图标，类比 `agdh` 执行引擎操作时的视觉反馈

---

## 五、上下文压缩：结构化摘要 + 尾部保留

### Kilo Code 做法

自动压缩流程：
1. 触发：Token 超 80% 上下文窗口
2. 生成结构化摘要：Goal / Constraints / Progress (Done/In Progress/Blocked) / Key Decisions / Next Steps / Critical Context
3. 保留尾部最近的 2 轮用户对话为原文
4. 旧工具输出按需剪除

### 对 Harness 的借鉴

Harness 的 Agent 记忆系统已设计了三层记忆（启动注入 / 按需检索 / 懒加载）。补充 Kilo 的结构化压缩模板：

```yaml
压缩摘要模板:
  Goal: "生成 Low-Poly 骑士并导入 Unreal 关卡"
  Progress:
    Done: [生成 GLB 模型, 骨骼绑定]
    In Progress: [MoMask 生成走路动画]
    Blocked: []
  Key Decisions: [选择 TRELLIS 而不选 TripoSR, 面数上限 3000]
  Next Steps: [动画重定向, 导入引擎, SpawnActor 到关卡]
  Scene State: { active_level: "/Game/Maps/TestMap", modified_actors: [] }
  Build Status: { last_compile: "success", errors: 0 }
```

游戏特定保护——压缩时**永远保留**：当前关卡状态、构建错误、活跃的文件路径。

---

## 六、插件与技能系统：工具定义 + SKILL.md

### Kilo Code 做法

**插件**：`Tool.define(id, init)` —— `{ description, parameters (Zod schema), execute }`。插件从 `.kilo/tool/` 或 `.kilo/tools/` 目录的 JS/TS 文件加载。

**技能**：`SKILL.md` 文件 + YAML 头 `{ name, description }`。从 `~/.claude/skills/`、`.kilo/skills/` 和远程 Git 仓库加载。技能内容在 Agent 调用时动态注入上下文。

### 对 Harness 的借鉴

**插件**：C++ 原生 (.dll/.so) + Lua 脚本已定。可以采用 Kilo 的 `{ description, parameters, execute }` 三元组模式——这是通用标准。

**技能**——这是 Harness 目前缺失、但价值极大的功能：

```
.gamedev/skills/
├── ue5-blueprint-patterns.md      # 常见 Blueprint 节点组合模板
├── unity-dots-migration.md        # DOTS 迁移最佳实践
├── shader-optimization.md         # Shader 优化常见模式
├── level-design-patterns.md       # 关卡设计模板
├── animation-retargeting.md       # 动画重定向工作流
└── build-pipeline.md              # 构建打包脚本模板
```

技能由社区贡献，托管在 GitHub，Harness 的插件市场可直接索引和安装。这直接对标 Aura 的 "Aura Skills"。

---

## 七、配置分层：全局 → 项目 → Agent

### Kilo Code 做法

```
kilo.json / kilo.jsonc:       项目级配置（Git 追踪）
~/.config/kilo/kilo.json:     全局用户配置
.kilo/agent/*.md:             Agent 定义
.kilo/skill/*.md:             技能定义
.kilo/tool/*.ts:              插件工具
```

### 对 Harness 的借鉴

```
aknirexGameDevHarness/
├── gamedev.config.json         # 项目级（Git 追踪）——引擎类型、MCP 列表、权限规则
├── .gamedev/
│   ├── agents/                 # 自定义 Agent
│   │   ├── coder.md
│   │   └── level-designer.md
│   ├── skills/                 # 项目特定技能
│   │   └── enemy-ai-patterns.md
│   ├── rules/                  # 项目特定规则
│   │   └── naming-conventions.md
│   └── audit/                  # Agent 操作审计日志
│       └── 2026-07-11.jsonl

~/.gamedev/config.json          # 全局用户配置——默认模型、API Key、引擎路径
~/.gamedev/skills/              # 全局技能库——跨项目复用
```

---

## 八、可立即落地的具体措施

| 优先级 | 借鉴项 | 实施 |
|--------|--------|------|
| P0 | **权限规则用 glob 模式** | 在 PermissionManager 中增加 `{action, pattern, permission}` 三元组匹配，支持 `*` 通配符 |
| P0 | **MCP 配置格式统一** | 采用 `{type: "local"|"remote", command/url, environment/headers}` 标准格式 |
| P0 | **CLI 子命令体系** | `agdh run`, `agdh serve`, `agdh session list`, `agdh mcp`——与 `kilo` 命令结构对齐 |
| P1 | **Agent 用 Markdown+YAML 定义** | 替换硬编码的 Agent 类，从 `.gamedev/agents/*.md` 加载 |
| P1 | **SKILL.md 技能系统** | 游戏开发领域知识以技能文件形式分发，SkillTool 按需注入上下文 |
| P2 | **结构化压缩模板** | 实现 Goal/Progress/Decisions/SceneState 游戏专用压缩摘要 |
| P2 | **配置分层** | 全局 `~/.gamedev/` → 项目 `.gamedev/` → Agent 专属 → CLI flag 覆盖 |
