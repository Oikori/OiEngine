# OiEngine — 项目说明（供 AI 协作者）

自研 C++23 游戏引擎，架构参考 Hazel。Windows + MinGW GCC + Ninja + CMake（≥3.20）。

- `Engine`：动态库（产物 `output/libEngine.dll`）
- `SandBox`：演示程序（产物 `output/SandBox.exe`，含飞机大战、太阳系、平台跳跃、阴影演示四个场景，F1/F2/F3/F5 切换）
- 渲染后端 OpenGL（glad + glfw），日志 spdlog，图像加载 stb_image，数学库 glm

## 目录结构

- `Engine/` — 库工程（include 根目录，见下）
  - `Engine/Engine/Core/` — `Base.h`（OI_API / OI_ASSERT / Scope / Ref）、`Log.h`、Application、Layer、LayerStack、Window、Input、Timer
  - `Engine/Engine/Events/` — 事件系统（Event、ApplicationEvent、KeyEvent、MouseEvent）
  - `Engine/Engine/Math/` — `Random` / `UUID` 工具类；几何数学已迁移至 glm（见下文），总入口是 `Engine/Engine/Math.h`
  - `Engine/Engine/Renderer/` — Renderer、Camera、Shader、Buffer、VertexArray、Framebuffer、ShadowMap 等抽象
  - `Engine/Engine/UI/` — `ImGuiLayer`（Dear ImGui 封装，作为 overlay 由 Application 内置）
  - `Engine/Platform/` — `OpenGL/`、`Windows/` 平台实现
- `SandBox/` — demo 可执行程序
  - `PlaneGame.h` / `PlaneGame.cpp` — 飞机大战（程序化几何 + 正交相机，全部顶点色，无图片资源）
  - `SolarSystem.h` / `SolarSystem.cpp` — 太阳系演示
  - `ShadowDemo.h` / `ShadowDemo.cpp` — 阴影演示（地面 + 几何体 + PBR 光照 + 可调方向光；详见「阴影」一节）
  - `Platformer/` — 平台跳跃小游戏（详见「平台跳跃手感」一节）
  - `RenderHelpers.h` — SandBox 共用的矩阵辅助（Translate/Scale/RotateZ/Orthographic）与碰撞判断
  - 四个场景都是 Layer，`main.cpp` 里的 `SceneSwitcherLayer`（overlay）用 F1/F2/F3/F5 切换 `SetActive`；
    场景内调试面板统一为 F4（原 PlaneGame 的 F3 调试键已迁移，避免与场景切换冲突）
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
- 已知问题：勾选「使用以前版本的微软拼音输入法」（旧版 IME 兼容模式）时，Ctrl+Space 切换输入法（先放 Ctrl 再放 Space）后 ImGui slider 点击会变成输入框且无法输入。根源：旧版 IME 吞掉 Ctrl 的释放事件，GLFW 一直认为 Ctrl 按下，ImGui 把点击判定为 Ctrl+Click。**引擎无 bug，新版输入法正常**；不加 workaround，建议用户关闭旧版兼容模式。若将来需要兜底：每帧用 `GetAsyncKeyState` 复核修饰键物理状态并**直接覆盖 `io.KeyCtrl` 等字段**（注意 ImGui 1.92 起 `io.AddKeyEvent` 只入队、`NewFrame()` 里才处理，且默认开启的 trickle 规则会把同键连续变化拆到多帧，走事件队列的修正会慢一帧/失效，必须放在 `NewFrame()` 之后直接赋值）

## 代码约定

- 命名：宏 `OI_`；类成员 `m_` 前缀；数学类型直接用 glm 命名（`glm::vec3`、`glm::mat4` 等）
- 风格：4 空格缩进，大括号不换行，`namespace OiEngine::Math {`
- 注释：中文为主，glm 等来源的注释保留英文原文
- `OI_API` 只用于**非模板类**（有 .cpp、需要跨 DLL 边界），如 `Random`、`UUID`；模板类型不要加（模板实例化的静态成员在 DLL 客户端无法链接）
- 调试断言用 `OI_ASSERT(条件)`

## 平台跳跃手感（SandBox/Platformer）

`PlatformerLayer` 的手感三要素集中在 `Tuning` 结构，游戏内 F4 面板可实时调参并显示状态：

1. **加速下落**：上升/下落两套重力（`riseGravity 2100` / `fallGravity 3900`），落地干脆；按住 ↓/S 再叠加 `fastFallExtra`
2. **顶点滞空**：空中 `|vy| <= apexBand`（120）时进入低重力带（`apexGravityScale 0.45`），退出用 `apexExitScale`（1.7）迟滞；
   资格条件是本次离地后 `vy` 曾超过 `apexBand`（`m_roseInAir`），从平台边缘自然跌落不触发
3. **可变跳跃高度**：起跳初速度固定 `jumpVelocity`（830，对应最大高度）；上升途中松开跳跃键对 `vy` 乘 `jumpCutScale`（0.55，
   低于 `jumpCutMinVelocity` 不再削减）——按住时长决定跳跃高度

其他：Coyote 0.10s、跳跃输入缓冲 0.12s；碰撞为分轴 + 8px 子步进的 AABB（防高速穿隧）；
游戏输入用 `Input::IsKeyPressed` 轮询，只被 `ImGui::GetIO().WantTextInput` 屏蔽
（**不能**用 `WantCaptureKeyboard`：打开 F4 面板后它常为 true，会让角色不听使唤）

## 渲染：2D 场景的 z 约定

- 正交投影 near=-10 / far=10 时 NDC z = -z/10，即**z 越大越靠前**（SandBox 里特效 0.1~0.2 > 投射物 0.08 > 玩家 0 > 敌机 -0.05 > 背景 -1/-2 就是按此排的）
- 深度测试为 GL_LESS：**同 z 的重叠几何只保留先画的**。做「底盘 + 图标/亮芯」这类覆盖时必须显式错开 z，
  例如 `builder.AddPolygon(points, color, 0.02f)` 或"光晕的 z 比本体小一点"，否则上层部件会被剔除、只看到底盘

## 相机 UBO（binding = 0）

- `Renderer::BeginScene(camera)` 把相机数据（`CameraData`，见 `Camera.h`）打包上传到 `Camera::UBOBinding = 0` 的 UBO；`Renderer::Submit` 只再设置 `u_Transform`
- shader 侧需声明同布局的 uniform 块（多个 stage 用到时，各 stage 中定义必须一致）：

  ```glsl
  layout(std140, binding = 0) uniform Camera {
      mat4 u_Projection;
      mat4 u_View;
      vec4 u_CameraPosition;  // xyz: 世界空间位置；w: 对齐占位
  };
  ```

- 相机视觉效果（如 PlaneGame 的受击震屏）应体现在 `Camera::GetViewMatrix()` 的返回值里，不要绕过 UBO 手动传 `u_view`
- `BeginScene` 只上传 UBO、不改变 GL 状态，需要相机矩阵的绘制应全部放在它之后（SolarSystem 已改为单次 BeginScene/EndScene 包裹整帧绘制）

## 光照（3D）

- 光源定义在 `Engine/Engine/Renderer/Light.h`：`DirectionalLight`（direction 为传播方向）、`PointLight`、`SpotLight`（inner/outerAngle 为半角、弧度）
- 场景用 `SceneEnvironment`（纯数据 struct，字段直用：`ambientColor`/`ambientIntensity`/`directionalLights`/`pointLights`/`spotLights`）；`Renderer::BeginScene(camera, scene)` 时打包上传（binding = 1 与 std140 打包细节都在 `Renderer.cpp` 里）
- shader 侧需自行声明同布局的 uniform 块（示例见 SandBox `SolarSystem.cpp` 的 `LIT_FRAGMENT_SHADER`）：

  ```glsl
  struct Light { vec4 positionType; vec4 directionRange; vec4 colorIntensity; vec4 params; };
  layout(std140, binding = 1) uniform Lighting {
      vec4 u_Ambient;      // rgb: 颜色；a: 强度
      int  u_LightCount;
      Light u_Lights[MAX_LIGHTS];
  };
  ```

- 光源上限 32（`Renderer.cpp` 里的 `MaxLights`），shader 里 `MAX_LIGHTS` 必须一致（`Renderer.cpp` 内有 `static_assert` 校验 std140 布局与大小）
- 打包顺序 Directional → Point → Spot；点光/聚光在 range 内平方衰减，聚光锥形因子用 `cos(innerAngle)/cos(outerAngle)` 插值
- 未声明该 uniform 块的 shader（如 unlit 绘制）不受影响；`SceneEnvironment` 是场景自有的普通对象，多个场景各建各的
- 光照按世界空间位置/法线计算；太阳系球体因球心在局部原点直接用位置方向当法线（变换后 `mat3(u_Transform) * a_Position`）
- SandBox 太阳系演示：太阳位置放暖色点光源 + 冷色环境光，UI 可调 Ambient / Sun intensity / Sun range

## 阴影（方向光 shadow map）

- `ShadowMap`（`Engine/Engine/Renderer/ShadowMap.h`）：只含一个深度附件（`FramebufferTextureFormat::DEPTH24`）的离屏帧缓冲；
  深度纹理为 NEAREST + CLAMP_TO_BORDER（边界外返回 1.0）；`BindDepthTexture(slot)` 绑定到指定纹理单元供着色器采样
- 每帧流程（在主场景 `BeginScene` 之前）：

  ```cpp
  const DirectionalLightShadow shadow = ComputeDirectionalLightShadow(sun, sceneCenter, sceneRadius);  // 正交投影覆盖包围球
  Renderer::BeginShadowPass(m_shadowMap, shadow.view, shadow.projection);  // 上传光源相机 + 阴影数据，绑定 FBO、清深度
  for (const auto& caster : casters) {
      Renderer::Submit(depthShader, caster.mesh, caster.transform);  // 只写深度的着色器（顶点变换即可，无需颜色输出）
  }
  Renderer::EndShadowPass(m_shadowMap);  // 解绑 FBO、恢复窗口视口
  ```

- 阴影 pass 复用**相机 UBO（binding = 0）**上传光源相机（深度着色器与普通着色器读法一致；主场景 `BeginScene` 会重新上传真正相机）；
  光空间矩阵与阴影参数走**阴影 UBO（binding = 2）**，shader 侧声明（与 `Renderer.cpp` 的 `ShadowData` std140 布局严格对应）：

  ```glsl
  layout(std140, binding = 2) uniform Shadow {
      mat4 u_LightSpaceMatrix;  // 世界空间 -> 光空间裁剪空间
      vec4 u_ShadowParams;      // x: 1/贴图尺寸（texel 大小）；y: 深度偏移基数（ShadowMap::SetBias）
  };
  ```

- 采样（3x3 PCF）示例见 SandBox `ShadowDemo.cpp` 的 `LIT_FRAGMENT_SHADER`：世界坐标乘 `u_LightSpaceMatrix` 后 `* 0.5 + 0.5`
  得到 [0,1] 光空间坐标，与深度纹理 `.r` 比较；bias 用斜率缩放（`max(bias * (1 - NdotL), bias * 0.2)`）防自阴影条纹；
  只对方向光乘可见度（点光 / 聚光分支不采样）
- 深度纹理通过 `ShadowMap::BindDepthTexture(slot)` 绑定（内部 `RenderCommand::BindTexture2D`），再用 `shader->SetInt("u_ShadowMap", slot)` 指定采样单元
- 底层接口：`Framebuffer` 增加 `GetDepthAttachmentRendererID()` 与 `DEPTH24` 格式；`RenderCommand::ClearDepth()`、`RenderCommand::BindTexture2D(slot, textureID)`
- SandBox F5「阴影演示」：地面 + 立方体 / 球体 + 可调太阳方向与 bias 的环绕相机；光照着色器为 Cook-Torrance PBR
  （GGX 分布 + Smith 几何项 + Schlick 菲涅尔；环境光用 Karis 解析 env BRDF 近似，输出前做 ACES tonemap 与 gamma 校正，曝光可调），
  材质（albedo / metallic / roughness / AO）逐实例通过 uniform 传入；UI 勾选可预览 shadow map（`ImGui::Image` 绑定深度纹理）

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
- 行尾约定：**新增文件一律使用 CRLF**（创建时就写 `\r\n`，不要事后依赖 checkout 转换）。
  仓库 `core.autocrlf=true`：提交时规范化为 LF 储存、checkout 时转回 CRLF，因此 git 的
  "LF will be replaced by CRLF" 提示属正常；若新文件误用 LF，用 PowerShell 统一转换：
  `[IO.File]::WriteAllText($p, [IO.File]::ReadAllText($p) -replace "`r`n", "`n" -replace "`n", "`r`n")`

## 协作习惯

- 改动后做**验证**：全量构建通过 + SandBox 运行观察；可编译期验证的尽量 `static_assert`
- 回复用中文，结论先行，专业名词保留英文
- 发现明显 bug 可以顺手修复并说明（用户接受「修复 + 报告」的方式）；改动保持与现有风格一致、尽量最小
