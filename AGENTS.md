# OiEngine — 项目说明（供 AI 协作者）

自研 C++23 游戏引擎，架构参考 Hazel。Windows + MinGW GCC + Ninja + CMake（≥3.20）。

- `Engine`：动态库（产物 `output/libEngine.dll`）
- `SandBox`：演示程序（产物 `output/SandBox.exe`，含飞机大战、太阳系、平台跳跃、阴影演示、室内多光源五个场景，F1/F2/F3/F5/F6 切换）
- 渲染后端 OpenGL（glad + glfw），日志 spdlog，图像加载 stb_image，数学库 glm

## 目录结构

- `Engine/` — 库工程（include 根目录，见下）
  - `Engine/Engine/Core/` — `Base.h`（OI_API / OI_ASSERT / Scope / Ref）、`Log.h`（OI_TRACE/INFO/WARN/ERROR/CRITICAL，见「日志」一节）、Application、Layer、LayerStack、Window、Input、Timer
  - `Engine/Engine/Events/` — 事件系统（Event、ApplicationEvent、KeyEvent、MouseEvent）
  - `Engine/Engine/Math/` — `Random` / `UUID` 工具类；几何数学已迁移至 glm（见下文），总入口是 `Engine/Engine/Math.h`
  - `Engine/Engine/Renderer/` — Renderer、Camera、Shader、Buffer、VertexArray、Framebuffer、UniformBuffer、**StorageBuffer（SSBO）**、ShadowMap / **ShadowMapArray**、**PostProcess**、**LightClustering**、**EnvironmentProbe** 等抽象
  - `Engine/Engine/UI/` — `ImGuiLayer`（Dear ImGui 封装，作为 overlay 由 Application 内置）
  - `Engine/Platform/` — `OpenGL/`、`Windows/` 平台实现
- `SandBox/` — demo 可执行程序
  - `PlaneGame.h` / `PlaneGame.cpp` — 飞机大战（程序化几何 + 正交相机，全部顶点色，无图片资源）
  - `SolarSystem.h` / `SolarSystem.cpp` — 太阳系演示
  - `ShadowDemo.h` / `ShadowDemo.cpp` — 阴影演示（地面 + 几何体 + PBR 光照 + 可调方向光；详见「阴影」一节）
  - `IndoorDemo.h` / `IndoorDemo.cpp` — 室内多光源演示（房间 + 吊灯/壁灯/台灯/射灯 + 阳光穿窗、Forward+ 剔除、聚光灯阴影、IBL 探针；详见「室内多光源」一节）
  - `Platformer/` — 平台跳跃小游戏（多段式关卡 + 移动平台 + 尖刺陷阱 + 存档点，详见「平台跳跃手感」一节）
  - `RenderHelpers.h` — SandBox 共用的矩阵辅助（Translate/Scale/RotateZ/Orthographic）与碰撞判断
  - 五个场景都是 Layer，`main.cpp` 里的 `SceneSwitcherLayer`（overlay）用 F1/F2/F3/F5/F6 切换 `SetActive`；
    场景内调试面板统一为 F4（原 PlaneGame 的 F3 调试键已迁移，避免与场景切换冲突）
  - 新增 `.cpp` 要同时加进 `SandBox/CMakeLists.txt`（SandBox 是显式源文件列表；`Engine/` 是 GLOB_RECURSE，
    新增引擎源文件只需重跑 `cmake --preset mingw-debug`）
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

### 关卡元素（陷阱 / 移动平台 / 存档点）

关卡从起点营地起分 §A~§F 六段（尖刺教学 → 阶梯 + 伸缩尖刺 → 深渊 + 水平移动平台 → 垂直升降 + 壁刺走廊 →
天花板闸门 → 终局跳台），全部数据都在 `PlatformerLayer::BuildLevel()` 里按「顶面高度」书写：

- `AddGround/AddBlock` 参数是 `top`（顶面），内部换算成中心 + 半尺寸；`AddMovingPlatform` 给的则是**中心**的路径两端
- **深坑一律不设坑底**（§B 缺口 / §D 深渊 / §F 缺口）：相机 y 固定为 0、视野底只有 -450，坑底机关玩家根本看不到，
  掉下去就该按坠落处理（提示「掉下去了」）。尖刺只在**看得见的地方**出现（地面 / 平台顶 / 贴墙 / 吊顶 / 顶盖上）
- **移动平台**与静态平台共用 `Platform` 与同一套碰撞：`speed > 0` 即为移动平台，沿 `pathStart → pathEnd` 往返、
  端点停顿 `pauseTime`；`UpdatePlatforms` 每帧把实际位移写进 `velocity`，玩家踩在它上面时（`m_groundPlatformIndex`）
  把这部分位移叠加进自己的移动
- Y 轴碰撞解算按**最小推出距离**判断「落顶面 / 撞底面」，不能只看位移方向：上行升降台载人时玩家的净位移是向上的，
  按方向判定会被误判成撞头而塞到平台下方（就是"平台一向上移动角色就掉下去"那个 bug）
- **尖刺** `Spike::position` 是附着面中心，`direction` 决定齿朝向（朝上 / 吊顶朝下 / 贴墙左右）；判定盒由
  `SpikeHitbox()` 按旋转后的 AABB 计算，并做两处宽容：横向乘 `spikeHitboxShrink`（0.78）、靠基座 0.2 倍高度内不判定
- **伸缩尖刺**（`cyclic`）的时间轴在 `SpikeExtendAt(phase)`：缩回 50% → 预警露尖 10%（**不致命**，配琥珀色脉冲光晕提示）
  → 快速弹出 7% → 完全伸出 23% → 收回 10%。`spikeDeadlyExtend`（0.45）是开始致命的总闸门阈值；
  §F 的天花板闸门就是「趁完全缩回时一口气跑过去」的关卡，顶盖上面还补了尖刺堵住翻越近路
- **存档点**更新 `m_respawnPoint`；尖刺 / 坠落死亡只回到最近存档点（金币、用时保留）并触发震屏 + 闪红，
  R 才是整关重建（`ResetLevel` 里调 `BuildLevel()`，云层和远山放在 `BuildBackdrop()` 里只生成一次，避免重开时随机重排）
- F4 面板「陷阱宽容度」可调上面两个阈值，并可勾选**显示判定盒**（平台绿 / 尖刺红 / 玩家白）方便对齐数值

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

- 光源定义在 `Engine/Engine/Renderer/Light.h`：`DirectionalLight`（direction 为传播方向）、`PointLight`、`SpotLight`
  （inner/outerAngle 为半角、弧度；`shadowIndex`/`shadowBias` 见「阴影」一节）
- **光源数据走 SSBO（binding = 3，std430）**，上限 `MaxLights = 256`（`Light.h` 常量，shader 侧 `MAX_LIGHTS` 必须一致；
  `Renderer.cpp` 内有 `static_assert` 校验布局与大小）。场景仍用 `SceneEnvironment`（纯数据 struct：
  `ambientColor`/`ambientIntensity`/`directionalLights`/`pointLights`/`spotLights`），`Renderer::BeginScene(camera, scene)` 时打包上传
- shader 侧声明（示例见 `IndoorDemo.cpp` 的 `LIT_FRAGMENT_SHADER`）：

  ```glsl
  #define MAX_LIGHTS 256
  #define MAX_SHADOW_CASTERS 8

  struct Light {
      vec4 positionType;   // xyz: 世界空间位置（Point/Spot）或传播方向（Directional）；w: 类型
      vec4 directionRange; // xyz: 方向（Spot）；w: 影响半径（Point/Spot）
      vec4 colorIntensity; // rgb: 颜色；w: 强度
      vec4 params;         // x: 内锥角余弦；y: 外锥角余弦（Spot）
      vec4 flags;          // x: 阴影矩阵槽位（-1 不投影）；y: 阴影深度偏移基数
  };
  layout(std430, binding = 3) readonly buffer Lighting {
      vec4  u_Ambient;      // rgb: 颜色；a: 强度
      ivec4 u_Counts;       // x: 光源数；y: 阴影矩阵数；z: 每 cluster 光源上限；w: 备用
      vec4  u_ClipPlanes;   // x: near；y: far；z: 1/log(far/near)；w: 备用（cluster 深度切片用）
      ivec4 u_Grid;         // x/y: tilesX / tilesY；z: 深度切片数；w: 是否启用 cluster 剔除
      Light u_Lights[MAX_LIGHTS];
      mat4  u_ShadowMatrices[MAX_SHADOW_CASTERS];
  };
  ```

- 打包顺序 Directional → Point → Spot；**衰减统一为窗函数截断的平方反比**
  （`w = saturate(1 - (d/range)^4)`，`attenuation = w² / max(d², 0.01)`），即 `intensity` = 距离 1 个单位处的照度，
  室内点光源典型取值 20 ~ 200（旧场景的 intensity 需要按此重新调参：太阳系的太阳点光源用了 320）
- 聚光锥形因子 `cos(innerAngle)/cos(outerAngle)` 乘在衰减之上
- 未声明该 buffer 的 shader（如 unlit 绘制）不受影响；`SceneEnvironment` 是场景自有的普通对象，多个场景各建各的
- 光照按世界空间位置/法线计算；太阳系球体因球心在局部原点直接用位置方向当法线（变换后 `mat3(u_Transform) * a_Position`）
- `Camera` 增加了 `GetNearClip()/GetFarClip()`（默认 0.1 / 1000），透视场景**务必重写为与投影矩阵一致的值**（cluster 深度切片依赖它）

## Forward+ 光源剔除（cluster）

- `LightClustering`（`Engine/Engine/Renderer/LightClustering.h`）：按 屏幕 16×16 px tile × 24 层视空间深度切片划分 cluster，
  compute shader 逐 cluster 与光源包围球求交，结果写进两个 SSBO：
  - binding 4：`uint u_ClusterLightCounts[clusterCount]`（每 cluster 光源数）
  - binding 5：`uint u_ClusterLightIndices[clusterCount * 48]`（光源索引列表，`MaxLightsPerCluster = 48`）
- 开关与派发：`Renderer::SetLightCullingEnabled(true)`；`Renderer::BeginScene` 在上传光照 SSBO 后自动
  `LightClustering::Dispatch()`（清计数 + compute + `MemoryBarrierShaderStorage()`）。窗口 resize 时网格随 `Renderer::OnWindowResize` 重建。
  注意这是**全局开关**，切换场景会残留，各场景需在自己的 `OnUpdate` 开头显式声明（见「HDR 后处理」一节）
- shader 侧按同一套索引规则定位 cluster（布局：slice 优先，其次 tile 行，最后 tile 列）：

  ```glsl
  layout(std430, binding = 4) readonly buffer ClusterLightCounts { uint u_ClusterLightCounts[]; };
  layout(std430, binding = 5) readonly buffer ClusterLightIndices { uint u_ClusterLightIndices[]; };
  // 片元：viewDepth -> slice = floor(log(d/near) * u_ClipPlanes.z * u_Grid.z)
  //       tile = gl_FragCoord.xy / 16；index = (slice * tilesY + tileY) * tilesX + tileX
  ```

- `u_Grid.w == 0`（未启用）时着色器应退回遍历全部光源（`IndoorDemo` 里两条路径并存，可对照排查）
- 实测（1600×900，211 个光源，RTX 5060 Ti）：剔除关 4.67 ms → 剔除开 1.19 ms（约 3.9×），画面一致
- 调试统计 `LightClustering::ReadStats()` 会读回 GPU（同步等待），UI 里应低频调用（IndoorDemo 每 30 帧一次）；
  统计里的 `overflowClusters` 表示光源列表被撑爆的 cluster 数——真出现时优先缩小光源 range，而不是盲目加大上限
- compute 着色器：`Shader::CreateCompute(name, src)` + `shader->Dispatch(groupsX, groupsY, groupsZ)`

## HDR 后处理

- `PostProcess`（`Engine/Engine/Renderer/PostProcess.h`）：`Renderer::SetHDREnabled(true)` 后场景渲染到
  RGBA16F 离屏目标（`FramebufferTextureFormat::RGBA16F`），`EndScene` 时全屏 tonemap（ACES + gamma + 曝光）输出到默认帧缓冲
- **开启 HDR 的场景着色器只输出线性 HDR 颜色**（不要再自己 ACES / gamma），曝光统一用 `Renderer::SetExposure()`
- 默认关闭时行为与旧路径完全一致（场景着色器各自 tonemap）
- **HDR / 光源剔除是引擎全局开关，每个场景必须在 `OnUpdate` 开头显式声明自己的状态**：HDR 场景
  `Renderer::SetHDREnabled(true); Renderer::SetExposure(m_exposure);`，非 HDR 场景 `Renderer::SetHDREnabled(false);`。
  未激活的场景在 `OnUpdate` 开头就 `return`、什么都不设，所以只写不关会让开关残留到下一个场景——
  HDR 开关残留到非 HDR 场景时颜色被二次 tonemap，画面整体泛白（`SetLightCullingEnabled` 同理：残留会让不读 cluster 的场景空跑 compute）
- `PostProcess::GetColorTextureRendererID()` 可拿到 HDR 颜色纹理做 ImGui 预览
- 后续接入 bloom 等 pass 时，扩展 `PostProcess::EndScene` 里的 pass 链即可（现只有 tonemap 一个 pass）

## 环境探针（IBL）

- `EnvironmentProbe`（`Engine/Engine/Renderer/EnvironmentProbe.h`）：`Capture(position, callback)` 把场景画进 RGBA16F cubemap
  （调用方回调里用探针给的 `faceView/faceProjection` 绘制，**不要用依赖探针自身的着色器**，否则递归），
  然后引擎自动做 irradiance 余弦卷积（32²）与 GGX prefilter（128²，8 级 mip）
- 光照着色器侧：漫反射用 `texture(u_IrradianceMap, N)`，镜面用
  `textureLod(u_PrefilteredMap, reflect(-V, N), roughness * u_ProbeMaxLod)` × Karis `EnvBRDFApprox`
  （本引擎未预计算 BRDF LUT，镜面项沿用解析近似）
- 纹理单元约定：2 = irradiance、3 = prefilter（`BindIrradiance(slot)` / `BindPrefiltered(slot)`）
- 捕获里的直射太阳通常要按比例缩小（`IndoorDemo` 的 "Probe sun scale"，默认 0.45），否则间接光会把阳光重复算一遍
- 光路改动（灯具 / 阳光）后需要重新 `Capture` —— `IndoorDemo` 面板有 "Recompute probe" 按钮（约百毫秒级）

## 日志

- `OI_TRACE/OI_INFO/OI_WARN/OI_ERROR/OI_CRITICAL` 已实装（`Engine/Engine/Core/Log.{h,cpp}`，不依赖 spdlog）：
  宏用 `std::format` 格式化（支持 `{0}` 位置参数与普通 `{}`），Info 及以下写 stdout、Error 及以上写 stderr，
  Trace 默认被级别过滤（`Log::SetLevel` 可调）
- 格式串必须是编译期字面量（`std::format` 的 consteval 检查）；`glGetString()` 之类的 `const GLubyte*` 要先
  `reinterpret_cast<const char*>`
- OpenGL 调试输出（`OI_DEBUG` 构建）会经 `glDebugMessageCallback` 送到日志——MEDIUM/HIGH 级别的 GL 错误会直接打在控制台

## 阴影（方向光 + 聚光灯）

- 方向光：`ShadowMap`（`Engine/Engine/Renderer/ShadowMap.h`）：只含一个深度附件（`FramebufferTextureFormat::DEPTH24`）的离屏帧缓冲；
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
- **聚光灯阴影**（`ShadowMapArray`，深度纹理数组，每盏一层，默认 1024² × 8 层）：

  ```cpp
  const SpotLightShadow shadow = ComputeSpotLightShadow(spot);           // 透视投影，fov = 2 * outerAngle
  Renderer::SetLightShadowMatrix(spot.shadowIndex, shadow.lightSpaceMatrix);  // 槽位 = SpotLight::shadowIndex
  Renderer::BeginShadowPass(m_spotShadowMaps, spot.shadowIndex, shadow.view, shadow.projection);
  for (const auto& caster : casters) { Renderer::Submit(depthShader, caster.mesh, caster.transform); }
  Renderer::EndShadowPass(m_spotShadowMaps);
  ```

  光空间矩阵随光照 SSBO 的 `u_ShadowMatrices[MAX_SHADOW_CASTERS]` 一起上传，着色器按 `u_Lights[i].flags.x` 取槽位并采样
  `sampler2DArray`（纹理单元 0，`u_SpotShadowMaps`），bias 取 `flags.y`（逐光源）。点光源阴影（cube/双抛物面）暂未实现
- 底层接口：`Framebuffer` 的 `GetDepthAttachmentRendererID()` 与 `DEPTH24`/`RGBA16F` 格式；`RenderCommand::ClearDepth()`、
  `BindTexture2D/BindTexture2DArray/BindTextureCube`、`SetDepthTest`、`MemoryBarrierShaderStorage`
- SandBox F5「阴影演示」：地面 + 立方体 / 球体 + 可调太阳方向与 bias 的环绕相机；光照着色器为 Cook-Torrance PBR
  （GGX 分布 + Smith 几何项 + Schlick 菲涅尔；环境光用 Karis 解析 env BRDF 近似，输出线性 HDR 交给后处理链），
  材质（albedo / metallic / roughness / AO）逐实例通过 uniform 传入；UI 勾选可预览 shadow map（`ImGui::Image` 绑定深度纹理）

## 室内多光源（SandBox/IndoorDemo，F6）

把上面几块拼起来的完整示例，也是新光照路径的参考实现：

- 房间（16 × 4 × 12，-z 墙上有窗洞 + 自发光窗玻璃）+ 家具 + 灯具，全部程序化生成（`AppendBox/AppendSphere/AppendQuad`）
- 光源：1 束穿窗阳光（方向光 + 4096² 阴影）+ 4 盏向下吊灯（聚光灯，各占一层阴影贴图数组）+ 4 壁灯 / 2 台灯（点光源）
  + 可加到 240 盏的额外点光源（压力测试，`range` 1.2 ~ 2.5 的室内尺度小光源）
- 走 HDR 路径 + `Renderer::SetLightCullingEnabled(true)`；面板可实时对比剔除开 / 关、聚光灯阴影开关、曝光、探针开关与"Recompute probe"
- 相机是轨道相机但**把机位夹在房间内**（`IndoorCamera::GetPosition` 里 `glm::clamp`）——室内观察用轨道相机必须限制，
  否则很容易转到墙外 / 天花板外
- 性能参考（1600×900）：11 个光源约 5 ms 上限（vsync 200Hz）；211 个光源、剔除开启 1.19 ms


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
