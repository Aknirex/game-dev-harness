# aknirex's Game Dev Harness —— 用户体验与亮点设计

> 文档类型：UX 愿景 + 亮点设计
> 启发来源：Kilo Code / Claude Code / Cursor / VS Code / Blender / Aura 15.0 / Figma / Linear / Arc Browser

---

## 设计原则

| 原则 | 含义 | 灵感 |
|------|------|------|
| **所见即所得，所做即可逆** | 每次 Agent 操作前预览变更，每次操作后可一键回滚。不让开发者"信任黑盒" | Aura Sandbox Mode + Git staged changes UX |
| **进度透明，不让人猜** | 3D 资产生成 30 秒、引擎编译 2 分钟——必须显示每一步在做什么、还要多久 | Blender render progress + Linear task progress |
| **键盘驱动，鼠标辅助** | 专业工具必须有完整快捷键体系。但拖拽文件、右键菜单同样顺手 | VS Code Command Palette + Figma 快捷键 |
| **上下文自觉，不需要教** | Agent 打开项目时自动检测引擎、扫描结构、加载 MCP。不说废话，直接给结果 | Claude Code 的 CLAUDE.md 自动发现 |
| **安静时优雅，工作时清晰** | 默认深色主题。工具调用时内联图标流动。编译失败时红色脉冲。不做多余动效。 | Arc Browser 的极简 + VS Code 的信息密度 |

---

## 一、首次启动：60 秒到第一个任务

### 1.1 引擎自动发现

用户打开 agdh，指向游戏项目文件夹。agdh 扫描目录：

```
检测到 Unreal Engine 项目 (MyGame.uproject)
  ✓ 引擎版本: UE 5.8
  ✓ 已配置 Epic 官方 Unreal MCP (localhost:8000)
  ✓ 检测到 GenOrca/unreal-mcp (253 actions) —— 强烈推荐安装
  ✓ 检测到本地安装的模型: Ollama → Qwen3-Coder-Next 22B (24GB VRAM)

[启动 Unreal Editor] [跳过引擎集成，只用 AI] [自定义配置]
```

**亮点**：不需要填 API Key、不需要配置 MCP JSON、不需要选引擎类型。agdh 自己搞清楚一切，用户只需点"启动"。

**灵感**：VS Code 打开文件夹自动检测语言/框架 → 推荐扩展。Blender 打开文件自动设置单位/渲染器。

---

### 1.2 60 秒演示任务

首次启动后，agdh 不走"欢迎向导"，而是直接跑一个 60 秒演示：

```
Agent: 我看到你的项目里有一个 ThirdPersonCharacter 蓝图。
       要我演示一下我能做什么吗？比如：
       "给这个角色添加一个双击空格二段跳的能力"
       
       [试试看]  [我自己来]
```

用户点"试试看" → Agent 在 Sandbox 中执行完整的"读蓝图 → 加 Input Action → 改代码 → 编译"流程。整个过程约 30 秒。结束后展示变更预览面板——用户可以选择应用或丢弃。

**亮点**：不解释，直接演示。用实际效果代替功能介绍。

**灵感**：Figma 的"跟随教程创建你的第一个设计"。Linear 的 demo project。

---

## 二、对话 + 视觉反馈闭环

### 2.1 截图 → 分析 → 修复

开发者最常见的场景：游戏里出了 Bug，不知道原因。

agdh 的交互：

```
用户: [截图: 角色手臂穿模了] 修复这个问题

Agent: 我看到角色的右手臂穿透了盾牌。
       分析: 盾牌装备插槽的碰撞预设可能未设置。
       → 修改 BP_Shield 的 Collision Preset 为 "Weapon"
       → 编译 Blueprint
       → 截图验证: [修复后截图]
       
       已修复。预览中，要应用吗？
       [应用] [对比前后] [放弃]
```

**亮点**：引擎截图直接拖入聊天框 → Agent 分析截图内容 → 修复 → 发回修复后截图对比。视觉反馈是 GameDevBench 验证过的 +11-14pp 准确率提升。

**灵感**：Cursor 的 inline diff。Claude Code 的 screenshot analysis（GameDevBench 2026）。Figma 的设计稿 → 代码。

---

### 2.2 变更预览面板（Sandbox UI）

所有 Agent 操作先在 Sandbox 中执行，结束后展示变更预览：

```
┌─ 待审核变更 (3 项) ──────────────────────────────────┐
│                                                        │
│  ✓ BP_Shield.uasset        碰撞预设修改                 │
│    属性: CollisionPreset: Default → Weapon             │
│    [预览] [还原]                                        │
│                                                        │
│  ✓ IA_DoubleJump.uasset    新增 Input Action            │
│    [预览] [还原]                                        │
│                                                        │
│  ✓ ThirdPersonCharacter.cpp   新增 47 行代码            │
│    ```diff                                               │
│    + void AThirdPersonCharacter::DoubleJump() {          │
│    +   if (JumpCount < 2) {                             │
│    +     LaunchCharacter(...);                          │
│    +   }                                                │
│    + }                                                  │
│    ```                                                  │
│    [预览] [还原]                                        │
│                                                        │
│  ─────────────────────────────────────────────────────  │
│  [全部应用] [全部放弃] [逐项确认]                        │
└────────────────────────────────────────────────────────┘
```

**亮点**：像 Git staged changes 一样审查 Agent 的变更。改了 Blueprint 的属性？看得到。加了代码？看 diff。一个按钮全部回滚。

**灵感**：VS Code 的 Source Control 面板。Aura 的 Sandbox Mode（竞品验证过的需求）。GitHub PR review UX。

---

## 三、Pipeline 可视化

### 3.1 3D 资产生成流水线

用户说"生成一个 Low-Poly 女战士" → 不只是一句回复，是一条可视化的流水线：

```
┌─ 3D 资产流水线: Low-Poly 女战士 ────────────────────┐
│                                                        │
│  ✓ Step 1/6: TRELLIS 生成模型         [16.2s]  ██████ │
│     输出: warrior_base.glb (3,421 面)                  │
│                                                        │
│  ✓ Step 2/6: Stable Projectorz 贴图    [8.1s]   ██████ │
│     输出: warrior_albedo.png, warrior_normal.png ...   │
│                                                        │
│  ⏳ Step 3/6: RigNet 骨骼绑定          [2.8s]   ████░░ │
│                                                        │
│  ○ Step 4/6: MoMask 走路动画                        │
│  ○ Step 5/6: Blender 动画重定向                       │
│  ○ Step 6/6: 导入 Unreal 关卡                         │
│                                                        │
│  ─────────────────────────────────────────────────────  │
│  预计总耗时: ~30s    已用: 27s                          │
│                                         [取消] [后台执行]│
└────────────────────────────────────────────────────────┘
```

**亮点**：每一步有精灵图式的可视化——TRELLIS 生成完那步，自动展示 3D 模型的**缩略图预览**。不需要等全部跑完才知道结果。

**灵感**：Blender 渲染进度面板。Figma 导出进度。Jenkins Pipeline Stage View。

---

### 3.2 构建/编译流水线

```
┌─ 构建: MyGame (Win64 Development) ────────────────────┐
│  ✓ 编译模块: MyGame          [12s]  ██████             │
│  ✓ 编译模块: MyGameEditor     [8s]   ██████             │
│  ⏳ Cook 资产: 234/1567       [45s]  ██░░░░  15%       │
│     当前: Textures/T_Grass.uasset                       │
│  ○ Stage: Windows                                 │
│  ○ Package: WindowsNoEditor                       │
│  ─────────────────────────────────────────────────────── │
│  已发现 2 个 Warning ── [查看] [忽略]                  │
│      Shader 编译警告: M_Grass 使用了已弃用节点          │
│      纹理大小警告: T_Skybox 分辨率 8192 超过建议值      │
└────────────────────────────────────────────────────────┘
```

**亮点**：构建中如果发现 Warning/Error，Agent 在构建结束前就**开始分析**并向用户汇报。不等用户问。

**灵感**：VS Code 的 Problems 面板实时更新。CI/CD 的 build log 实时流。

---

## 四、多 Agent 透明化

### 4.1 Agent 协作视图

当 Orchestrator 调度多个 Agent 时，用户看到：

```
┌─ 任务: 为玩家角色添加冲刺能力 ─────────────────────────┐
│                                                          │
│  Orchestrator          分解任务 → 3 个子任务              │
│  ───────────────────────────────────────────────────────  │
│                                                          │
│  Planner         ○ 分析完成                               │
│  输出 DAG: [1.生成 Input Action] → [2.改 C++ 代码] →     │
│           [3.编译验证]                                    │
│                                                          │
│  Coder Agent     ⏳ 修改 C++ 中...                        │
│  已修改: ThirdPersonCharacter.h (+12 行)                  │
│           ThirdPersonCharacter.cpp (+38 行)                │
│                                                          │
│  Asset Agent     ○ 创建 Input Action 完成                │
│  IA_Sprint.uasset                                         │
│                                                          │
│  Reviewer        ○ 等待 Coder 完成...                    │
│                                                          │
│  ───────────────────────────────────────────────────────  │
│  子 Agent 并行工作      总进度: 67%                       │
└──────────────────────────────────────────────────────────┘
```

**亮点**：每个 Agent 有独立的卡片，显示当前状态（○ 等待 / ⏳ 工作中 / ✓ 完成 / ✗ 失败）。用户点击卡片展开查看该 Agent 的详细工具调用链。

**灵感**：Jira Kanban 板的多列并行。Figma 多人协作光标。Notion 的任务层级视图。

---

### 4.2 Agent 对话分支

当用户在一个对话中切换方向时，agdh 自动分叉：

```
┌─ 对话历史 ──────────────────────────────────────────────┐
│                                                          │
│  主分支: 为玩家添加冲刺能力                               │
│  ├── ✓ 创建 IA_Sprint Input Action                       │
│  ├── ✓ 修改 ThirdPersonCharacter.h                       │
│  ├── ✓ 编译通过                                          │
│  │                                                       │
│  ├── 分支: "等等，冲刺时应该消耗体力"                    │
│  │   ├── ⏳ 添加 SprintCost 属性                          │
│  │   └── ⏳ 对接体力系统                                  │
│  │                                                       │
│  └── 分支: "冲刺要有粒子特效"                            │
│      └── ✗ 取消──用户说不需要了                           │
│                                                          │
│  [+ 新分支]                                              │
└──────────────────────────────────────────────────────────┘
```

**亮点**：灵感来自 Git 分支图。游戏开发是迭代的——用户可以随时从任何历史点分叉出新对话方向，旧方向保留不丢失。

**灵感**：Git Graph 可视化。Figma 的 Version History 分支。

---

## 五、项目感知与智能建议

### 5.1 项目上下文自动感知

agdh 不只是"一个能聊天的 AI"。它**持续理解**项目状态：

```
右侧边栏: 项目上下文
┌─────────────────────────────┐
│ 引擎: Unreal Engine 5.8      │
│ 项目: MyGame.uproject        │
│ 已连接 MCP: Unreal (官方)    │
│             GenOrca (253 工具)│
│ 上次编译: 2 分钟前 ✓         │
│ 未提交变更: 3 文件            │
│ ───────────────────────────  │
│ 最近修改:                     │
│  BP_Enemy.uasset (3分钟前)   │
│  CombatSystem.cpp (8分钟前)   │
│ ───────────────────────────  │
│ 建议:                         │
│  💡 BP_Enemy 缺少组件配置     │
│  💡 CombatSystem.cpp 有 TODO  │
│  💡 项目中 14 个材质未使用     │
└─────────────────────────────┘
```

**亮点**：Agent 不只在被问时才工作——它持续扫描项目，主动发现 Todo、未使用资产、配置缺失等问题，以非侵入式建议的方式呈现。

**灵感**：VS Code 的 Problems 面板 + TODO Tree 扩展。Linear 的 inbox 主动推送。Xcode 的 Issue Navigator。

---

### 5.2 持续式诊断

当用户在 Unreal Editor 里拖拽操作时，agdh 静默监听：

```
右下角 Toast:
┌──────────────────────────────────────────┐
│ 💡 你刚才复制了 5 个 Enemy BP。          │
│    它们的 PatrolSpeed 都是默认值 600。    │
│    要我帮你批量设置为随机 400-800 吗？    │
│                                          │
│    [好的]  [忽略]  [不再提示此类]         │
└──────────────────────────────────────────┘
```

**亮点**：不是打断性的弹窗，是右下角 5 秒自动消失的 toast。Agent 看到用户在做重复操作时主动提供批量处理方案。

**灵感**：VS Code 的 refactoring 灯泡提示。macOS 通知中心。Google Docs 的"看到你在重复输入"。

---

## 六、极致细节

### 6.1 流式打字 + 思考过程分层

```
用户: 为什么我的 AI 敌人不会转身追玩家？

Agent:
  让我看一下...正在读取 BP_AIEnemy 的行为树
  [⚙ scan_behavior_tree: BP_AIEnemy]

  找到问题了。RotateToFace 节点的目标引用是
  "Self Actor" 而不是 "Player Pawn"。

  原因是 Blackboard 中 "TargetActor" 键
  没有在 Event BeginPlay 时设置。

  [建议修改]
  1. 在 BP_AIEnemy 的 BeginPlay 中添加:
  "Set Blackboard Value as Object → TargetActor → GetPlayerPawn"
  2. 确认 RotateToFace 目标指向 "TargetActor"

  要我在 Sandbox 中演示修复吗？
```

**亮点**：AI 的"思考"和"编写回复"是分开渲染的——思考过程以灰字斜体快速闪过（2x 速度），结论以正常速度输出。让用户看到 Agent 真的有在推理，而不是黑盒返回结果。

**灵感**：Claude Code 的 thinking blocks（dim italic reasoning in terminal）。Kilo Code 的 inline tool status icons。Cursor 的 inline chat。

---

### 6.2 命令面板（Ctrl+K）

```
┌─ 命令面板 ─────────────────────────────────────────────◆
│                                                          │
│  ▏ agdh: 生成 3D 资产                                     │
│  ▏ agdh: 导入资产到引擎                                    │
│  ▏ agdh: 分析性能瓶颈                                      │
│  ▏ agdh: 审查最近的变更                                    │
│  ▏ agdh: 打包构建                                          │
│  ▏ agdh: 查找未使用的资产                                  │
│  ▏ agdh: 切换到 Unity 项目                                 │
│  ▏ agdh: 管理 MCP Server 连接                             │
│  ▏ agdh: 创建自定义 Agent                                 │
│  ▏ agdh: 导出会话记录                                      │
│                                                          │
│  ───────────────────────────────────────────────────────  │
│  Alt+1 聊天  Alt+2 流水线  Alt+3 文件  Alt+4 项目上下文   │
└──────────────────────────────────────────────────────────┘
```

**亮点**：Ctrl+K 打开命令面板。可以无鼠标完成所有操作。每项操作有快捷键提示。类似 VS Code 的 Ctrl+Shift+P——开发者最熟悉的交互模式。

**灵感**：VS Code Command Palette。Linear 的 Cmd+K。Figma 的 Quick Actions。Superhuman 的键盘驱动。

---

### 6.3 暗色主题 + 引擎色彩语义

```
色彩语义:
  Unreal 操作   → 蓝色系 (#0D6EFD)   —— UE 的蓝
  Unity 操作    → 灰色系 (#808080)   —— Unity 的灰
  Godot 操作    → 青色系 (#478CBF)   —— Godot 的蓝
  AI 模型操作   → 紫色系 (#7C3AED)   —— AI 的紫
  Sandbox 操作  → 琥珀色系 (#F59E0B) —— 警示琥珀
  构建/编译     → 绿色系 (#10B981)   —— 成功绿
  错误/失败     → 红色系 (#EF4444)   —— 危险红
```

**亮点**：工具调用、Agent 名称、引擎操作在 UI 中通过色彩区分。不需要文字标签就能识别当前操作的引擎和目标。色彩来自各引擎的品牌色，降低认知负担。

**灵感**：VS Code 的语法高亮色彩系统。GitHub 的语言色彩圆点。Figma 的组件色彩编码。

---

### 6.4 错误恢复与"撤销"体验

```
Agent 操作结束后:
┌────────────────────────────────────────────────────────┐
│ ✓ 已完成: 为所有 Enemy BP 添加 Patrol 组件               │
│   修改了 23 个文件 · Sandbox 编译通过                     │
│                                                         │
│   [全部应用]  [仅应用代码]  [全部回滚]                    │
│                                                         │
│   ────────────────────────────────────────────────────  │
│   提示: 按 Ctrl+Z 撤销本次所有修改                        │
│   提示: 按 Ctrl+Y 恢复撤销                                │
└────────────────────────────────────────────────────────┘
```

**亮点**：Ctrl+Z 可以撤销 Agent 的整批操作。Ctrl+Y 恢复。不需要理解 Git。Sandbox 提供了天然的回滚边界——相当于每一次 Agent 操作都是一笔大号的 Undo 事务。

**灵感**：Blender 的无限 Undo。VS Code 的 Ctrl+Z 文件操作。Photoshop 的 History 面板。

---

## 七、亮点路线图

| Phase | 亮点 | 类别 |
|-------|------|------|
| **Phase 1** | 引擎自动发现 + 一键配置 MCP | 零配置 |
| **Phase 1** | 60 秒演示任务 | 即时感知价值 |
| **Phase 1** | 变更预览面板 (Sandbox UI) + Ctrl+Z 回滚 | 安全 + 可控 |
| **Phase 1** | 流式打字 + 思考过程分层渲染 | 透明 |
| **Phase 2** | Pipeline 可视化（3D 资产生成 + 构建） | 进度透明 |
| **Phase 2** | 截图 → 分析 → 修复视觉反馈闭环 | 多模态 |
| **Phase 2** | 多 Agent 透明化面板 | 协作可视化 |
| **Phase 2** | 命令面板 (Ctrl+K) + 键盘驱动 | 效率 |
| **Phase 3** | 项目上下文自动感知 + 持续式诊断建议 | 主动智能 |
| **Phase 3** | Agent 对话分支 | 迭代友好 |
| **Phase 3** | 暗色主题 + 引擎色彩语义 | 品牌识别 |

---

## 八、竞品 UX 差距分析

| 能力 | Aura 15.0 | agdh 差异化 |
|------|-----------|------------|
| Sandbox 隔离 | ✅ 有 | ✅ 有 + **变更预览面板**（Git 风格的逐文件审查，比 Aura 的"全部应用/放弃"更细粒度） |
| 多 Agent | 不明 | **透明化面板**（每个 Agent 独立卡片，实时状态流） |
| Agent 自定义 | Aura Skills | **SKILL.md 社区贡献**（去中心化、GitHub 托管、开放生态） |
| 引擎接入 | Unreal + Unity | **全 MCP 生态**（Unreal + Unity + Godot + Wwise，且可扩展任意 MCP Server） |
| 引擎截屏反馈 | 不明 | **拖拽截图 → 分析 → 修复 → 对比** 的多模态闭环 |
| 收费模式 | 订阅制 | **开源核心 + 增值服务**（本地 LLM 可选，无订阅强制） |
| 项目主动诊断 | 不明 | **持续扫描 + 非侵入式 toast 建议** |
