# OiEngine

**C++23** 实时渲染引擎与游戏框架，架构参考 [Hazel](https://github.com/TheCherno/Hazel)。引擎编译为动态库，
上层是可执行程序：一个装五个演示场景的 SandBox，以及两个独立小游戏 Platformer / Survivors。

- **构建**：Windows + MinGW GCC 15 + Ninja + CMake（≥ 3.31）
- **图形**：OpenGL（glad 加载 + GLFW 窗口）
- **其他**：glm 数学 · stb_image 图像 · miniaudio 音频 · Dear ImGui（docking）
- **日志**：自研 `OI_*` 宏，`std::format` 编译期格式检查

| 产物 | 说明 |
| --- | --- |
| `output/libEngine.dll` | 引擎动态库：Application / Layer / 渲染 / 音频 / ImGui |
| `output/SandBox.exe` | 五个演示场景：飞机大战、太阳系、阴影、室内多光源、体积云 |
| `output/Platformer.exe` | 平台跳跃小游戏：六段关卡、移动平台、尖刺陷阱、存档点 |
| `output/Survivors.exe` | 类《吸血鬼幸存者》俯视角生存游戏：10 关关卡制 |

## 特性

**渲染**

- 前向渲染管线抽象：`Renderer` / `Shader` / `VertexArray` / `Buffer` / `Texture` / `Framebuffer`，
  OpenGL 实现集中在 `Engine/Platform/OpenGL/`
- 相机数据走 UBO（binding = 0），shader 侧声明同布局的 uniform 块即可
- HDR 渲染 + ACES tonemap 后处理，可逐场景开关
- 光源走 SSBO（最多 256 个），物理平方反比衰减 + Forward+ cluster 剔除
- 方向光 ShadowMap、聚光灯 ShadowMapArray（最多 8 个投影光源），bias 可按光源调
- IBL 环境探针（`EnvironmentProbe`），室内 PBR 场景用它补环境光
- 体积云：程序化地形 + 半分辨率光线步进 + 深度感知降噪 + 大气 / 太阳合成

**框架**

- `Application` / `Layer` / `LayerStack` 分层与事件系统，Dear ImGui 作为内置 overlay
- 窗口、输入、计时、`OI_ASSERT`；ImGui 中文字体与 IME（解绑输入法 + 物理按键兜底）
- `Random` / `UUID` 工具类，几何数学统一用 glm

**音频与内容**

- miniaudio 封装：2D / 3D 声源，decode（一次性音效）与 stream（流式 BGM）两种资源模型
- 全部演示美术都是程序化几何（顶点色网格），不依赖图片资源；音频由各工程的 `tools/generate_audio.py` 生成

## 快速开始

### 环境要求

- Windows 10/11 x64
- CMake **≥ 3.31**（`CMakePresets.json` 用 preset schema v10，3.30 会报 `Unrecognized "version" field`）
- Ninja、MinGW-w64 GCC 15（CLion 2025.2 自带的那套即可）
- （可选）CLion 2025.2+，presets 里带了 CLion 的 MinGW toolchain 配置

### 获取源码

```powershell
git clone --recurse-submodules https://github.com/Oikori/OiEngine.git
# 已克隆过则补一次子模块
git submodule update --init --recursive
```

### 配置与构建

```powershell
# 首次 / 改过 CMake 后：配置 + 全量构建
$cmake = 'D:\Program Files\JetBrains\CLion 2025.2.1\bin\cmake\win\x64\bin\cmake.exe'   # 换成你的 cmake（≥ 3.31）
& $cmake --preset mingw-debug
& $cmake --build build/cmake-build-debug

# 之后日常构建
cmake --build build/cmake-build-debug

# 只构建某个目标
cmake --build build/cmake-build-debug --target SandBox
```

Release 用 `mingw-release` / `build/cmake-build-release`。产物统一输出到 `output/`，
三个可执行工程各自的 `assets/`（着色器、音频）由 CMake `POST_BUILD` 复制到 `output/assets/`。

### 运行

```powershell
# 命令行直接启动需要 MinGW 运行时 DLL 在 PATH 里，否则程序秒退且没有输出
$env:PATH = "D:\Program Files\JetBrains\CLion 2025.2.1\bin\mingw\bin;$env:PATH"

cd output
.\SandBox.exe      # 或 .\Platformer.exe / .\Survivors.exe
```

必须在 `output/` 目录下启动：程序按运行时工作目录读取 `assets/...`，从别处启动会找不到着色器 / 音频。

## 目录结构

```
OiEngine/
├─ Engine/                引擎动态库（include 根目录：Engine/）
│  ├─ Engine/Core/        Application / Layer / LayerStack / Window / Input / Log / Timer / Base
│  ├─ Engine/Events/      事件系统
│  ├─ Engine/Audio/       miniaudio 封装
│  ├─ Engine/Math/        Random / UUID（几何数学用 glm）
│  ├─ Engine/Renderer/    Renderer / Camera / Shader / Buffer / ShadowMap / PostProcess …
│  ├─ Engine/UI/          ImGuiLayer
│  └─ Platform/           OpenGL 与 Windows 平台实现
├─ SandBox/               五个演示场景（PlaneGame / SolarSystem / ShadowDemo / IndoorDemo / VolumetricCloudDemo）
├─ Platformer/            平台跳跃小游戏
├─ Survivors/             幸存者小游戏
├─ vendor/                第三方库（部分为 git submodule）
├─ docs/                  子系统文档
├─ output/                构建产物（不入库）
├─ CMakePresets.json      预设：mingw-debug / mingw-release
└─ AGENTS.md              全局约定与文档索引（贡献代码前先看）
```

头文件包含以 `Engine/` 为根，写作 `#include "Engine/Core/Base.h"`、`#include "Platform/OpenGL/OpenGLContext.h"`。

## 文档

| 文档 | 内容 |
| --- | --- |
| [渲染基础](docs/渲染基础.md) | 2D 场景 z 约定、相机 UBO |
| [着色器](docs/着色器.md) | 各工程 `assets/shader/` 的组织、`#type` 分段与加载 |
| [光照与后处理](docs/光照与后处理.md) | 光照 SSBO、Forward+ cluster 剔除、HDR、IBL |
| [阴影](docs/阴影.md) | 方向光 ShadowMap、聚光灯 ShadowMapArray、采样与 bias |
| [体积云](docs/体积云.md) | 地形 / 云层 / 管线 / 参数 |
| [室内多光源渲染方案](docs/室内多光源渲染方案.md) | IndoorDemo 的设计取舍与实施记录 |
| [平台跳跃](docs/平台跳跃.md) | 手感三要素、移动平台、尖刺与存档点 |
| [幸存者](docs/幸存者.md) | 玩法、属性、输入、敌人管理 |
| [ImGui 与输入](docs/ImGui与输入.md) | ImGui 集成、中文字体、IME（解绑输入法 + 物理按键兜底） |
| [数学库](docs/数学库.md) | glm 用法、include 陷阱、测试约定 |
| [日志](docs/日志.md) | `OI_*` 宏、格式串与 GL 调试输出 |
| [音频](docs/音频.md) | miniaudio 集成、2D / 3D 声源、资源模型 |

## 第三方依赖

| 库 | 用途 | 引入方式 |
| --- | --- | --- |
| [glad](https://github.com/Dav1dde/glad) | OpenGL 函数加载 | 直接入库 |
| [GLFW](https://github.com/glfw/glfw) | 窗口 / 输入 / GL 上下文 | submodule（`master`） |
| [Dear ImGui](https://github.com/ocornut/imgui) | 调试与工具 UI | submodule（`docking`） |
| [glm](https://github.com/icaven/glm) | 数学库 | submodule（`master`） |
| [stb_image](https://github.com/nothings/stb) | 图像加载 | 直接入库 |
| [miniaudio](https://github.com/mackron/miniaudio) | 音频 | 直接入库 |
| [spdlog](https://github.com/gabime/spdlog) | 日志后端（`OI_*` 宏的彩色控制台 + 文件轮转输出） | submodule（`v1.x`） |

更新子模块：`git submodule update --remote vendor/glfw`（其余同理）。

## 开发约定

- 样式：4 空格缩进、大括号不换行、`m_` 成员前缀、`OI_` 宏前缀；注释以中文为主
- 日志统一 `OI_TRACE/OI_INFO/OI_WARN/OI_ERROR/OI_CRITICAL`，格式串必须是编译期字面量
- 新增引擎源文件会被 `GLOB_RECURSE` 自动纳入，只需重新 configure；SandBox 是显式源文件列表，新增 `.cpp` 要同步改 `SandBox/CMakeLists.txt`
- 新增文件一律使用 CRLF 行尾
- 更多约定（`OI_ENGINE_API` 使用边界、事件系统、自动化验证方式）见 [AGENTS.md](AGENTS.md)

## 许可

[MIT](LICENSE) © 2026 Harvey
