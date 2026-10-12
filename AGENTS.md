# OiEngine — 项目说明（供 AI 协作者）

自研 C++23 游戏引擎，架构参考 Hazel。Windows + MinGW GCC + Ninja + CMake（≥3.20）。

- `Engine`：动态库（产物 `output/libEngine.dll`）
- `SandBox`：演示程序（产物 `output/SandBox.exe`，含飞机大战与太阳系两个场景，F1/F2 切换）
- 渲染后端 OpenGL（glad + glfw），日志 spdlog，图像加载 stb_image，数学库 glm

## 目录结构

- `Engine/` — 库工程（include 根目录，见下）
  - `Engine/Engine/Core/` — `Base.h`（OI_API / OI_ASSERT / Scope / Ref）、`Log.h`、Application、Layer、LayerStack、Window、Input、Timer
  - `Engine/Engine/Events/` — 事件系统（Event、ApplicationEvent、KeyEvent、MouseEvent）
  - `Engine/Engine/Math/` — `Random` / `UUID` 工具类；几何数学已迁移至 glm（见下文），总入口是 `Engine/Engine/Math.h`
  - `Engine/Engine/Renderer/` — Renderer、Camera、Shader、Buffer、VertexArray、Framebuffer 等抽象
  - `Engine/Engine/UI/` — `ImGuiLayer`（Dear ImGui 封装，作为 overlay 由 Application 内置）
  - `Engine/Platform/` — `OpenGL/`、`Windows/` 平台实现
- `SandBox/` — demo 可执行程序
  - `PlaneGame.h` / `PlaneGame.cpp` — 飞机大战（程序化几何 + 正交相机，全部顶点色，无图片资源）
  - `SolarSystem.h` / `SolarSystem.cpp` — 太阳系演示
  - `RenderHelpers.h` — SandBox 共用的矩阵辅助（Translate/Scale/RotateZ/Orthographic）与碰撞判断
  - 两个场景都是 Layer，`main.cpp` 里的 `SceneSwitcherLayer`（overlay）用 F1/F2 切换 `SetActive`
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
- shell 是 Windows PowerShell 5.1：不支持 `&&` 和三元 `?:`；含逗号的链接参数（如 `-Wl,--out-implib,...`）需加引号

## 测试（数学）

自建数学库已删除、全面改用 glm，原 `test/` 验证用例一并删除（见 `test/README.md`）。
数学相关改动以全量构建 + SandBox 运行观察为准；可编译期验证的尽量写 `static_assert`。

## UI（Dear ImGui）

- 引擎内置 `ImGuiLayer`（overlay，`Application::Init` 自动创建）：业务层实现 `Layer::OnImGuiRender()` 后直接用 `ImGui::` API 绘制；渲染时机在场景之后、SwapBuffers 之前，最小化时跳过
- 事件：overlay 逆序最先收到事件，UI 占用鼠标/键盘（`WantCaptureMouse/Keyboard`）时标记 `handled`，不再传入游戏层（有 1 帧延迟，正常）
- SandBox 直接 `#include <imgui.h>` 即可：include 路径与 `IMGUI_API=dllimport` 经 Engine target 导出
- dll 边界：imgui 源码以 OBJECT 库并入 `Engine.dll`，编译时 `IMGUI_API/IMGUI_IMPL_API=__declspec(dllexport)`；**必须用 `target_compile_options` 传 `-D`，不能用 `target_compile_definitions`（CMake 会拆解值中的括号，生成畸形的 `-D`）**
- 版本为 docking 分支（tag `v1.92.9b-docking`）；升级：`git submodule update --remote vendor/imgui`
- 中文界面需在 `ImGuiLayer::OnAttach` 里加载字体：`io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msyh.ttc", 18.0f, nullptr, io.Fonts->GetGlyphRangesChineseSimplifiedCommon())`（默认字体不含中文）

## 代码约定

- 命名：宏 `OI_`；类成员 `m_` 前缀；数学类型直接用 glm 命名（`glm::vec3`、`glm::mat4` 等）
- 风格：4 空格缩进，大括号不换行，`namespace OiEngine::Math {`
- 注释：中文为主，glm 等来源的注释保留英文原文
- `OI_API` 只用于**非模板类**（有 .cpp、需要跨 DLL 边界），如 `Random`、`UUID`；模板类型不要加（模板实例化的静态成员在 DLL 客户端无法链接）
- 调试断言用 `OI_ASSERT(条件)`

## 渲染：2D 场景的 z 约定

- 正交投影 near=-10 / far=10 时 NDC z = -z/10，即**z 越大越靠前**（SandBox 里特效 0.1~0.2 > 投射物 0.08 > 玩家 0 > 敌机 -0.05 > 背景 -1/-2 就是按此排的）
- 深度测试为 GL_LESS：**同 z 的重叠几何只保留先画的**。做「底盘 + 图标/亮芯」这类覆盖时必须显式错开 z，
  例如 `builder.AddPolygon(points, color, 0.02f)` 或"光晕的 z 比本体小一点"，否则上层部件会被剔除、只看到底盘

## 数学库（glm）

自建 Vector / Matrix / Quaternion 已删除，全面改用 `vendor/glm`（submodule；默认以
`GLM_BUILD_LIBRARY=ON` 编译出静态库 `libglm.a`，经 `Engine` target 以 PUBLIC 传递给 SandBox）。

- `Engine/Engine/Math.h` = 引擎数学入口（包含 `<glm/glm.hpp>` 与 `Random`/`UUID`），被 `OiEngine.h` 一并包含
- 类型直接用 `glm::vec2/3/4`、`glm::mat3/4`；几何运算用 `glm::dot/cross/normalize/length` 等
- 变换函数 `glm::translate/rotate/scale/perspective/ortho/lookAt` 在 `<glm/gtc/matrix_transform.hpp>`；
  上传 uniform 用 `glm::value_ptr`（`<glm/gtc/type_ptr.hpp>`）——两者都不在 `glm/glm.hpp` 里，需显式 include
- 与 GLSL 一致：列主序；`m * v` 为列向量乘法；矩阵上传写作 `glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(m))`
- 角度取弧度，度→弧度用 `glm::radians`
- SandBox 的矩阵辅助（`RenderHelpers.h`）与太阳系相机（透视 / lookAt）直接用 glm 实现

### 已知陷阱

1. `glm::vec4(1.0f)` 只填 x，值是 **(1, 0, 0, 0)**（不是白色）。颜色 / tint 默认值必须写全分量
   `glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)`，否则 alpha 为 0 会被着色器 `discard`。`glm::mat4(1.0f)` 例外——单位阵
2. 扩展功能不在 `glm/glm.hpp`（见上述 matrix_transform / type_ptr），用到时忘 include 会以缺失符号报错

## Git / 其他

- `.gitignore`：`.idea`、`output`、`test/*`；`build/` 由 CMake 生成的 .gitignore 覆盖
- `vendor/glfw`、`vendor/spdlog`、`vendor/imgui`、`vendor/glm` 是 submodule（分支跟踪：glfw `master`、spdlog `v1.x`、imgui `docking`、glm `master`；更新用 `git submodule update --remote <path>`）；`glad`、`stb_image` 直接入库
- 行尾为 LF，git 提示 "LF will be replaced by CRLF" 属正常

## 协作习惯

- 改动后做**验证**：全量构建通过 + SandBox 运行观察；可编译期验证的尽量 `static_assert`
- 回复用中文，结论先行，专业名词保留英文
- 发现明显 bug 可以顺手修复并说明（用户接受「修复 + 报告」的方式）；改动保持与现有风格一致、尽量最小
