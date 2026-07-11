# AI 驱动的 3D 资产全流程生成调研

> 调研时间: 2026-07-11 · 涵盖国内外商业平台 + 可自部署开源方案

---

## 流程总览

``` mermaid
graph TD
    O[根据想法、需求产生设计书、设计图] --> A[设计书/设计图 → 3D模型]
    O --> D[骨骼动画数据<br>可独立并行]

    A --> B[模型贴图/纹理]
    A --> C[骨骼绑定 Rigging]

    B --> E[汇总渲染 / 导入游戏引擎]
    C --> E
    D --> E
```

---

## 1a — 设计图/文本 → 3D 模型生成

### 商业服务平台

| 平台 | 开发商 | 输入 | 核心技术 | 特色 | 地址 |
|------|--------|------|----------|------|------|
| **Meshy** | Meshy LLC (美国) | 图/文/草图 | 自研 Gaussian Splatting + Transformer | 3D 打印优化 (水密网格)、PBR 贴图、自动修复 | [meshy.ai](https://www.meshy.ai/) |
| **Tripo AI** | VAST (北京) | 图/文 | LRM (Large Reconstruction Model) | 与 Stability AI 合作，最快 <1s | [tripo3d.ai](https://www.tripo3d.ai/) |
| **Luma AI** | Luma (美国) | 视频/图 | NeRF / 3D Gaussian Splatting | 手机扫描真实场景首选 | [lumalabs.ai](https://lumalabs.ai/) |
| **Rodin** | Deemos (北京/清华) | 图/文 | Diffusion + Transformer | 3D 头像专长 | [hyper3d.ai](https://hyper3d.ai/) |
| **CSM** | CSM (美国) | 图 | 多视图扩散 + NeuS | 多视图重建 | [csm.ai](https://csm.ai/) |
| **Wan3D (万相3D)** | 阿里通义 | 图/文 | 自研 3D Diffusion | 国内生态(魔搭) | [tongyi.aliyun.com](https://tongyi.aliyun.com/) |
| **Hunyuan3D** | 腾讯混元 | 图/文 | DiT (Diffusion Transformer) + 3D VAE | 国产开源可部署 | [3d-models.hunyuan.tencent.com](https://3d-models.hunyuan.tencent.com/) |
| **Dreamina (即梦)** | 字节跳动 | 图/文 | — | 中文生态 | [jimeng.jianying.com](https://jimeng.jianying.com/) |

### 开源模型 (可自部署)

| 项目 | 作者 | 输入 | 显存 | License | Stars | 特点 | 地址 |
|------|------|------|------|---------|-------|------|------|
| **TRELLIS** | Microsoft | 图/文 | 16GB+ | MIT | 13.2k | 🔥 **当前最强**，2B 参数，统一输出 Gaussian/Mesh/RF，CVPR 2025 Spotlight | [GitHub](https://github.com/microsoft/TRELLIS) |
| **TripoSR** | VAST + Stability AI | 图 | 6GB | MIT | 6.7k | 最快 (<0.5s A100)，轻量入门 | [GitHub](https://github.com/VAST-AI-Research/TripoSR) |
| **Stable Fast 3D** | Stability AI | 图 | 8GB+ | Community | — | UV 展开 + 材质，纹理质量最高 | [GitHub](https://github.com/Stability-AI/stable-fast-3d) |
| **stable-dreamfusion** | ashawkey | 图/文 | 16GB | Apache 2.0 | 8.8k | DreamFusion 经典实现，NeRF+SD | [GitHub](https://github.com/ashawkey/stable-dreamfusion) |
| **threestudio** | 社区 | 图/文/多视图 | 12GB+ | Apache 2.0 | 7.0k | 统一框架，支持 20+ 种方法 | [GitHub](https://github.com/threestudio-project/threestudio) |
| **Shap-E** | OpenAI | 图/文 | 4GB | MIT | 12.3k | 轻量隐式函数生成 | [GitHub](https://github.com/openai/shap-e) |
| **Hunyuan3D** | 腾讯 | 图/文 | — | 开源 | — | DiT 架构，国内首选 | [GitHub](https://github.com/Tencent/Hunyuan3D-1) |
| **Zero-1-to-3** | Columbia | 图→多视图 | 8GB | 开源 | — | 单图转多视角(管线前置) | [GitHub](https://github.com/cvlab-columbia/zero123) |
| **InstantMesh** | TencentARC | 图 | — | 开源 | — | 多视图→Mesh 快速方案 | [GitHub](https://github.com/TencentARC/InstantMesh) |

### 技术路线对比

| 路线 | 代表 | 速度 | 质量 | 原理 |
|------|------|------|------|------|
| **前馈式 (Feed-forward)** | TRELLIS, TripoSR, SF3D | 秒级 | 中高 | 单次推理直接出 3D |
| **优化式 (SDS)** | DreamFusion, ProlificDreamer | 分钟~小时 | 高 | 2D 扩散模型蒸馏到 3D |
| **混合式** | threestudio 框架 | 可配置 | 可配置 | 自由切换路线 |

---

## 2a — 模型贴图/纹理生成

### 商业服务平台

| 平台 | 开发商 | 输入 | 核心技术 | 特色 | 地址 |
|------|--------|------|----------|------|------|
| **Meshy AI Texturing** | Meshy | 3D 模型 | AI PBR 纹理生成 | 自动生成 Albedo/Normal/Metallic/Roughness，4K HD 模式 | [meshy.ai](https://www.meshy.ai/) |
| **ArmorPaint / Substance** | Adobe | 3D 模型 | 程序化 + AI | 行业标准，AI 辅助材质生成 | [Substance](https://www.adobe.com/products/substance3d.html) / [ArmorPaint](https://armorpaint.org/) |
| **Leonardo.ai** | Leonardo AI | 文本/图→材质 | Diffusion | 贴图/纹理图片生成 | [leonardo.ai](https://leonardo.ai/) |
| **PolyCam** | PolyCam | 照片扫描 | 摄影测量+AI | 实拍转 PBR 贴图 | [poly.cam](https://poly.cam/) |

### 开源方案

| 项目 | 说明 | 技术栈 | 地址 |
|------|------|--------|------|
| **Stable Projectorz** | Stability AI，3D 模型自动纹理投影 | SD + 多视图投影 | [stableprojectorz.com](https://stableprojectorz.com/) |
| **Paint3D** | 清华，3D 模型贴图生成 | UV 空间 Diffusion | [GitHub](https://github.com/OpenTexture/Paint3D) |
| **TEXTure** | 文本驱动 3D 纹理编辑 | Depth-aware Diffusion | [GitHub](https://github.com/TEXTurePaper/TEXTurePaper) |
| **SyncMVD** | 多视图一致纹理生成 | Multi-view Diffusion | [GitHub](https://github.com/LIU-Yuxin/SyncMVD) |
| **DreamTexture** | 文本→3D 纹理 | SDS 优化 | [GitHub](https://github.com/anantarb/dreamtexture) |

---

## 2b — 骨骼绑定 (Auto-Rigging)

### 商业服务平台

| 平台 | 开发商 | 输入 | 核心技术 | 免费 | 地址 |
|------|--------|------|----------|------|------|
| **Mixamo** | Adobe | 3D 模型 (OBJ/FBX) | 关节检测 + 自动蒙皮权重映射 | ✅ 完全免费 | [mixamo.com](https://www.mixamo.com/) |
| **AccuRIG** | Reallusion | 3D 模型 | 自研骨架匹配算法，支持人形/四足/多足 | ✅ 免费 | [actorcore.reallusion.com](https://actorcore.reallusion.com/auto-rig) |
| **Meshy Animation** | Meshy | AI 生成的 3D 模型 | 内置骨架匹配 + 600+ 动作库 | Freemium | [meshy.ai](https://www.meshy.ai/) |
| **Rokoko Rig** | Rokoko | 3D 模型 | 深度学习骨骼预测(云端) | 付费 | [rokoko.com](https://www.rokoko.com/) |

### 开源方案

| 项目 | 说明 | 技术 | 地址 |
|------|------|------|------|
| **RigNet** | MIT，神经网络自动骨骼绑定 | GNN + 蒙皮权重预测 | [GitHub](https://github.com/zhan-xu/rig-net) |
| **Pinocchio** | CMU 经典算法，自动骨架提取嵌入 | 热扩散骨架提取 | [GitHub](https://github.com/jingma-git/Pinocchio) |
| **Auto-Rig Pro** | Blender 插件 | 规则+半自动 | [superhivemarket.com](https://superhivemarket.com/products/auto-rig-pro) |

---

## 1c — 骨骼动画数据制作

> 此环节可与 1a 模型生成并行进行。

### 子路线 A: 视频动捕 (Video → 骨骼动画)

| 平台 | 开发商 | 输入 | 输出 | 技术 | 价格 | 地址 |
|------|--------|------|------|------|------|------|
| **Plask** | Plask (韩国) | 单目视频 | FBX/BVH/GLB + 3D 渲染 | AI MoCap V2 + SMPL | 免费基础版 | [plask.ai](https://plask.ai/) |
| **DeepMotion** | DeepMotion (美国) | 视频 | FBX/BVH/GLB | 物理仿真 + 姿态估计 + 手指追踪 | 免费试用 | [deepmotion.com](https://www.deepmotion.com/) |
| **Rokoko Vision** | Rokoko (丹麦) | 单/双目视频 | FBX/BVH | AI 姿态估计 + IK 求解器 | 免费有限 | [rokoko.com/products/vision](https://www.rokoko.com/products/vision) |
| **Move.ai** | Move.ai (英国) | 多机位视频 | FBX/BVH | 多视图 3D 重建，亚毫米级精度 | 付费 | [move.ai](https://www.move.ai/) |
| **Radical Motion** | Radical (英国) | 单目视频 | FBX/BVH | 实时 3D 姿态估计 | Freemium | [radicalmotion.com](https://radicalmotion.com/) |
| **Kinetix** | Kinetix (法国) | 视频→3D 动画 | FBX + 表情 | AI 动捕 + 面部同步 | Freemium | [kinetix.tech](https://kinetix.tech/) |
| **Wonder Dynamics** | Autodesk | 视频→角色替换 | 全场景 3D | 视频中演员自动替换为 3D 角色 | 付费 | [wonderdynamics.com](https://wonderdynamics.com/) |

### 子路线 B: AI 动画数据生成 (Text/条件 → 骨骼动画)

| 项目 | 作者 | 输入 | 输出 | 技术 | Stars | 地址 |
|------|------|------|------|------|-------|------|
| **MDM** | Tel Aviv Univ (ICLR 2023) | 文本/动作类别 | SMPL 骨骼序列 | Diffusion + Transformer | 4.1k | [GitHub](https://github.com/GuyTevet/motion-diffusion-model) |
| **MoMask** | 阿尔伯塔大学 (CVPR 2024) | 文本 | BVH 骨骼动画 | VQ-VAE + Masked Transformer | 1.3k | [GitHub](https://github.com/EricGuo5513/momask-codes) |
| **T2M-GPT** | — | 文本 | 骨骼序列 | VQ-VAE + GPT 自回归 | ~500 | [GitHub](https://github.com/Mael-zys/T2M-GPT) |
| **MLD** | — | 文本 | 骨骼序列 | 潜空间扩散 | ~500 | [GitHub](https://github.com/ChenFengYe/motion-latent-diffusion) |
| **MotionDiffuse** | — | 文本 | 骨骼动画 | Diffusion + CLIP | ~400 | [GitHub](https://github.com/MotrixLab/MotionDiffuse) |
| **DiP (CLoSD)** | Tel Aviv Univ (2024) | 文本+控制 | 实时物理角色动画 | MDM + 物理仿真闭环 | (MDM子项目) | [GitHub](https://github.com/GuyTevet/motion-diffusion-model) |
| **Cascadeur** | Nekki | 关键帧+AI辅助 | FBX | AI 物理修正 + 自动插值 | — | [cascadeur.com](https://cascadeur.com/) |

### 子路线 C: 预置动画库

| 平台 | 说明 | 地址 |
|------|------|------|
| **Mixamo** | Adobe 维护的免费骨骼动画库 (数百种动作) | [mixamo.com](https://www.mixamo.com/) |
| **ActorCore** | Reallusion 动作库 | [actorcore.reallusion.com](https://actorcore.reallusion.com/) |
| **Unreal Marketplace** | 大量付费/免费动画资产 | [unrealengine.com/marketplace](https://www.unrealengine.com/marketplace) |

### 开源动作捕捉方案 (可自部署)

| 项目 | 说明 | 技术栈 | 地址 |
|------|------|--------|------|
| **OpenPose** | CMU，2D 人体关键点检测 | Caffe/PyTorch，结合 SMPL 可升 3D | [GitHub](https://github.com/CMU-Perceptual-Computing-Lab/openpose) |
| **SMPL / SMPL-X** | 马克思普朗克研究所 | 参数化人体模型 (业界标准基座) | [SMPL](https://smpl.is.tue.mpg.de/) / [SMPL-X](https://smpl-x.is.tue.mpg.de/) |
| **ROMP** | 单目视频多人 3D 姿态+形状 | PyTorch | [GitHub](https://github.com/Arthur151/ROMP) |
| **VIBE** | 视频中 3D 人体姿态+形状估计 | PyTorch + SMPL | [GitHub](https://github.com/mkocabas/VIBE) |
| **WHAM** | 视频中精确 3D 人体运动重建 (2024) | PyTorch | [GitHub](https://github.com/yohanshin/WHAM) |
| **4D-Humans** | 单目视频全局一致的 3D 人体跟踪 | PyTorch | [GitHub](https://github.com/shubham-goel/4D-Humans) |

### 动画数据重定向工具

| 工具 | 用途 | 地址 |
|------|------|------|
| **Keemap Rig Transfer** (Blender 插件) | BVH → 自定义角色骨骼映射+重定向 | [GitHub](https://github.com/nkeeline/Keemap-Blender-Rig-ReTargeting-Addon) |
| **Rokoko Retargeting** | 自动骨骼映射 | [rokoko.com](https://www.rokoko.com/) |
| **Blender Auto-Rig Pro** | 绑定+重定向一体化 | [superhivemarket.com](https://superhivemarket.com/products/auto-rig-pro) |
| **bvh2vrma** | BVH 文件在线可视化+格式转换 | [GitHub](https://github.com/vrm-c/bvh2vrma) |

---

## 3 — 汇总渲染 / 导入游戏引擎

> 此环节将前述步骤产出的 **3D模型** + **贴图纹理** + **骨骼动画** 合并，输出最终成果。

### 3a — 离线渲染

| 工具 | 用途 | 地址 |
|------|------|------|
| **Blender** | 免费开源，Cycles/Eevee 渲染器，支持 Python 脚本批处理 | [blender.org](https://www.blender.org/) |
| **Unreal Engine (Movie Render Queue)** | 影视级离线渲染 | [unrealengine.com](https://www.unrealengine.com/) |
| **Unity (Timeline + Recorder)** | 游戏引擎内渲染视频 | [unity.com](https://unity.com/) |
| **Maya + Arnold** | 工业标准离线渲染 | [autodesk.com/maya](https://www.autodesk.com/products/maya/) |
| **Houdini + Karma** | 程序化特效+渲染 | [sidefx.com](https://www.sidefx.com/) |

### 3b — 实时导入游戏引擎

| 引擎 | 导入格式 | 说明 | 地址 |
|------|----------|------|------|
| **Unreal Engine 5** | FBX/GLB/OBJ + 骨骼动画 | 最大生态，Nanite/Lumen 技术 | [unrealengine.com](https://www.unrealengine.com/) |
| **Unity** | FBX/GLB/OBJ + Animation Clip | 独立游戏首选 | [unity.com](https://unity.com/) |
| **Godot** | GLB/FBX/OBJ | 开源引擎 | [godotengine.org](https://godotengine.org/) |
| **Roblox Studio** | FBX/OBJ | UGC 平台 | [create.roblox.com](https://create.roblox.com/) |
| **Three.js / Babylon.js** | GLB/GLTF | Web 端 3D 渲染 | [threejs.org](https://threejs.org/) / [babylonjs.com](https://www.babylonjs.com/) |

### 典型数据流

```
TRELLIS → GLB (含 Mesh+纹理)
   +
MoMask → BVH (骨骼动画)
   ↓
Blender: GLB导入 → Keemap Rig Transfer (BVH重定向到模型骨骼)
   ↓
导出 FBX/GLB → Unity/Unreal/Web
```

---

## 推荐技术栈总结

### 方案 A: 全云端 (零部署, 最快)

```
1a 模型: Meshy (图→3D) / Tripo AI (图→3D, <1s)
2a 贴图: Meshy AI Texturing (自动 PBR)
2b 绑定: Mixamo (免费, 自动)
1c 动画: Plask (视频动捕) 或 Mixamo 动画库
3 汇总: Blender 或直接导入游戏引擎
```

### 方案 B: 全开源自部署 (可控, 免费商用)

```
1a 模型: TRELLIS (MIT) 或 TripoSR (MIT)，单卡 16GB
2a 贴图: Stable Projectorz 或 Paint3D
2b 绑定: RigNet (MIT) 或 Pinocchio
1c 动画: MoMask (MIT, CVPR2024) + Mixamo 免费动画库
3 汇总: Blender → GLB/FBX 导出
```

### 方案 C: 最低门槛 (消费级显卡)

```
1a 模型: TripoSR (6GB VRAM, MIT)
1c 动画: Mixamo 预置动画库 (无需GPU)
2b 绑定: Mixamo 自动绑定
3 汇总: Blender (CPU 可跑)
```

---

## 附录: 关键技术名词解释

| 术语 | 全称 | 说明 |
|------|------|------|
| **LRM** | Large Reconstruction Model | 单图→3D 的大规模重建模型 |
| **SDS** | Score Distillation Sampling | 用 2D 扩散模型梯蒸馏到 3D 表示 |
| **NeRF** | Neural Radiance Fields | 神经辐射场，连续 3D 场景表示 |
| **3DGS** | 3D Gaussian Splatting | 3D 高斯泼溅，替代 NeRF 的实时渲染方案 |
| **SLAT** | Structured LATent | TRELLIS 提出的统一 3D 潜空间表示 |
| **DiT** | Diffusion Transformer | Transformer 替代 U-Net 做扩散去噪 |
| **SMPL** | Skinned Multi-Person Linear | 参数化人体模型 (业界标准) |
| **BVH** | Biovision Hierarchy | 骨骼动画数据文件格式 |
| **FBX** | Filmbox | Autodesk 的 3D 交换格式 (含模型+骨骼+动画) |
| **GLB/GLTF** | GL Transmission Format | 开放 3D 标准，Web 端首选 |
| **PBR** | Physically Based Rendering | 基于物理的渲染 (Albedo/Normal/Metallic/Roughness) |
| **IK** | Inverse Kinematics | 逆向运动学，末端定位反推关节角度 |
| **Auto-Rigging** | 自动骨骼绑定 | 自动为 3D 模型生成骨架并计算蒙皮权重 |
| **Motion Retargeting** | 运动重定向 | 将一套骨骼动画数据迁移到不同骨架结构的角色 |
