# Agent Memory Persistence —— 调研报告与架构建议

> 调研日期：2026-07-11
> 范围：Claude Code · Cursor · 向量数据库 · 对话持久化 · 跨会话状态 · 多 Agent 共享上下文
> 目标：为 Game Dev Harness 的 4-Agent 系统（Planner / Coder / Asset / Reviewer）设计持久化记忆方案

---

## 一、行业基准：Claude Code 与 Cursor 的做法

### 1.1 Claude Code 记忆系统

Claude Code 是当前记忆机制最完整的编码 Agent 工具，核心由两个互补系统构成：

**A. CLAUDE.md 文件（人类编写的持久化指令）**

四级作用域，从广到窄加载：

| 作用域 | 位置 | 用途 |
|--------|------|------|
| Managed Policy | `/Library/Application Support/ClaudeCode/CLAUDE.md` (macOS) | 组织级安全/合规策略，IT 部署 |
| User | `~/.claude/CLAUDE.md` | 跨项目的个人偏好 |
| Project | `./CLAUDE.md` 或 `./.claude/CLAUDE.md` | 项目规范、构建命令、架构约定 |
| Local | `./CLAUDE.local.md` | 个人项目私有配置（加入 .gitignore） |

加载机制：Claude 从工作目录向上遍历目录树，收集所有 CLAUDE.md 和 CLAUDE.local.md，拼接后注入每条会话开头。子目录中的 CLAUDE.md 在 Agent 读取该子目录文件时才懒加载。

**B. Auto Memory（Agent 自主记录的持久化记忆）**

- 位置：`~/.claude/projects/<git-repo>/memory/`
- 所有 worktree 和子目录共享同一个记忆目录（按 git remote origin 去重）
- 结构：`MEMORY.md`（索引）+ 主题文件（`debugging.md`、`api-conventions.md` 等）
- 每次会话加载：`MEMORY.md` 的前 **200 行或 25KB**（取先到者）
- 主题文件按需通过常规文件工具读取——**不做向量语义检索**
- 检索机制是索引匹配，而非 embedding 相似度搜索

**C. Dreams 管道（记忆整理）**

Anthropic 的 Dreams API（Research Preview）在 Agent 空闲时自动运行：
- 消除重复项
- 用最新值替换过时/冲突的条目
- 从会话转录中发现新洞察
- 仅在单个记忆存储上运行——不跨仓库/用户/Agent 做跨域合并

**D. 子 Agent 记忆**

子 Agent 可启用独立的 Auto Memory（参见 sub-agent configuration），存储在自己的记忆目录中。

**Claude Code 的关键限制：**

1. `MEMORY.md` 200 行/25KB 启动上限——超出部分不自动加载
2. 索引驱动检索，非语义检索——词汇不匹配会导致主题文件遗漏
3. 按仓库隔离——跨项目记忆无原生支持
4. Dreams 只在一个存储内做合并——多仓库的跨域心理模型无法构建

### 1.2 Cursor 记忆系统

**A. Rules 文件（主记忆机制）**

| 类型 | 存储位置 | 作用域 |
|------|----------|--------|
| Project Rules | `.cursor/rules/*.mdc` | 按仓库 |
| User Rules | Cursor Settings UI | 本机所有项目 |
| Team Rules | Cursor Dashboard | 组织（Team/Enterprise） |
| AGENTS.md | 项目根目录或子目录，纯 Markdown | 按目录层级 |

`.mdc` 格式支持 YAML frontmatter 精细控制：
```yaml
---
description: "TypeScript coding standards for this repo"
alwaysApply: false
globs: ["**/*.ts", "**/*.tsx"]
---
```

四种应用模式：Always Apply、Apply Intelligently（基于 description）、Apply to Specific Files（glob 匹配）、Apply Manually（@mention）。

**B. 代码库索引（语义搜索）**

- Cursor 将代码库索引到其服务器端的向量数据库
- 每 5 分钟刷新
- Agent 自动按需检索相关上下文
- 这是代码库理解——不是对话记忆

**C. 会话内上下文管理**

- 每次新对话从零开始——无跨会话记忆
- 会话内自动汇总历史以适应上下文窗口
- 用户可 `/summarize` 手动压缩
- `@Past Chats` 可手动引用之前的对话

**D. 已移除的 Notepads 功能**

2025 年底弃用。原设计是存储可复用文本片段的"记事本"，用作持久化上下文。替代方案：
- `AGENTS.md` 或 `.cursor/rules/` 做持久化项目指令
- `.cursor/plans/` 保存实施计划
- MCP 做跨会话外部记忆

---

## 二、向量数据库方案对比（游戏开发场景）

### 2.1 核心对比

| 维度 | SQLite + sqlite-vec | Chroma | Pinecone | Qdrant | Weaviate |
|------|---------------------|--------|----------|--------|----------|
| **部署模式** | 嵌入式，单文件 | 本地 Python 进程 | 云 Only | 本地/云 | 本地/云 |
| **零配置** | ✓ | ✗ (需 Python env) | ✗ (云账号) | ✗ (Docker) | ✗ (Docker) |
| **游戏场景适用性** | ★★★★★ | ★★★ | ★ | ★★★ | ★★ |
| **本地推理** | ✓ (llama.cpp) | ✓ | ✗ | ✓ | ✓ |
| **混合检索** | FTS5 + 向量 | 向量 only | Sparse + Dense | 全文 + 向量 | 混合搜索 |
| **隐私** | 完全本地 | 完全本地 | 云端 | 可本地 | 可本地 |
| **跨平台** | 所有平台 | Python host 需管理 | ✓ | ✓ | ✓ |
| **安装包增量** | ~5MB | ~200MB+ (Python env) | 0 (API) | ~50MB (Docker) | ~100MB+ |

### 2.2 推荐：SQLite + sqlite-vec + FTS5

**理由：**

1. **零运维**——Harness 承诺"下载即运行"。不需要用户安装 Docker、Python 环境、或注册云服务
2. **单文件便携**——整个知识索引是一个 `.db` 文件，备份/迁移/重置简单
3. **混合检索**——FTS5 全文搜索（BM25 排名）+ sqlite-vec 向量余弦相似度，双路融合排序
4. **游戏文档天然适配**——UE/Unity API 文档、C++ 宏定义、Blueprint 节点名——这些高度结构化的内容，关键词搜索比纯语义搜索命中率更高
5. **离线优先**——所有嵌入和检索都在本地运行（用 llama.cpp 加载 GGUF 模型），无 API 费用、无延迟、数据不出本地
6. **容错降级**——如果 sqlite-vec 扩展加载失败（跨平台兼容性问题），可回退到 JavaScript/C++ 内存中的暴力余弦计算

**参考实现：**
- OpenClaw（桌面 AI Agent）使用 SQLite + sqlite-vec + FTS5 做本地 RAG
- sqlite-memory（83 github stars）提供了完整的 SQL 扩展——markdown 分块、嵌入缓存、CRDT 多 Agent 同步
- Hermes Agent 采用 SQLite + JSONL 双写模式

### 2.3 何时需要专用向量数据库？

**不需要。** 游戏开发文档（UE 源码/API 文档）的体量：
- UE 5 头文件：~50MB 文本
- UE 文档：~200MB
- 一个 embedding 模型的维度（768 维 × 4 字节 = 3KB/条目）

即使索引 10,000 个代码块，向量存储需求约 30MB——SQLite 完全胜任。Pinecone/Weaviate 是为"亿级向量"场景设计的，对桌面游戏工具是严重过度工程。

### 2.4 UE 源码 + 文档的索引策略

```
索引管道:
  UE C++ Header (.h) ──┐
  UE C++ Source (.cpp) ─┤
  UE 官方文档 (HTML/MD) ─┤
  Blueprint 节点库 ────┘
      │
      ▼
  分块器（Markdown + C++ 感知）
  - 按函数/类边界分块
  - 保留 UPROPERTY/UFUNCTION 宏元数据
  - 保留头文件路径和行号
      │
      ▼
  嵌入生成（本地 llama.cpp GGUF 模型）
  - 模型：nomic-embed-text Q8_0（约 137MB）
      │
      ▼
  SQLite 存储
  - chunks 表：文本 + 元数据 + 行范围
  - chunks_vec 表（虚拟）：浮点向量
  - chunks_fts 表（虚拟）：FTS5 全文索引
      │
      ▼
  混合检索
  - 向量余弦相似度 + BM25 关键词
  - 可调权重：vector_weight=0.6, text_weight=0.4
```

---

## 三、对话历史持久化

### 3.1 SQLite vs JSONL

| 维度 | SQLite | JSONL (文件) |
|------|--------|-------------|
| **查询能力** | 任意 SQL 查询（按时间/Agent/类型过滤） | 需要全量扫描 |
| **写入性能** | 事务性、并发安全 | 追加写入快 |
| **索引** | 支持（时间、Agent、关键词） | 需要外部索引 |
| **可读性** | 二进制 | 人类可读 |
| **版本控制友好** | 差（二进制 diff） | 好（行级 diff） |
| **向后兼容** | Schema 迁移复杂 | 天然兼容（追加字段） |
| **搜索** | FTS5 全文索引 | grep / 外部索引 |
| **适用规模** | 百万条内 | 数千条内 |

### 3.2 推荐：SQLite 主存储 + JSONL 审计日志

**业界共识——Hermes Agent 和 Cursor SDK 都采用双写模式：**

```
~/.game-dev-harness/
├── sessions/
│   ├── session-{uuid}.jsonl      # 每条对话的完整原始日志（人类可读、可 git）
│   └── metadata.db              # SQLite: 查询索引 + 对话摘要 + Agent 状态
├── memory/
│   ├── project/
│   │   ├── MEMORY.md            # 项目级记忆索引（Claude Code 模式）
│   │   ├── engine-ue-patterns.md
│   │   ├── build-errors.md
│   │   └── user-preferences.md
│   └── agent-private/
│       ├── planner-memory.db    # 每个 Agent 独立的 SQLite 记忆
│       ├── coder-memory.db
│       ├── asset-memory.db
│       └── reviewer-memory.db
└── knowledge/                    # RAG 知识索引
    ├── ue-docs.db               # UE 文档嵌入索引
    ├── ue-source.db             # UE 源码嵌入索引
    └── project-files.db         # 当前项目文件索引
```

**SQLite schema 设计：**

```sql
-- 会话元数据表
CREATE TABLE sessions (
    id TEXT PRIMARY KEY,
    agent_role TEXT NOT NULL,        -- 'planner' | 'coder' | 'asset' | 'reviewer' | 'orchestrator'
    parent_session_id TEXT,          -- 子 Agent 会话的父会话（多 Agent 分支）
    start_time INTEGER NOT NULL,
    end_time INTEGER,
    user_prompt TEXT,
    summary TEXT,                    -- LLM 生成的对话摘要
    tags TEXT,                       -- JSON 数组：['combat', 'unreal', 'blueprint']
    project_root TEXT,
    engine_type TEXT                 -- 'unreal' | 'unity' | 'godot'
);

-- 消息表（可查询的对话历史）
CREATE TABLE messages (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id TEXT NOT NULL REFERENCES sessions(id),
    turn_number INTEGER NOT NULL,
    role TEXT NOT NULL,              -- 'user' | 'assistant' | 'tool' | 'system'
    content TEXT NOT NULL,
    tool_calls TEXT,                 -- JSON: [{name, args, result}]
    token_count INTEGER,
    timestamp INTEGER NOT NULL
);

-- FTS5 全文搜索索引
CREATE VIRTUAL TABLE messages_fts USING fts5(
    content,
    content='messages',
    content_rowid='id'
);

-- 记忆条目表（长期持久化的知识点）
CREATE TABLE memories (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    scope TEXT NOT NULL,             -- 'user' | 'project' | 'agent' | 'session'
    agent_role TEXT,                 -- 哪个 Agent 记录的
    key TEXT NOT NULL,
    value TEXT NOT NULL,
    embedding BLOB,                  -- 语义搜索用向量
    created_at INTEGER NOT NULL,
    expires_at INTEGER,              -- TTL 过期时间
    access_count INTEGER DEFAULT 0,
    last_accessed INTEGER
);

-- 用户偏好表（跨会话持久化）
CREATE TABLE user_preferences (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL,
    updated_at INTEGER NOT NULL,
    source_session_id TEXT           -- 哪个会话中学到的
);
```

### 3.3 对话检索策略

**三层检索，匹配 Claude Code 模式：**

1. **启动注入层**（200 行/25KB 上限）——最近 N 天的交互摘要 + 用户偏好 + 项目关键发现 → 注入 System Prompt
2. **按需检索层**——FTS5 关键词搜索 + sqlite-vec 语义搜索，在 Agent 需要时通过工具调用获取
3. **懒加载知识层**——UE 文档/源码索引仅在 Coder Agent 遇到 API 问题时查询

### 3.4 对话裁剪/压缩策略

```
压缩策略（按优先级）:
  1. 最近 3 轮对话 → 完整保留（包含 tool_calls 结果）
  2. 4-10 轮 → 保留用户意图 + Agent 决策 + 关键工具输出摘要
  3. 10 轮以上 → LLM 生成 {{date}} {{task}} {{outcome}} 三元组摘要
  4. 工具调用结果 → 大输出（> 500 tokens）替换为 [Tool output: 2347 tokens - read full output with recall_tool(12345)]
  5. 跨会话 → 旧会话存为摘要向量，语义搜索召回后用 rowid 读取原始 JSONL
```

**参考：Letta 的三层分页内存模型**
- Core Memory（始终在上下文，2-4KB）：当前任务理解 + 用户当前偏好
- Recall Memory（按需分页）：完整对话历史的可分页 SQLite 日志
- Archival Memory（外部向量存储）：跨会话知识库

### 3.5 多 Agent 对话分支

```
会话树结构:
  Orchestrator Session (root)
      ├── Planner 子任务 A → Planner Session (child of root)
      ├── Coder 子任务 B → Coder Session (child of Agent A)
      └── Asset 子任务 C → Asset Session (child of root)
          └── Reviewer 子任务 D → Reviewer Session (child of root)

  parent_session_id 实现分支追溯
  子 Agent 的 system prompt 包含父任务的 JSON 上下文 + 父 Agent 的关键发现摘要
```

---

## 四、项目上下文持久化

### 4.1 需要持久化的内容

```json
// .game-dev-harness/project-state.json
{
  "project_root": "/path/to/game-project",
  "engine_type": "unreal",
  "engine_version": "5.8.0",
  "engine_path": "C:/Program Files/Epic Games/UE_5.8",
  "detected_on": "2026-07-11T10:00:00Z",

  "file_structure_hash": "sha256:abc123...",
  "build_config": {
    "target": "MyGameEditor",
    "platform": "Win64",
    "configuration": "Development"
  },

  "recent_operations": [
    { "timestamp": "...", "agent": "coder", "operation": "added_class", "file": "src/CombatSystem.h" },
    { "timestamp": "...", "agent": "asset", "operation": "imported_asset", "asset": "knight_rigged.fbx" }
  ],
  "recent_files": ["src/CombatSystem.h", "src/CombatSystem.cpp"],

  "active_tasks": [
    { "id": "task-1", "description": "Implement combat hit detection", "agent": "coder", "status": "in_progress" }
  ],

  "known_issues": [
    { "pattern": "LNK2019 unresolved external", "resolution": "Check Build.cs module dependencies", "encountered": 3 }
  ],
  "learned_conventions": {
    "naming": "PascalCase classes, camelCase methods",
    "include_order": "CoreMinimal.h → Engine headers → Project headers"
  }
}
```

### 4.2 文件格式与缓存策略

**格式：JSON（项目状态）+ Markdown（Agent 约定）**

- `project-state.json`：机器可读的结构化数据，快速解析
- `MEMORY.md` 模式文件（`engine-ue-patterns.md`、`build-errors.md` 等）：Agent 可读的自然语言记忆
- 进程启动时一次加载 project-state.json 到内存，文件修改时间变化时重载
- 每次工具调用后异步写入变更（不阻塞 Agent loop）

### 4.3 VS Code 的 `.vscode/` 目录模式参考

```
.vscode/
├── settings.json       # 编辑器设置（对标 Harness 的 Agent 行为配置）
├── launch.json         # 调试配置（对标 Harness 的 Build Config）
├── tasks.json          # 任务配置（对标 Harness 的 Pipeline 模板）
└── extensions.json     # 推荐扩展（对标 Harness 的推荐插件列表）

.game-dev-harness/
├── project-state.json  # 项目级别配置（引擎类型、构建参数、文件索引哈希）
├── agents.json         # Agent 行为配置（每个 Agent 的 model、temperature、system prompt 覆盖）
├── mcp-servers.json    # MCP Server 连接配置
├── sandbox/            # Sandbox 隔离工作区
│   ├── staging/        # 暂存变更（用户未确认）
│   └── snapshots/      # 回滚快照
├── plans/              # Planner Agent 生成的任务 DAG（可复用）
└── rules/              # 项目级 Agent 规则（对标 .cursor/rules/）
    ├── always-apply.md     # alwaysApply: true
    ├── ue-cpp-conventions.md   # globs: ["**/*.h", "**/*.cpp"]
    └── blueprint-rules.md      # globs: ["**/*.uasset"]
```

---

## 五、跨会话 Agent 状态

### 5.1 应该记住什么？

| 类别 | 内容 | 风险 | 建议 |
|------|------|------|------|
| **用户偏好** | 引擎版本、编码风格、插件选择 | 低 | 始终记住，用户可编辑 |
| **项目结构** | 文件树、模块依赖、Build.cs 配置 | 低 | 自动检测 + 缓存索引 |
| **交互模式** | "用户喜欢先给方案再执行" / "用户喜欢直接动手" | 中 | 被动学习，用户可覆盖 |
| **任务上下文** | "昨天你在做战斗系统的连招检测" | 高 | **需要用户确认——危险但高价值** |
| **错误模式** | "这个 LNK2019 错误上次是缺模块依赖" | 低 | 自动记录，按需检索 |
| **工具调用习惯** | "生成模型时总是用 TRELLIS 而不是 TripoSR，因为质量更好" | 中 | 记录后人工审核 |

### 5.2 "昨天你在做战斗系统"——有价值还是危险？

**可以是有价值的**, 但取决于实现方式：

- **危险的做法**：Agent 自动继续昨天的工作，假定所有上下文仍然有效
  - 用户可能已经手动修改了文件
  - 昨天的方案可能已经被推翻
  - 今天的任务可能与昨天无关

- **安全的做法**（推荐）：
  1. 启动时 Agent 说："检测到昨天的会话在修改 `CombatSystem.h`，有一个未完成的任务'实现连招检测'。需要我恢复上下文吗？"
  2. 用户选择：恢复 / 忽略 / 查看摘要
  3. Agent 从 SQLite 加载该会话的关键摘要（不是完整重放）
  4. 确认文件状态与预期一致后才继续

**实现思路——文件指纹校验：**
```
恢复前检查：
  1. 读取 project-state.json 记录的 file_structure_hash
  2. 与当前文件树 SHA256 对比
  3. 变更文件列表 → 展示给用户
  4. 用户确认哪些变更需要 Agent 考虑
```

### 5.3 用户偏好/模式学习

**被动学习 + 人工审核管道：**

```
Agent 学习循环:
  1. Agent 在交互中检测到模式（如："用户总是拒绝 Blueprint 方案，只在 C++ 中工作"）
     → 写入 memories 表，标记 status='proposed'
  2. 每 5-10 个会话后，向用户推送记忆审核：
     "我注意到以下模式，要保存到我的长期记忆吗？
      - 你偏好 C++ 而非 Blueprint [是/否/编辑]
      - 你在运行时使用 -dx12 而非 -vulkan [是/否/编辑]
      - 你的 Unreal 项目使用 UE 5.8 [是]"
  3. 用户确认 → status='active'
  4. 未确认 → 自动过期

隐私设计：
  - 所有记忆本地存储（~/.game-dev-harness/memory/）
  - 记忆目录可加入 .gitignore
  - 用户可随时通过 UI 查看/编辑/删除
  - 支持 "隐私模式"——本次会话不学习新偏好
```

---

## 六、多 Agent 共享上下文

### 6.1 当前架构的上下文传输

根据 `Architecture-Overview.md`，当前设计：
- 子 Agent 通过 Orchestrator 传递结构化 JSON——**不共享原始对话上下文**
- 每个 Agent 有独立上下文窗口（Prompt Caching 降低重复 Token 费用）
- System Prompt 分三层：核心指令 → 角色特定指令 → 任务上下文

**这个设计是正确的起点，但缺少持久化的共享知识层。**

### 6.2 推荐架构：三层共享记忆

```
┌─────────────────────────────────────────────────────────────────┐
│                      全局共享记忆 (Global)                        │
│  引擎规范 · UE C++ 约定 · 项目架构 · 公共错误模式                  │
│  读写: 所有 Agent · 格式: Markdown + 向量索引                     │
├───────────────────────┬─────────────────────────────────────────┤
│   Agent 角色记忆       │   Agent 私有记忆                         │
│                       │                                         │
│  Coder: UE API 用法    │  会话级临时上下文                         │
│  Asset: AI 管线偏好    │  "当前 DAG 第 3 步，等待 Coder 的输出"    │
│  Reviewer: 验证规则    │  工具调用临时缓存                          │
│                       │                                         │
│  读写: 同角色所有实例  │  读写: 仅当前 Agent 实例                  │
└───────────────────────┴─────────────────────────────────────────┘
```

**全局共享的实现：**

```cpp
// SharedMemoryManager —— 跨 Agent 的持久化共享记忆
class SharedMemoryManager {
public:
    struct MemoryEntry {
        std::string key;           // "ue.compilation_error.LNK2019"
        std::string content;       // Markdown 内容
        std::string scope;         // "global" | "agent-team" | "agent-private"
        std::string author_agent;  // 哪个 Agent 记录的
        std::vector<std::string> tags; // ["unreal", "compilation", "linker"]
        std::chrono::system_clock::time_point created_at;
        std::chrono::system_clock::time_point expires_at; // TTL
    };

    // Agent 通过工具调用写入共享记忆
    void AddMemory(const MemoryEntry& entry);

    // Agent 通过工具调用查询共享记忆
    // 支持关键词 (FTS5) + 语义 (向量) 混合检索
    std::vector<MemoryEntry> SearchMemory(
        const std::string& query,
        const std::string& scope_filter = "all",
        int max_results = 5
    );

    // 清理过期记忆（启动时 / 定期）
    void CleanExpired();

    // 导出全局记忆为 Markdown 文件（可版本控制）
    void ExportToMarkdown(const std::filesystem::path& output_dir);

private:
    std::unique_ptr<SQLite::Database> db_;
};

// Agent 工具注册
// 工具名: memory.add
// 工具名: memory.search
// 工具名: memory.list
```

### 6.3 多 Agent CRDT 同步（可选增强）

当多个 Agent 同时运行并写入共享记忆时：

- 短期：单 Orchestrator 串行调度 → 无并发写入冲突
- 中期：如果需要并行发现（如 Coder 和 Reviewer 同时运行），参考 **sqlite-sync** 的 CRDT 方案
- 每个写入操作由 `agent_id + session_id + clock` 标识
- 合并策略：Last-Write-Wins 或 LLM 仲裁

---

## 七、实现建议：Harness Memory 模块架构

### 7.1 推荐技术栈

```
记忆存储层:
  SQLite (主存储: 对话历史、记忆条目、用户偏好、Agent 状态)
  + sqlite-vec (嵌入向量相似度搜索)
  + FTS5 (全文关键词搜索)
  JSONL (审计日志、会话完整记录、可导出)

本地嵌入:
  llama.cpp + nomic-embed-text-v1.5.Q8_0.gguf (~137MB)
  备选: 用户可选配远程 OpenAI text-embedding-3-small（通过 LLM Provider 插件）

C++ 侧:
  SQLite3 amalgamation (单文件 C 源码, ~12MB 编译产物)
  + 自定义 sqlite_vec 绑定 (或直接调 C API)

文件系统布局:
  ~/.game-dev-harness/
    ├── sessions/{session-uuid}.jsonl
    ├── sessions/metadata.db          ← SQLite 主库
    ├── memory/project/{index,topics}/
    ├── memory/agent-private/{agent-role}.db
    ├── knowledge/ue-docs.db          ← 向量索引
    ├── knowledge/ue-source.db        ← 向量索引
    └── knowledge/project-files.db    ← 向量索引
```

### 7.2 C++ 接口设计

```cpp
// Agent Memory Module
namespace harness::memory {

class MemoryManager {
public:
    // === 会话管理 ===
    std::string StartSession(AgentRole role, std::optional<std::string> parent_session_id);
    void EndSession(const std::string& session_id);
    void AppendMessage(const std::string& session_id, const LLMMessage& msg);
    std::vector<LLMMessage> GetRecentMessages(const std::string& session_id, int count = 10);

    // === 对话压缩（Claude Code /compact 等效） ===
    struct CompactionResult {
        std::string summary;                          // LLM 生成的摘要
        std::vector<std::string> key_facts;           // 提取的关键事实
        int original_tokens;
        int compacted_tokens;
    };
    CompactionResult CompactSession(
        const std::string& session_id,
        std::shared_ptr<ILLMClient> llm               // 用 LLM 做摘要
    );

    // === 持久化记忆 ===
    void AddMemory(
        const std::string& key,
        const std::string& content,
        MemoryScope scope,                             // user | project | agent | session
        std::optional<AgentRole> author_agent
    );

    // 混合检索 (FTS5 + 向量)
    struct SearchResult {
        std::string key;
        std::string content;
        float relevance_score;                         // 0.0-1.0
        MemoryScope scope;
    };
    std::vector<SearchResult> SearchMemory(
        const std::string& query,
        MemoryScope scope_filter = MemoryScope::All,
        int max_results = 5,
        float min_score = 0.6
    );

    // === 会话搜索 ===
    struct SessionMatch {
        std::string session_id;
        std::string summary;
        float score;
    };
    std::vector<SessionMatch> SearchSessions(const std::string& query, int max_results = 3);

    // === 项目状态 ===
    void SaveProjectState(const ProjectState& state);
    ProjectState LoadProjectState();
    bool IsProjectStateStale();                        // 检查文件树是否变更

    // === 跨会话上下文恢复 ===
    struct RecoveryContext {
        std::vector<std::string> active_tasks;         // 未完成的任务
        std::vector<std::string> recent_files;         // 最近操作的文件
        std::string last_session_summary;              // 上次会话摘要
        std::vector<SearchResult> relevant_memories;   // 语义相关的前期记忆
    };
    RecoveryContext BuildRecoveryContext();

    // === 用户偏好 ===
    void UpsertPreference(const std::string& key, const std::string& value);
    std::optional<std::string> GetPreference(const std::string& key);
    std::map<std::string, std::string> GetAllPreferences();

    // === 知识索引（UE 文档等） ===
    void IndexDocumentation(const std::filesystem::path& docs_dir, const std::string& topic);
    std::vector<std::string> QueryKnowledge(
        const std::string& query,
        const std::string& topic_filter = "all",
        int max_results = 5
    );

    // === Token 预算管理 ===
    size_t GetCurrentTokenBudget(const std::string& session_id);
    void ApplyTokenBudget(
        std::vector<LLMMessage>& messages,
        const std::string& session_id,
        size_t max_tokens
    );

private:
    std::unique_ptr<SQLiteDatabase> db_;
    std::unique_ptr<EmbeddingEngine> embedding_engine_;
    std::filesystem::path storage_root_;

    // C++ 嵌入引擎 (llama.cpp)
    std::vector<float> Embed(const std::string& text);
};

} // namespace harness::memory
```

### 7.3 记忆生命周期

```
会话生命周期:
  ┌─ Session Start ───────────────────────────────────────────┐
  │ 1. 加载 project-state.json                               │
  │ 2. 加载 ~/.game-dev-harness/memory/project/MEMORY.md     │
  │    (前 200 行/25KB 注入 System Prompt)                     │
  │ 3. 检查 project-state.json 的文件哈希与磁盘                │
  │    不匹配 → 提示用户（"检测到外部修改"）                   │
  │ 4. SearchSessions() 查找最近的未完成任务                   │
  │ 5. 生成 RecoveryContext，提示用户（"需要恢复上次工作吗？"）│
  ├─ During Session ──────────────────────────────────────────┤
  │ 6. 每轮对话 → AppendMessage() 写入 JSONL + SQLite         │
  │ 7. 每 N 轮 → 检查 token 预算，触发 Compaction              │
  │ 8. Agent 发现新知识 → AddMemory()                          │
  │ 9. 工具调用结果 > 500 tokens → 截断 + 引用 ID              │
  ├─ Session End ────────────────────────────────────────────┤
  │ 10. EndSession() → 写结束时间 + 生成摘要                   │
  │ 11. 保存 project-state.json（文件哈希 + 最近操作）         │
  │ 12. 后台：索引新发现到向量库                                │
  └──────────────────────────────────────────────────────────┘

长期维护:
  - 每周: CleanExpired() 清理 TTL 过期记忆
  - 每月: 记忆合并（类似 Claude Code Dreams 管道）
          - 消除重复项
          - 解决冲突（用户偏好变化）
          - 提取高阶模式
  - 用户触发: "清理我的记忆" → 交互式审核界面
```

### 7.4 Token 预算管理算法

```cpp
void MemoryManager::ApplyTokenBudget(
    std::vector<LLMMessage>& messages,
    const std::string& session_id,
    size_t max_tokens
) {
    size_t total = CountTokens(messages);
    if (total <= max_tokens) return;

    // 优先级降序:
    // 1. System Prompt（核心指令 - 不可裁剪）
    // 2. 最近 N 轮完整对话（用户最后说的话最重要）
    // 3. Auto Memory 注入的上下文（启动时注入的 MEMORY.md 片段）
    // 4. 中间轮次的对话（尝试压缩而不是删除）
    // 5. 早期对话摘要（已压缩过的）

    // 步骤 1: 保留最近 3 轮，计算其余令牌配额
    auto recent_messages = ExtractRecentTurns(messages, 3);
    size_t recent_tokens = CountTokens(recent_messages);
    size_t remaining_budget = max_tokens - recent_tokens - SYSTEM_PROMPT_TOKENS;

    // 步骤 2: 对旧消息应用压缩——LLM 生成摘要替换原始内容
    // 保留最后一条的用户意图和工具输出关键信息
    std::vector<LLMMessage> compacted;
    for (auto& msg : older_messages) {
        if (msg.role == LLMMessage::Role::Tool && msg.content.size() > 500) {
            // 截断长工具输出: [Tool output: 2347 tokens - read full output with recall_tool(12345)]
            msg.content = FormatTruncatedToolOutput(msg.tool_name, msg.content.size());
        }
        compacted.push_back(msg);
    }

    // 步骤 3: 如果还超出预算，移除最早的紧凑消息
    while (CountTokens(compacted) > remaining_budget) {
        compacted.erase(compacted.begin() + SYSTEM_PROMPT_OFFSET);
    }

    messages = compacted;
    messages.insert(messages.end(), recent_messages.begin(), recent_messages.end());
}
```

---

## 八、给 Game Dev Harness 的具体建议

### 8.1 Phase 1 实现（MVP —— 最小可用记忆）

```
优先级 P0（必须）:
  [X] project-state.json 持久化（引擎类型、构建配置、文件哈希）
  [X] 会话级对话历史（JSONL，无搜索——仅当前会话内可恢复）
  [X] Sandbox 快照的回滚点持久化

优先级 P1（重要）:
  [ ] SQLite 会话元数据（parent_session_id 实现子 Agent 分支）
  [ ] MEMORY.md 模式项目记忆（手动编写，启动时注入）
  [ ] 简单关键词搜索旧会话（SQLite LIKE/FTS5）

优先级 P2（增强）:
  [ ] 向量嵌入 + 语义搜索（sqlite-vec，UE 文档索引）
  [ ] 跨会话上下文恢复（"需要恢复上次工作吗？"）
  [ ] 用户偏好被动学习
  [ ] CompactionSummary（自动压缩长对话）
```

### 8.2 八项核心设计原则

1. **CLAUDE.md 模式优先**——项目级 Markdown 文件是人类可读、可版本控制、可跨工具共享的记忆格式。Agent 可以读取，人类可以编辑。不要用纯向量数据库代替可读记忆。

2. **SQLite 做唯一后端**——不要引入 Chroma / Qdrant / Pinecone。SQLite + sqlite-vec + FTS5 覆盖了游戏开发场景下的全部检索需求（关键词、语义、时间段），且完全嵌入式零运维。

3. **三层加载模型**——启动层（200 行/25KB 上限注入 System Prompt）、按需检索层（Agent 工具调用 memory.search）、懒加载知识层（UE 文档仅在 Coder 遇到 API 问题时查询）。

4. **工具输出截断 + 引用 ID**——大文件工具输出（> 500 tokens）不存入对话历史全文，替换为 `[Tool output: N tokens - recall_tool(session_id, invocation_id)]`，需要时按需恢复。

5. **人类在环的记忆审核**——Agent 被动学习用户偏好，但永久化需要用户确认。每 5-10 个会话推送记忆审核。

6. **跨会话恢复需用户确认**——启动时提示"检测到未完成的任务"，但不自动继续。需要用户确认任务仍然相关，同时展示外部文件变更。

7. **每个 Agent 独立记忆空间**——Coder 的 UE API 记忆不和 Asset Agent 的 3D 管线偏好混合。全局共享记忆（项目架构、错误模式）通过 `global` scope 标记。

8. **本地优先、隐私优先**——所有记忆存储在本地 `~/.game-dev-harness/`，不自动上传云端。用户可选择通过插件实现云端同步（如 SQLite Cloud、自定义 MCP Server）。

### 8.3 与现有架构的整合点

```
Architecture-Overview.md 中需要扩展的模块:
  ContextBuilder (已存在) → 集成 MemoryManager::BuildRecoveryContext()
  AgentLoop (已存在) → 每轮对话后调 AppendMessage()
  Orchestrator → 调度前先查 SearchMemory() 获取相关上下文
  工具注册 → 新增 memory.add / memory.search / memory.summarize 工具

Technology stack:
  - SQLite amalgamation 直接嵌入 C++ Core（零外部依赖）
  - sqlite-vec 作为预编译扩展随 Harness 分发
  - llama.cpp 嵌入模型可选——LLM Provider 插件也可提供远程 embedding API
  - JSONL 会话日志直接用 std::ofstream 追加写入
```

### 8.4 风险与缓解

| 风险 | 概率 | 缓解措施 |
|------|------|----------|
| **记忆污染**（恶意用户输入导致 Agent 记住错误信息） | 中 | 人类在环审核 + TTL 过期 + 写模式异常检测 |
| **隐私泄露**（记忆文件被其他程序读取或提交到 git） | 中 | 记忆目录默认 `.gitignore` + 文件权限 0600 |
| **SQLite 性能退化**（百万条消息后查询变慢） | 低 | FTS5 索引 + 定期归档旧会话 + 分区策略 |
| **嵌入模型与平台不兼容** | 中 | 优雅降级到纯 FTS5 关键词搜索（无嵌入模式） |
| **记忆膨胀**（上下文窗口被过多记忆占用） | 高 | 200 行/25KB 硬上限 + 按需检索 + 周期性整理 |
| **多 Agent 竞争条件** | 低 | Phase 1 串行调度，Phase 2 + WAL 模式 SQLite |

---

## 九、参考文献

- [Claude Code Memory Documentation](https://code.claude.com/docs/en/memory) — 官方 CLAUDE.md / Auto Memory / Rules 文档
- [Claude Code Memory: Complete Guide to Persistence](https://vectorize.io/articles/claude-code-memory) — Vectorize 的深度分析（2026-06-17）
- [AI Agent Memory Architectures (Zylos Research)](https://zylos.ai/research/2026-04-05-ai-agent-memory-architectures-persistent-knowledge/) — 行业全景调查（2026-04-05）
- [Cursor Rules / AGENTS.md / MDC Format](https://github.com/agent-sh/agent-knowledge/blob/main/cursor-ide-memory-context.md) — agent-sh 知识库
- [SQLite-Memory: Persistent AI Agent Memory](https://github.com/sqliteai/sqlite-memory) — sqliteai/sqlite-memory
- [Local-First RAG: SQLite for AI Agent Memory (OpenClaw)](https://www.pingcap.com/blog/local-first-rag-using-sqlite-ai-agent-memory-openclaw/) — PingCAP 技术博客（2026-02-06）
- [SQLite + FTS5 Conversation Memory](https://gist.github.com/betoneh/79006333939dc2433e5e2f30428dd615) — OpenClaw 实现参考
- [Session Storage & Persistence (Codexia)](https://deepwiki.com/milisp/codexia/5.3-session-storage-and-persistence) — Hermes 双写模式参考
- [Mem0 Architecture Paper (arxiv 2504.19413)](https://arxiv.org/abs/2504.19413) — 生产级 AI Agent 记忆架构
- [Zep Graphiti (arxiv 2501.13956)](https://arxiv.org/abs/2501.13956) — 时态知识图谱记忆
- [Memory Poisoning in AI Agents (Unit 42)](https://unit42.paloaltonetworks.com/indirect-prompt-injection-poisons-ai-longterm-memory/) — 记忆安全风险分析
- [MINJA: Memory Injection Attack](https://www.mintmcp.com/blog/ai-agent-memory-poisoning) — 记忆注入攻击防御
