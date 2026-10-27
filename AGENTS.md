# OiEngine — 项目说明（供 AI 协作者）

自研 C++23 游戏引擎，架构参考 Hazel。Windows + MinGW GCC + Ninja + CMake（≥3.20）。

- `Engine`：动态库（产物 `output/libEngine.dll`）
- `SandBox`：演示程序（产物 `output/SandBox.exe`，含飞机大战、太阳系、平台跳跃、阴影演示、室内多光源、体积云六个场景，F1/F2/F3/F5/F6/F7 切换）
- `Survivors`：独立可执行工程（产物 `output/Survivors.exe`，类《吸血鬼幸存者》俯视角生存游戏）
- 渲染后端 OpenGL（glad + glfw），日志为自研 `OI_*` 宏（`std::format`，见 [日志](docs/日志.md)），图像加载 stb_image，数学库 glm

## 文档导航

子系统细节按主题拆在 `docs/`，本文件只保留每次会话都需要的全局信息；改某块代码前先看对应文档。

| 文档 | 内容 |
|---|---|
| [渲染基础](docs/渲染基础.md) | 2D 场景 z 约定、相机 UBO（binding = 0） |
| [光照与后处理](docs/光照与后处理.md) | 光照 SSBO、Forward+ cluster 剔除、HDR、IBL、IndoorDemo 参考实现 |
| [阴影](docs/阴影.md) | 方向光 ShadowMap、聚光灯 ShadowMapArray、采样与 bias |
| [体积云](docs/体积云.md) | VolumetricCloudDemo 的地形 / 云层 / 管线 / 参数 |
| [平台跳跃](docs/平台跳跃.md) | 手感三要素、移动平台、尖刺与存档点 |
| [幸存者](docs/幸存者.md) | Survivors 的玩法、属性、输入、敌人管理与渲染 |
| [ImGui 与输入](docs/ImGui与输入.md) | ImGui 集成、中文字体、IME 吞键与自动化输入 |
| [数学库](docs/数学库.md) | glm 用法、include 陷阱、数学测试约定 |
| [日志](docs/日志.md) | `OI_*` 宏、格式串与 GL 调试输出 |
| [室内多光源渲染方案](docs/室内多光源渲染方案.md) | IndoorDemo 的设计取舍与实施记录 |

新增子系统时，细节写进 `docs/`，并在上表补一行。

## 目录结构

- `Engine/` — 库工程（include 根目录，见下）
  - `Engine/Engine/Core/` — `Base.h`（OI_API / OI_ASSERT / Scope / Ref）、`Log.h`（OI_TRACE/INFO/WARN/ERROR/CRITICAL，见 [日志](docs/日志.md)）、Application、Layer、LayerStack、Window、Input、Timer
  - `Engine/Engine/Events/` — 事件系统（Event、ApplicationEvent、KeyEvent、MouseEvent）
  - `Engine/Engine/Math/` — `Random` / `UUID` 工具类；几何数学已迁移至 glm（见 [数学库](docs/数学库.md)），总入口是 `Engine/Engine/Math.h`
  - `Engine/Engine/Renderer/` — Renderer、Camera、Shader、Buffer、VertexArray、Framebuffer、UniformBuffer、**StorageBuffer（SSBO）**、ShadowMap / **ShadowMapArray**、**PostProcess**、**LightClustering**、**EnvironmentProbe** 等抽象
  - `Engine/Engine/UI/` — `ImGuiLayer`（Dear ImGui 封装，作为 overlay 由 Application 内置；见 [ImGui 与输入](docs/ImGui与输入.md)）
  - `Engine/Platform/` — `OpenGL/`、`Windows/` 平台实现
- `SandBox/` — demo 可执行程序
  - `PlaneGame.h` / `PlaneGame.cpp` — 飞机大战（程序化几何 + 正交相机，全部顶点色，无图片资源）
  - `SolarSystem.h` / `SolarSystem.cpp` — 太阳系演示
  - `ShadowDemo.h` / `ShadowDemo.cpp` — 阴影演示（地面 + 几何体 + PBR 光照 + 可调方向光；见 [阴影](docs/阴影.md)）
  - `IndoorDemo.h` / `IndoorDemo.cpp` — 室内多光源演示（房间 + 吊灯/壁灯/台灯/射灯 + 阳光穿窗、Forward+ 剔除、聚光灯阴影、IBL 探针；见 [光照与后处理](docs/光照与后处理.md) 与 [方案](docs/室内多光源渲染方案.md)）
  - `VolumetricCloudDemo.h` / `VolumetricCloudDemo.cpp` — 仿《地平线》体积云场景（程序化雪山 + 半分辨率光线步进云层 + 大气/太阳合成；见 [体积云](docs/体积云.md)）
  - `Platformer/` — 平台跳跃小游戏（多段式关卡 + 移动平台 + 尖刺陷阱 + 存档点；见 [平台跳跃](docs/平台跳跃.md)）
  - `RenderHelpers.h` — SandBox 共用的矩阵辅助（Translate/Scale/RotateZ/Orthographic）与碰撞判断
  - 六个场景都是 Layer，`main.cpp` 里的 `SceneSwitcherLayer`（overlay）用 F1/F2/F3/F5/F6/F7 切换 `SetActive`；
    场景内调试面板统一为 F4（原 PlaneGame 的 F3 调试键已迁移，避免与场景切换冲突）
  - 新增 `.cpp` 要同时加进 `SandBox/CMakeLists.txt`（SandBox 是显式源文件列表；`Engine/` 是 GLOB_RECURSE，
    新增引擎源文件只需重跑 `cmake --preset mingw-debug`）
- `Survivors/` — 第二个引用 `Engine` 的独立可执行工程：类《吸血鬼幸存者》俯视角生存游戏
  （产物 `output/Survivors.exe`，源码只有 `main.cpp` + `SurvivorsGame.*` + `SurvivorsArt.*`；见 [幸存者](docs/幸存者.md)）
- `vendor/` — glad、glfw、spdlog、stb_image、imgui、glm（glfw/spdlog/imgui/glm 登记为 git submodule）
- `test/` — 原数学库验证用例已删除（详见 `test/README.md`）；不参与 CMake，被 .gitignore 忽略
- `output/`、`build/` — 构建产物，不入库

头文件包含：include 根是 `Engine/` 目录，写作 `#include "Engine/Core/Base.h"`、`#include "Platform/OpenGL/OpenGLContext.h"`。

## 构建

```powershell
# 日常构建（构建目录已配置好）
cmake --build build/cmake-build-debug

# 首次 / 修改 CMake 后重新配置（Ninja + CLion 自带 MinGW，见 CMakePresets.json）
cmake --preset mingw-debug      # 或 mingw-release
```

- 产物在 `output/`：`libEngine.dll`、`SandBox.exe`
- 命令行直接运行 `output/SandBox.exe` 需要 MinGW 运行时 DLL 在 PATH 中（否则秒退无输出）：
  `$env:PATH = "D:\Program Files\JetBrains\CLion 2025.2.1\bin\mingw\bin;$env:PATH"`；CLion 运行配置会自动处理
- 编译器路径记录在 `build/cmake-build-debug/CMakeCache.txt`（当前为 CLion 自带 MinGW GCC 15）
- **注意 `cmake` 版本**：`CMakePresets.json` 用的 preset schema 版本 10 需要 CMake ≥ 3.31，而 PATH 里的
  `D:\Program Files\CMake\bin\cmake.exe` 是 3.30（会报 `Unrecognized "version" field`）。重新配置/构建请用 CLion 自带的：
  `$cmake = 'D:\Program Files\JetBrains\CLion 2025.2.1\bin\cmake\win\x64\bin\cmake.exe'; & $cmake --preset mingw-debug; & $cmake --build build/cmake-build-debug`
- shell 是 Windows PowerShell 5.1：不支持 `&&` 和三元 `?:`；含逗号的链接参数（如 `-Wl,--out-implib,...`）需加引号

## 代码约定

- 命名：宏 `OI_`；类成员 `m_` 前缀；数学类型直接用 glm 命名（`glm::vec3`、`glm::mat4` 等）
- 风格：4 空格缩进，大括号不换行，`namespace OiEngine::Math {`
- 注释：中文为主，glm 等来源的注释保留英文原文
- `OI_API` 只用于**非模板类**（有 .cpp、需要跨 DLL 边界），如 `Random`、`UUID`；模板类型不要加（模板实例化的静态成员在 DLL 客户端无法链接）
- 调试断言用 `OI_ASSERT(条件)`
- 日志用 `OI_TRACE/OI_INFO/OI_WARN/OI_ERROR/OI_CRITICAL`；格式串必须是编译期字面量（`std::format` 的 consteval 检查），
  `glGetString()` 之类的 `const GLubyte*` 要先 `reinterpret_cast<const char*>`（详见 [日志](docs/日志.md)）

## Git / 其他

- `.gitignore`：`.idea`、`output`、`test/*`；`build/` 由 CMake 生成的 .gitignore 覆盖
- `vendor/glfw`、`vendor/spdlog`、`vendor/imgui`、`vendor/glm` 是 submodule（分支跟踪：glfw `master`、spdlog `v1.x`、imgui `docking`、glm `master`；更新用 `git submodule update --remote <path>`）；`glad`、`stb_image` 直接入库
- 行尾约定：**新增文件一律使用 CRLF**（创建时就写 `\r\n`，不要事后依赖 checkout 转换）。
  仓库 `core.autocrlf=true`：提交时规范化为 LF 储存、checkout 时转回 CRLF，因此 git 的
  "LF will be replaced by CRLF" 提示属正常；若新文件误用 LF，用 PowerShell 统一转换：
  `[IO.File]::WriteAllText($p, [IO.File]::ReadAllText($p) -replace "`r`n", "`n" -replace "`n", "`r`n")`

## 协作习惯

- 改动后做**验证**：全量构建通过 + SandBox 运行观察；可编译期验证的尽量 `static_assert`
- 自动化验证（注入按键 / 截图）优先用**鼠标点击**而不是模拟键盘：中文输入法会吞按键，
  见 [ImGui 与输入](docs/ImGui与输入.md) 的「输入法」一节（`keybd_event` 也可能被吞，且窗口非前台时 `PostMessage` 不生效）
- 回复用中文，结论先行，专业名词保留英文
- 发现明显 bug 可以顺手修复并说明（用户接受「修复 + 报告」的方式）；改动保持与现有风格一致、尽量最小
