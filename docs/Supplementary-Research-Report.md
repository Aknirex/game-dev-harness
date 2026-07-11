# Game Dev Harness —— 补充调研：竞品/生态/学术/安全/行业实践

> 调研日期：2026-07-11
> 覆盖：竞品对标 · MCP 游戏生态 · 学术前沿 · Agent 评估 · 3D 资产标准 · 游戏专用 LLM · 安全前沿 · 行业实践 · 音频自动化 · 开发者态度

---

## 一、竞品深度对标

### Aura 15.0 (Ramen)

**时间线**：2026 年 1 月发布 → 6 月 26 日迭代到 15.0 版本
**规模**：8 人团队，融资超 $40M，30K+ 开发者使用
**核心能力**：

| 能力 | 说明 |
|------|------|
| **Verification Agent** | 编辑器内自动玩游戏、跑测试、抓 Bug |
| **Sandbox Mode** | 隔离环境、可逆操作——操作失败不影响原项目 |
| **Aura Skills** | 自定义工作流，兼容 Claude Code skills |
| **Material Agent** | 构建复杂程序化材质 |
| **无限量 Auto Mode** | 多步自动执行，无需逐次确认 |

**生态动作**：收购了 Flopperam 和 Coplay 两家 Unreal AI 工具公司
**引擎支持**：Unreal 5.8 + Unity

**对 Harness 的影响**：
- 直接竞品，$40M 融资、8 人团队、30K 用户——这是一个已验证的产品市场
- Aura 的迭代速度（1 月→6 月 15 个版本）意味着 Harness 如果只做"工具调用 Agent"，在功能广度上无法竞争
- Sandbox Mode 和 Verification Agent 不是锦上添花，是竞品的标配能力

### Unreal-MCP 生态

| 项目 | 规模 | 架构 | 
|------|------|------|
| **Epic 官方 Unreal MCP** | UE 5.8 内置 | HTTP localhost:8000/mcp，Editor Subsystem |
| **IvanMurzak/Unreal-MCP** | 62 工具 / 8 工具族 | C++ Editor Plugin + .NET sidecar |
| **ultimate-unreal-engine-mcp** | 133 工具 / 26 域 | C++ Editor Plugin |
| **ue-mcp** | 22 类别 / 612+ 动作 | — |

### Unity / Godot MCP 生态

| 项目 | 规模 |
|------|------|
| **Unity-MCP** (GameDev-MCP-Server) | 与 Unreal-MCP 共享后端 |
| **GladeKit MCP for Unity** | 235+ 工具 |
| **@akiojin/unity-editor-mcp** | 70+ 工具 |
| **GladeKit MCP for Godot** | 33 工具 |

**关键发现**：MCP 已成为跨引擎的统一工具访问层标准。Epic 官方将其内置到 UE 5.8，Unity 官方在 2026 年 7 月讨论中已涉及 Unity AI MCP。

---

## 二、MCP 游戏生态全景

MCP 在游戏领域的三个层次：

| 层级 | 实例 | 说明 |
|------|------|------|
| **引擎层** | Unreal MCP (官方) · Unity-MCP · Godot-MCP | 各自提供引擎内部操作 |
| **中间件层** | Wwise-MCP | 通过 WAAPI 暴露 46 个验证工具 |
| **通用层** | horizOn-MCP · GladeKit MCP | 跨引擎通用能力 |

**对 Harness 的架构影响**：
- Harness 目前设计"自建 Named Pipe IPC + 自定义协议"
- 这意味着：**无法利用已有的 MCP 工具生态**（Unreal-MCP 62-133 工具、Unity-MCP 70+ 工具、Wwise-MCP 46 工具）
- **无法与 Claude Code / Cursor 等现有 Agent 兼容**——这些 Agent 已原生支持 MCP 协议
- 架构选择：是自建 IPC 作为 MCP Client 连接已有 MCP Server，还是完全重建工具集？

---

## 三、学术前沿

### GameDevBench（2026 年 2 月）

- 首个游戏开发 Agent 评估基准
- **132 个任务**源自真实教程
- 任务复杂度是现有软件工程基准的 **3 倍以上**
- 最佳 Agent 仅解决 **54.5%** 任务
- 玩法类 46.9%，2D 图形类仅 **31.6%**
- 引入图像/视频反馈后，Claude Sonnet 4.5 从 33.3% → 47.7%

### OpenGame（2026 年 4 月）

- 首个开源端到端 Web 游戏创建 Agent 框架
- 核心机制：**Game Skill**——Template Skill（项目骨架库）+ Debug Skill（修复协议库）
- 配套训练 **GameCoder-27B** 专用模型
- 150 个游戏 prompt 上达 SOTA

### AutoUE（2026 年 3 月，ACL'26 Findings）

- 多 Agent 系统在 Unreal Engine 中端到端生成 3D 游戏
- 覆盖：模型检索 → 场景生成 → 玩法代码合成 → 自动化测试
- 引入 **RAG 机制**用 UE 工具文档缓解 LLM 工具幻觉

**对 Harness 的架构影响**：
- 学术共识：游戏开发对 Agent **极难**（最佳成功率 ~50%）
- 方向共识：**多 Agent 分工 + RAG + 专用模型**
- Harness 目前的单 Agent 设计可能不够——可能需要 Planner Agent + Coder Agent + Asset Agent + Reviewer Agent 的分工架构

---

## 四、Agent 评估体系

### GameDevBench 评估维度

任务所需的多模态理解能力，与任务难度强相关。

### OpenGame-Bench 三维评估

| 维度 | 说明 |
|------|------|
| **Build Health** | 编译和无错误运行 |
| **Visual Usability** | VLM 评判的视觉可用性 |
| **Intent Alignment** | VLM 评判的意图对齐 |

**自动化流程**：无头浏览器自动构建 → 执行 → 截图 → VLM 评判

**对 Harness 的评估体系影响**：
- KR1（"端到端 3D 资产制作成功率 ≥ 80%"）缺乏可量化的评估机制
- 需要一个类似 OpenGame-Bench 的自动化评估流水线
- 当前仅靠人工判断"成功/失败"不够——需要 Build Health（可导入引擎？）+ Visual Usability（视觉可用？）+ Intent Alignment（符合描述？）

---

## 五、生产级 3D 资产标准

2026 年 4 月综述《From Visual Synthesis to Interactive Worlds》：

- 按资产管线而非算法组织
- 区分三类资产：通用物体、角色、场景
- 追踪完整生产周期：数据基础 → 几何合成 → 拓扑优化 → UV 展开 → PBR 外观 → 骨骼绑定 → 场景组装
- **核心结论**：当前生成方法的输出与生产就绪标准之间**持续存在差距**

**对 Harness 的影响**：
- `AI_3D_Pipeline_Research.md` 主要关注"有哪些模型可用"
- 但未系统回答"什么算引擎可用"的技术标准
- Harness 需要明确：
  - Low-Poly 标准（面数上限？）
  - 拓扑可接受度（三角面 / 四边面 / N-gon 比例？）
  - UV 展开质量指标
  - PBR 贴图完整性（Albedo/Normal/Metallic/Roughness 必须全部存在？）

---

## 六、游戏专用 LLM

| 模型 | 参数 | 训练方法 | 能力 |
|------|------|----------|------|
| **GameCoder-27B** | 27B | 三阶段：持续预训练(游戏引擎语料) → 监督微调(开发轨迹) → 执行反馈 RL | 生成 150+ 种游戏类型 |
| **yuspec-gamedev-ai** | 59M | 小型实验 | 探索性 |

**关键结论**：
- 通用 LLM 在游戏领域有明确天花板（GameDevBench 最佳 54.5%）
- 专用模型是提升成功率的关键路径之一
- Harness 目前完全依赖通用 LLM（Claude/GPT/DeepSeek），可能触及性能上限

**对 Harness 的影响**：
- 短期内（Phase 1-2）继续用通用 LLM + 优化 Prompt 和上下文是合理的
- 中长期需评估：是否接入 GameCoder-27B 等专用模型作为子 Agent 的推理引擎？

---

## 七、安全前沿（2026 年新动态）

| 事件 | 时间 | 要点 |
|------|------|------|
| **GDC 2026 调查** | 2026 Q1 | 52% 开发者认为 GenAI 对行业有负面影响 |
| **AgentSecBench** | 2026 年 5 月 | 专门测量 LLM Agent 的 Prompt 注入/隐私泄露/工具完整性 |
| **Microsoft Agent Framework Security** | 2026 年 5 月 | 新的 Agent 安全能力发布 |
| **BioShocking 攻击** | 2026 年 7 月 | 通过虚构场景让 AI 浏览器放弃安全护栏 |

**对 Harness 的影响**：
- Agent 安全是持续对抗的，不是一次设计就完事
- 52% 开发者对 AI 持负面态度——意味着安全事件会被放大
- Sandbox Mode 和可逆操作不是"增强功能"，是**产品能被接受的前提条件**

---

## 八、行业实践：腾讯 Harness Engineering

腾讯 2026 年 6 月公开的实践：

| 组件 | WorkBuddy（Web 原型） | CodeBuddy（代码） |
|------|----------------------|-------------------|
| **用途** | 进入 UE 前低成本验证核心循环 | UE 项目代码开发 |
| **流程** | — | Plan → Rules → Execute → Review |
| **知识库** | 530 个 IMA 游戏知识库 | — |
| **覆盖** | 淘米 ~80% 员工 | — |

四大核心组件：
1. **规则引擎**：定义任务边界和禁止事项
2. **工具接口**：通过 **MCP 协议**接入游戏领域需求
3. **日志与版本控制**：反馈证据与安全回滚
4. **核心逻辑**：从 Prompt Engineering 升级为 **Harness Engineering**

**对 Harness 的影响**：
- 腾讯的实践验证了"规则引擎 + MCP + 可回滚"范式
- Harness 的设计方向与之一致，但缺少明确的规则引擎设计
- 腾讯也选择了 MCP——这是额外的行业验证

---

## 九、游戏音频 AI 自动化

**Wwise-MCP**（2026 年 5 月）：

- 通过 MCP 协议暴露 46 个验证工具
- AI 可：浏览/编辑/试听/性能分析/构建 Wwise 项目
- 自然语言示例："创建一个名为 Footstep 的 Sound 对象并导入音频文件"

**对 Harness 的影响**：
- Wwise 配置繁琐是独立开发者常见痛点
- 已有 MCP 解决方案，Harness 可以直接作为插件接入
- 这进一步验证了"拥抱 MCP 生态"的策略价值

---

## 十、开发者采用与态度

GDC 2026《游戏产业状况报告》（3000+ 开发者）：

| 指标 | 2024 | 2025 | 2026 |
|------|------|------|------|
| **GenAI 对行业有负面影响** | 18% | — | **52%** |
| **GenAI 对行业有正面影响** | — | — | **7%** |
| **工作中使用 AI** | — | 36% | 29% |
| **Steam 新作使用 AI** | — | 21% | 30% (Jan) |

**核心趋势**：
- 开发者对 AI 的态度正在**急剧恶化**（负面比例 18% → 52%）
- 但 AI 生成内容在 Steam 上的实际使用在**增加**（21% → 30%）
- 矛盾：嘴上反对，手上在用

**对 Harness 的影响**：
- 产品定位需强调"开发者控制权"——不是 AI 替代开发者，是开发者指挥 AI
- "可验证、可回滚、不破坏项目"不是宣传语，是消除抵触情绪的必需品
- Aura 的 Sandbox Mode（隔离+可逆）很可能就是针对这种市场情绪设计的

---

## 总结：对 Harness 的核心架构影响

### 需要重新评估的决策

| # | 当前设计 | 调研发现 | 建议变更 |
|---|---------|---------|---------|
| **A** | 自建 Named Pipe IPC + 自定义协议 | MCP 已是 Unreal/Unity 官方标准，社区有 62-612 个现成工具 | **Harness 应作为 MCP Client**，连接已有的 MCP Server，而非重新实现引擎工具 |
| **B** | 单 Agent 架构 | 学术界共识：多 Agent 分工（Planner/Coder/Asset/Reviewer）效果更好 | 评估是否引入多 Agent 架构 |
| **C** | 无自动化评估体系 | GameDevBench/OpenGame-Bench 提供了标准评估框架 | KR1 需要自动化评估流水线 |
| **D** | 依赖通用 LLM | 通用 LLM 天花板 54.5%，专用模型 GameCoder-27B 存在 | Phase 2 评估接入专用模型 |
| **E** | 无 Sandbox 模式 | Aura 标配 Sandbox Mode；52% 开发者对 AI 持负面态度 | Sandbox Mode 从 P2 升级到 P0/P1 |

### 不受影响的设计

- C++ Core + Electron Shell 架构（Aura 也是桌面应用，非 Web）
- Lua 插件系统
- 去中心化插件市场（GitHub 托管）
- Unreal 优先策略（Aura 验证了这个市场）
- 开源核心 + 增值服务商业模式
