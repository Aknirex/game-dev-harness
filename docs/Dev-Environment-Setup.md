# Game Dev Harness — 开发环境搭建指南

**文档版本**: 1.1  
**更新日期**: 2026-07-12  
**适用范围**: `game-dev-harness` 项目本地开发环境

---

## 目录

1. [环境概览](#1-环境概览)
2. [所需工具与安装路径](#2-所需工具与安装路径)
3. [环境初始化](#3-环境初始化)
4. [项目结构](#4-项目结构)
5. [构建命令](#5-构建命令)
6. [xmake.lua 骨架说明](#6-xmakelua-骨架说明)
7. [Electron Shell](#7-electron-shell)
8. [IPC Client (TypeScript)](#8-ipc-client-typescript)
9. [FAQ](#9-faq)
10. [验证清单](#10-验证清单)

---

## 1. 环境概览

| 组件 | 选型 | 版本 | 路径 |
|------|------|------|------|
| 构建系统 | xmake | 3.0.9 | `D:\tools\xmake\xmake.exe` |
| 包管理器 | xrepo (xmake 内置) | — | 随 xmake |
| 编译器 | clang (clang-cl 驱动模式) | 22.1.8 | `D:\tools\llvm\bin\clang.exe` |
| 链接器 | lld-link | 22.1.8 | `D:\tools\llvm\bin\lld-link.exe` |
| Windows SDK | Visual Studio 2019 Community | 16.x | `C:\Program Files (x86)\Microsoft Visual Studio\2019\Community` |
| 辅助工具 | CMake | 4.3.3 | `D:\tools\cmake\cmake-4.3.3-windows-x86_64\bin` |
| 辅助工具 | Ninja | 1.13.2 | `D:\tools\ninja` |
| Electron | electron | — | `electron\` 子目录 |
| C++ 标准 | C++20 | — | — |

> **说明**: 项目已从 CMake + vcpkg + MSVC 迁移至 xmake + clang (clang-cl) + lld-link。Visual Studio 仅保留以提供 Windows SDK 头文件和库，不再用作编译器。`D:\vcpkg` 目录保留供参考，本项目不再使用。

---

## 2. 所需工具与安装路径

### 2.1 xmake 3.0.9

- 安装路径: `D:\tools\xmake`
- 可执行文件: `D:\tools\xmake\xmake.exe`
- 官网: https://xmake.io

### 2.2 LLVM / clang 22.1.8

- 安装路径: `D:\tools\llvm`
- 编译器: `D:\tools\llvm\bin\clang.exe`, `D:\tools\llvm\bin\clang++.exe`
- 链接器: `D:\tools\llvm\bin\lld-link.exe`
- 驱动模式: **clang-cl**（兼容 MSVC 命令行风格）

clang-cl 是 clang 的 MSVC 兼容驱动程序，支持 `/` 风格选项，可以直接使用 Windows SDK 头文件和库。

### 2.3 Visual Studio 2019 Community（Windows SDK）

- 安装路径: `C:\Program Files (x86)\Microsoft Visual Studio\2019\Community`
- 用途: 提供 Windows SDK 头文件、库、rc 编译器等
- 通过 `vcvars64.bat` 设置 `INCLUDE`、`LIB`、`PATH` 等环境变量

### 2.4 CMake & Ninja（辅助工具）

- CMake 4.3.3: `D:\tools\cmake\cmake-4.3.3-windows-x86_64\bin\cmake.exe`
- Ninja 1.13.2: `D:\tools\ninja\ninja.exe`
- 本项目构建不直接使用 CMake/Ninja，保留供其他子项目或工具链使用。

### 2.5 xrepo 包镜像

xrepo 使用 TUNA 镜像加速下载:

```
xmake g --pkg_searchdirs="https://mirrors.tuna.tsinghua.edu.cn/xmake/packages"
```

当前依赖:
- `nlohmann_json` — JSON 解析库
- `doctest` — 轻量级 C++ 测试框架

---

## 3. 环境初始化

### 3.1 env.ps1 脚本

项目提供 `D:\tools\env.ps1` 用于一键初始化开发环境。在任意终端中执行:

```powershell
. D:\tools\env.ps1
```

该脚本执行以下操作:

1. 调用 `vcvars64.bat` 设置 Windows SDK 路径（`INCLUDE`、`LIB`、`PATH`）
2. 将 `D:\tools\llvm\bin` 加入 `PATH`（clang、clang++、lld-link）
3. 将 `D:\tools\xmake` 加入 `PATH`（xmake）
4. 将 `D:\tools\cmake\cmake-4.3.3-windows-x86_64\bin` 加入 `PATH`
5. 将 `D:\tools\ninja` 加入 `PATH`
6. 设置 `VCPKG_ROOT=D:\vcpkg`（保留用于参考）

### 3.2 验证环境

初始化后，执行以下命令确认工具链就绪:

```powershell
clang-cl --version          # 应显示 clang version 22.1.8
xmake --version             # 应显示 xmake 3.0.9
lld-link /?                 # 应显示 lld-link 帮助信息
```

---

## 4. 项目结构

```
game-dev-harness/
├── xmake.lua                    # 项目构建定义（顶层）
├── .xmake/                      # xmake 本地缓存目录（自动生成）
├── src/
│   ├── core/
│   │   └── main.cpp             # 主程序入口
│   ├── harness/
│   │   └── tool_registry.cpp    # 工具注册表
│   └── ipc/
│       ├── frame_codec.cpp      # 帧编解码
│       └── json_rpc.cpp         # JSON-RPC 实现
├── tests/
│   ├── test_main.cpp            # 测试入口
│   ├── frame_codec_test.cpp
│   ├── json_rpc_test.cpp
│   └── tool_registry_test.cpp
├── electron/                    # Electron 前端工程
├── docs/
│   └── Dev-Environment-Setup.md # 本文档
└── include/
```

> 旧文件 `CMakeLists.txt`、`CMakePresets.json`、`vcpkg.json` 已移除。所有构建配置统一在 `xmake.lua` 中管理。

---

## 5. 构建命令

所有命令在项目根目录 `D:\00yw\game-dev-harness` 下执行。

### 5.1 配置（重新生成）

```powershell
xmake f -c -y
```

- `-c` 清除已有配置并重新检测工具链
- `-y` 自动确认（非交互式）

### 5.2 构建（Release 模式，默认）

```powershell
xmake
# 或
xmake -y
```

构建产物默认输出至 `build\` 目录。

### 5.3 构建 Debug 模式

```powershell
xmake f -m debug -y && xmake -y
```

Debug 模式开启调试符号，关闭优化。

### 5.4 运行测试

```powershell
xmake run harness-tests
```

`harness-tests` 是测试目标，使用 `doctest` 框架。

### 5.5 运行核心程序

```powershell
xmake run harness-core
```

### 5.6 清理

```powershell
xmake c          # 清理构建产物
xmake f -c       # 清除所有缓存（含依赖下载缓存）
```

---

## 6. xmake.lua 骨架说明

以下是项目根目录 `xmake.lua` 的完整内容及逐段注释:

```lua
-- 项目元信息
set_project("game-dev-harness")
set_version("0.1.0")
set_languages("c++20")

-- 工具链: 使用 clang-cl 驱动 + lld-link 链接器
set_toolchains("clang-cl")
set_config("ld", "lld-link")

-- 内置构建模式 (自动处理 debug/release 标志)
add_rules("mode.debug", "mode.release")

-- 第三方依赖（由 xrepo 自动下载与集成）
add_requires("nlohmann_json", "doctest")

-- 核心静态库
target("harness-core-lib")
    set_kind("static")
    add_files("src/harness/tool_registry.cpp")
    add_files("src/ipc/frame_codec.cpp")
    add_files("src/ipc/json_rpc.cpp")
    add_includedirs("src", {public = true})
    add_packages("nlohmann_json", {public = true})
    set_warnings("all")

-- 主可执行程序
target("harness-core")
    set_kind("binary")
    add_files("src/core/main.cpp")
    add_deps("harness-core-lib")

-- 测试可执行程序
target("harness-tests")
    set_kind("binary")
    add_files("tests/test_main.cpp")
    add_files("tests/frame_codec_test.cpp")
    add_files("tests/json_rpc_test.cpp")
    add_files("tests/tool_registry_test.cpp")
    add_deps("harness-core-lib")
    add_packages("doctest")
```

### 关键配置说明

| 配置项 | 含义 |
|--------|------|
| `set_toolchains("clang-cl")` | 强制使用 clang 的 MSVC 兼容驱动，而非 MSVC 编译器 |
| `set_config("ld", "lld-link")` | 指定链接器为 lld-link（LLVM 链接器） |
| `add_rules("mode.debug", "mode.release")` | 启用 `xmake f -m debug` / `xmake f -m release` 模式切换 |
| `add_requires(...)` | 声明项目依赖，xmake 首次构建时自动通过 xrepo 下载 |
| `add_packages(..., {public = true})` | 公开继承：依赖此库的 target 自动获得相同包 |

---

## 7. Electron Shell

本章节与之前版本保持一致。

### 7.1 概述

项目使用 Electron 作为桌面外壳，通过 IPC 与 C++ 后端通信。

`electron/` 子目录为独立的 Node.js 项目，包含 `package.json` 和相关前端代码。

### 7.2 环境需求

- Node.js ≥ 18.x
- npm 或 pnpm

### 7.3 启动步骤

```powershell
cd electron
npm install
npm run dev
```

Electron 启动后会自动加载 `electron/main.js`（主进程），前端页面位于 `electron/renderer/` 目录。

### 7.4 与 C++ 后端的通信

Electron 主进程通过 `child_process.spawn()` 启动 C++ 编译产物（`harness-core`），使用 `stdin/stdout` 管道发送 JSON-RPC 请求并接收响应。帧协议由 `src/ipc/frame_codec.cpp` 和 TypeScript 侧对应的编解码模块实现。

---

## 8. IPC Client (TypeScript)

本章节与之前版本保持一致。

### 8.1 概述

TypeScript IPC 客户端封装了与 C++ 后端的通信。代码位于 `electron/` 项目中。

### 8.2 核心模块

- `electron/src/ipc/frameCodec.ts` — 帧编解码（与 C++ 侧 `frame_codec.cpp` 协议一致）
- `electron/src/ipc/jsonRpc.ts` — JSON-RPC 客户端

### 8.3 使用示例

```typescript
// 连接 C++ 后端并发送请求
import { JsonRpcClient } from './ipc/jsonRpc';

const client = new JsonRpcClient();
await client.connect();  // 启动 harness-core 子进程

const result = await client.call('tool.registry.list', {});
console.log(result);
```

---

## 9. FAQ

### 9.1 构建相关

**Q: xmake 首次下载依赖太慢怎么办？**

```powershell
xmake g --pkg_searchdirs="https://mirrors.tuna.tsinghua.edu.cn/xmake/packages"
```

该命令将 xrepo 包搜索源切换至清华 TUNA 镜像，大幅提升国内下载速度。此设置全局生效，仅需执行一次。

**Q: xmake 提示找不到 clang 或 clang-cl？**

确保已在当前终端中执行初始化脚本:

```powershell
. D:\tools\env.ps1
```

然后确认 `clang-cl --version` 能正常输出版本信息。如果仍找不到，检查 `D:\tools\llvm\bin` 是否在 `PATH` 中:

```powershell
echo $env:PATH
```

**Q: 构建时提示找不到 Windows SDK 头文件？**

执行 `. D:\tools\env.ps1` 确保 `vcvars64.bat` 已正确设置 `INCLUDE` 和 `LIB` 环境变量。VS 2019 Community 必须已安装 "使用 C++ 的桌面开发" 工作负载。

### 9.2 Electron 相关

**Q: Electron 启动后无法连接 C++ 后端？**

1. 确保已先构建 `harness-core` (`xmake`)，二进制文件已生成
2. 检查 Electron 主进程中 `harness-core` 的路径配置是否正确
3. 在终端中单独运行 `xmake run harness-core` 确认后端本身无异常

**Q: npm install 在 Electron 目录下失败？**

- 检查 Node.js 版本 ≥ 18.x
- 尝试清理缓存: `npm cache clean --force`
- 如在国内，可设置 npm 镜像: `npm config set registry https://registry.npmmirror.com`

---

## 10. 验证清单

完成环境搭建后，按顺序执行以下命令验证:

```powershell
# 1. 初始化环境
. D:\tools\env.ps1

# 2. 验证工具链
clang-cl --version
xmake --version

# 3. 配置项目
cd D:\00yw\game-dev-harness
xmake f -c -y

# 4. 构建 Release
xmake -y

# 5. 运行测试
xmake run harness-tests

# 6. 构建 Debug
xmake f -m debug -y && xmake -y

# 7. 运行核心程序
xmake run harness-core

# 8. Electron 验证（可选）
cd electron
npm install
npm run dev
```

所有步骤通过即为环境搭建成功。
