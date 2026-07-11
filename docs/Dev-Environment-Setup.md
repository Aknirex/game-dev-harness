# 开发环境搭建指南

> 版本: 1.0 · 日期: 2026-07-11
> 目标: 将 Harness C++ Core + Electron Shell 的开发环境跑起来

---

## 1. 必需工具

| 工具 | 最低版本 | 安装方式 | 用途 |
|------|----------|----------|------|
| **Visual Studio 2022** | 17.8+ | winget / 官网 | MSVC C++20 编译器 + MSBuild |
| **CMake** | 3.28+ | winget / 官网 | C++ 构建系统 |
| **vcpkg** | 2024.01+ | GitHub clone | C++ 包管理 |
| **Node.js** | 22 LTS | nvm-windows / 官网 | Electron 前端 |
| **Python** | 3.11+ | winget / 官网 | AI Pipeline Host |
| **Git** | 2.40+ | winget / 官网 | 版本控制 |

### 状态检查

运行以下命令确认已安装的工具：

```powershell
cmake --version     # 应 >= 3.28
node --version      # 应 >= 22 (当前环境: v24.18.0 ✅)
python --version    # 应 >= 3.11 (当前环境: 3.12.8 ✅)
git --version       # 应 >= 2.40 (当前环境: 2.52.0 ✅)
npm --version       # 应 >= 10 (当前环境: 11.16.0 ✅)
```

---

## 2. 安装步骤

### 2.1 Visual Studio 2022

```powershell
# 安装 Community 版 + C++ 桌面开发工作负载
winget install Microsoft.VisualStudio.2022.Community --override "--add Microsoft.VisualStudio.Workload.NativeDesktop --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.Windows11SDK.22621"

# 或从官网下载安装器: https://visualstudio.microsoft.com/downloads/
# 安装时勾选 "使用 C++ 的桌面开发"
```

验证：
```powershell
where msbuild
# → C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe
```

### 2.2 CMake

```powershell
winget install Kitware.CMake

# 验证
cmake --version
```

### 2.3 vcpkg

```powershell
# 克隆到固定位置（推荐 C:\dev\vcpkg）
git clone https://github.com/Microsoft/vcpkg.git C:\dev\vcpkg
C:\dev\vcpkg\bootstrap-vcpkg.bat

# 设置环境变量
[Environment]::SetEnvironmentVariable("VCPKG_ROOT", "C:\dev\vcpkg", "User")

# 重新打开终端后验证
vcpkg version
```

### 2.4 前端依赖

```powershell
# 项目内安装（不需要全局）
# 进入 electron-shell 目录后执行:
npm install
```

---

## 3. 项目初始化

### 3.1 C++ Core

```powershell
cd game-dev-harness

# 配置（首次运行，vcpkg 自动下载依赖 ~20分钟）
cmake --preset default

# 编译 Debug
cmake --build build/Debug --config Debug

# 运行 Core (验证)
./build/Debug/harness-core.exe --version
```

### 3.2 Electron Shell

```powershell
cd game-dev-harness/electron-shell
npm install
npm run dev          # 启动 Vite + Electron 热重载
```

### 3.3 全链路启动

```powershell
# 方式1: Electron 自动启动 Core
cd electron-shell
npm run dev          # main.ts 中 spawn Core 进程

# 方式2: 手动分别启动
# 终端1 - Core
./build/Debug/harness-core.exe
# 终端2 - Electron
cd electron-shell && npm run dev
```

---

## 4. 项目结构

```
game-dev-harness/
├── CMakeLists.txt              # 顶层 CMake：src/ + third_party/
├── CMakePresets.json           # CMake 预设：vcpkg toolchain + x64-windows
├── vcpkg.json                  # C++ 依赖声明
├── src/
│   ├── core/                   # Harness Core 主程序
│   │   └── main.cpp
│   ├── harness/                # Core 框架层
│   │   ├── tool.h              # Tool 接口
│   │   ├── tool_registry.h/cpp
│   │   ├── execution_engine.h/cpp
│   │   ├── permission_manager.h/cpp
│   │   └── sandbox_engine.h/cpp
│   ├── mcp/                    # MCP Client
│   │   ├── mcp_client.h/cpp
│   │   └── mcp_tool_adapter.h/cpp
│   ├── agent/                  # Agent Layer
│   │   ├── llm_client.h
│   │   ├── openai_client.h/cpp
│   │   ├── anthropic_client.h/cpp
│   │   ├── agent_loop.h/cpp
│   │   └── context_builder.h/cpp
│   ├── ipc/                    # IPC 层
│   │   └── ipc_channel.h/cpp
│   └── common/                 # 公共工具
│       └── json_utils.h
├── third_party/                # 非 vcpkg 管理的第三方库
│   └── cxxmcp/                 # MCP C++ Client
├── electron-shell/             # Electron 前端
│   ├── package.json
│   ├── vite.config.ts
│   ├── src/
│   │   ├── main.ts             # Electron 主进程
│   │   ├── preload.ts          # Context bridge
│   │   ├── renderer/           # React UI
│   │   │   ├── App.tsx
│   │   │   └── components/
│   │   └── ipc/                # IPC TypeScript 实现
│   │       └── IpcClient.ts
│   └── index.html
├── python_pipeline/            # Python AI Pipeline Host
│   ├── server.py
│   └── requirements.txt
├── tests/                      # C++ 测试
│   ├── tool_registry_test.cpp
│   ├── permission_test.cpp
│   ├── sandbox_test.cpp
│   └── agent_loop_test.cpp
├── references/
│   ├── api-docs/
│   └── projects/
└── docs/
```

---

## 5. C++ 依赖配置

### vcpkg.json（项目根目录）

```json
{
  "$schema": "https://raw.githubusercontent.com/microsoft/vcpkg-tool/main/docs/vcpkg.schema.json",
  "name": "game-dev-harness",
  "version": "0.1.0",
  "dependencies": [
    "cpp-httplib",
    "glaze",
    "spdlog",
    "doctest",
    "asio",
    "taskflow",
    "sqlite3",
    "sol2",
    "lua"
  ],
  "builtin-baseline": "latest"
}
```

### CMakePresets.json（项目根目录）

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "default",
      "displayName": "Windows x64 Debug",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/${presetName}",
      "toolchainFile": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "CMAKE_CXX_STANDARD": "20",
        "CMAKE_CXX_STANDARD_REQUIRED": "ON"
      }
    },
    {
      "name": "release",
      "displayName": "Windows x64 Release",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/${presetName}",
      "toolchainFile": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "CMAKE_CXX_STANDARD": "20",
        "CMAKE_CXX_STANDARD_REQUIRED": "ON"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "default",
      "configurePreset": "default"
    },
    {
      "name": "release",
      "configurePreset": "release"
    }
  ]
}
```

### CMakeLists.txt 骨架

```cmake
cmake_minimum_required(VERSION 3.28)
project(game-dev-harness VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# vcpkg 依赖
find_package(httplib CONFIG REQUIRED)     # cpp-httplib
find_package(glaze CONFIG REQUIRED)        # glaze
find_package(spdlog CONFIG REQUIRED)       # spdlog
find_package(doctest CONFIG REQUIRED)      # doctest
find_package(asio CONFIG REQUIRED)          # asio
find_package(Taskflow CONFIG REQUIRED)     # taskflow
find_package(sqlite3 CONFIG REQUIRED)      # sqlite3
find_package(lua CONFIG REQUIRED)          # lua 5.4

# sol2 (header-only, FetchContent)
include(FetchContent)
FetchContent_Declare(
  sol2
  GIT_REPOSITORY https://github.com/ThePhD/sol2.git
  GIT_TAG v3.3.0
)
FetchContent_MakeAvailable(sol2)

# Harness Core 库（供 Core 可执行文件和测试链接）
add_library(harness-core-lib OBJECT
  src/harness/tool_registry.cpp
  src/harness/execution_engine.cpp
  src/harness/permission_manager.cpp
  src/harness/sandbox_engine.cpp
  src/ipc/ipc_channel.cpp
  # ...
)
target_link_libraries(harness-core-lib PUBLIC
  httplib::httplib glaze::glaze spdlog::spdlog asio::asio
  Taskflow::Taskflow SQLite::SQLite3 Lua::Lua
)
target_include_directories(harness-core-lib PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${sol2_SOURCE_DIR}/include
)

# Core 可执行文件
add_executable(harness-core
  src/core/main.cpp
)
target_link_libraries(harness-core PRIVATE harness-core-lib)

# 测试目标
add_executable(harness-tests
  tests/tool_registry_test.cpp
  tests/permission_test.cpp
  # ...
)
target_link_libraries(harness-tests PRIVATE
  harness-core-lib
  doctest::doctest
)
```

---

## 6. Electron Shell 配置

### package.json

```json
{
  "name": "harness-electron-shell",
  "version": "0.1.0",
  "private": true,
  "main": "dist-electron/main.js",
  "scripts": {
    "dev": "vite",
    "build": "vite build && electron-builder",
    "lint": "eslint . --ext ts,tsx",
    "typecheck": "tsc --noEmit"
  },
  "dependencies": {
    "react": "^19.0",
    "react-dom": "^19.0",
    "react-markdown": "^10.0",
    "shiki": "^3.0",
    "monaco-editor": "^0.52"
  },
  "devDependencies": {
    "electron": "^43.0",
    "electron-builder": "^25.0",
    "vite": "^7.0",
    "vite-plugin-electron": "^0.29",
    "typescript": "^5.8",
    "@types/react": "^19.0",
    "@types/react-dom": "^19.0",
    "eslint": "^9.0"
  }
}
```

### electron-shell/src/ipc/IpcClient.ts

```typescript
import { ChildProcess, spawn } from 'child_process';
import { EventEmitter } from 'events';

interface JsonRpcRequest {
  jsonrpc: '2.0';
  id: number;
  method: string;
  params?: any;
}

interface JsonRpcResponse {
  jsonrpc: '2.0';
  id: number;
  result?: any;
  error?: { code: number; message: string };
}

export class HarnessIpcClient extends EventEmitter {
  private coreProcess: ChildProcess | null = null;
  private requestId = 0;
  private pending = new Map<number, { resolve: any; reject: any }>();
  private buffer = '';

  async start(corePath: string): Promise<void> {
    this.coreProcess = spawn(corePath, ['--stdio'], {
      stdio: ['pipe', 'pipe', 'inherit'],
    });

    this.coreProcess.stdout!.on('data', (chunk: Buffer) => {
      this.buffer += chunk.toString();
      this.processBuffer();
    });

    // Handshake
    const result = await this.request('initialize', {
      protocolVersion: '1.0',
      clientInfo: { name: 'Harness Electron Shell', version: '0.1.0' },
    });
    this.emit('initialized', result);
  }

  async request(method: string, params?: any, timeout = 30000): Promise<any> {
    const id = ++this.requestId;
    const msg: JsonRpcRequest = { jsonrpc: '2.0', id, method, params };
    this.sendRaw(msg);

    return new Promise((resolve, reject) => {
      this.pending.set(id, { resolve, reject });
      setTimeout(() => {
        this.pending.delete(id);
        reject(new Error(`Request ${method} timed out after ${timeout}ms`));
      }, timeout);
    });
  }

  notify(method: string, params?: any): void {
    this.sendRaw({ jsonrpc: '2.0', method, params });
  }

  async shutdown(): Promise<void> {
    try {
      await this.request('shutdown');
    } catch {}
    this.notify('exit');
    this.coreProcess?.kill();
  }

  private sendRaw(data: any): void {
    const json = JSON.stringify(data);
    const header = `Content-Length: ${Buffer.byteLength(json)}\r\nContent-Type: application/vnd.harness+json; charset=utf-8\r\n\r\n`;
    this.coreProcess!.stdin!.write(header + json);
  }

  private processBuffer(): void {
    const headerMatch = this.buffer.match(/Content-Length: (\d+)\r\n.*?\r\n\r\n/);
    if (!headerMatch) return;

    const contentLength = parseInt(headerMatch[1]);
    const headerEnd = headerMatch.index! + headerMatch[0].length;

    if (this.buffer.length < headerEnd + contentLength) return;

    const body = this.buffer.slice(headerEnd, headerEnd + contentLength);
    this.buffer = this.buffer.slice(headerEnd + contentLength);

    try {
      const msg = JSON.parse(body);
      if (msg.id !== undefined && this.pending.has(msg.id)) {
        const { resolve, reject } = this.pending.get(msg.id)!;
        this.pending.delete(msg.id);
        if (msg.error) reject(msg.error);
        else resolve(msg.result);
      } else if (msg.method) {
        this.emit(msg.method, msg.params);
      }
    } catch (e) {
      console.error('Failed to parse IPC message:', e);
    }

    // 继续处理缓冲区中可能存在的更多帧
    if (this.buffer.includes('Content-Length:')) {
      this.processBuffer();
    }
  }
}
```

---

## 7. 常见问题

### Q: vcpkg 首次下载太慢？
```powershell
# 设置镜像（国内）
set VCPKG_BINARY_SOURCES="clear;http,http://mirrors.tuna.tsinghua.edu.cn/vcpkg/cache,readwrite"
vcpkg install
```

### Q: CMake 找不到 vcpkg toolchain？
确保 `VCPKG_ROOT` 环境变量已设置且终端已重启。

```powershell
echo $env:VCPKG_ROOT
# → C:\dev\vcpkg
```

### Q: sol2 编译错误 `lua.hpp not found`？
sol2 是 header-only，不需要 CMake 构建。确保 `lua` 包已安装且 `find_package(lua CONFIG REQUIRED)` 成功。

### Q: Electron 热重载不工作？
`vite-plugin-electron` 需要 `nodeIntegration: true` 和 `contextIsolation: false`（开发模式）。生产模式使用 preload + contextBridge。

### Q: Core 进程退出但 Electron 窗口没有反应？
检查 Core 的 stderr 输出（spdlog 默认输出到 stderr）。在开发模式下将 stderr 重定向到终端：

```typescript
this.coreProcess = spawn(corePath, ['--stdio'], {
  stdio: ['pipe', 'pipe', 'inherit'], // stderr → inherit
});
```

---

## 8. 快速验证检查表

完成后逐一确认：

- [ ] `cmake --preset default && cmake --build build/default --config Debug` 编译成功
- [ ] `./build/default/harness-core.exe --version` 输出版本号
- [ ] `cd electron-shell && npm install && npm run dev` 启动 Electron 窗口
- [ ] Electron 控制台显示 `[Harness Core] initialized`
- [ ] `ctest --test-dir build/default --output-on-failure` 测试全部通过
