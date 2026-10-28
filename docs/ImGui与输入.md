# ImGui 与输入

> 返回 [AGENTS.md](../AGENTS.md)。相关：[平台跳跃](平台跳跃.md)、[幸存者](幸存者.md)。

## Dear ImGui 集成

- 引擎内置 `ImGuiLayer`（overlay，`Application::Init` 自动创建）：业务层实现 `Layer::OnImGuiRender()` 后直接用 `ImGui::` API 绘制；渲染时机在场景之后、SwapBuffers 之前，最小化时跳过
- 事件：overlay 逆序最先收到事件，UI 占用鼠标/键盘（`WantCaptureMouse/Keyboard`）时标记 `handled`，不再传入游戏层（有 1 帧延迟，正常）
- SandBox 直接 `#include <imgui.h>` 即可：include 路径与 `IMGUI_API=dllimport` 经 Engine target 导出
- 窗口与输入法（IME）：引擎创建窗口时就解绑输入法，并另有一条 `GetAsyncKeyState` 物理按键通道兜底
  （见下文「输入法」一节），所以中文输入法开着也能正常操作游戏
- dll 边界：imgui 源码以 OBJECT 库并入 `Engine.dll`，编译时 `IMGUI_API/IMGUI_IMPL_API=__declspec(dllexport)`；**必须用 `target_compile_options` 传 `-D`，不能用 `target_compile_definitions`（CMake 会拆解值中的括号，生成畸形的 `-D`）**
- 版本为 docking 分支（tag `v1.92.9b-docking`）；升级：`git submodule update --remote vendor/imgui`
- 中文界面需在 `ImGuiLayer::OnAttach` 里加载字体：`io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msyh.ttc", 18.0f, nullptr, io.Fonts->GetGlyphRangesChineseSimplifiedCommon())`（默认字体不含中文）
- 已知问题：勾选「使用以前版本的微软拼音输入法」（旧版 IME 兼容模式）时，Ctrl+Space 切换输入法（先放 Ctrl 再放 Space）后 ImGui slider 点击会变成输入框且无法输入。根源：旧版 IME 吞掉 Ctrl 的释放事件，GLFW 一直认为 Ctrl 按下，ImGui 把点击判定为 Ctrl+Click。**引擎无 bug，新版输入法正常**；不加 workaround，建议用户关闭旧版兼容模式。若将来需要兜底：每帧用 `GetAsyncKeyState` 复核修饰键物理状态并**直接覆盖 `io.KeyCtrl` 等字段**（注意 ImGui 1.92 起 `io.AddKeyEvent` 只入队、`NewFrame()` 里才处理，且默认开启的 trickle 规则会把同键连续变化拆到多帧，走事件队列的修正会慢一帧/失效，必须放在 `NewFrame()` 之后直接赋值）

## 输入法（IME）会吞按键（实测 2026-10）

中文输入法开启时，按键先被输入法处理：**数字键最容易丢**（`1/2/3` 被当作候选词选择），WASD 会被组词流程吃掉，
游戏层根本收不到 `KeyPressed`，GLFW 也拿不到 `WM_KEYDOWN`；用 `keybd_event` / `SendInput` / `PostMessage`
注入的自动化按键同样可能被吞。表现是「角色不动 / 快捷键没反应」——排查输入问题时**先看窗口的 IME 状态与焦点，再怀疑 GLFW 回调**。

**引擎的处理（2026-10 起，两层）**

1. **窗口不接受输入法**：`WindowsWindow::Init` 创建窗口后立刻 `ImmAssociateContextEx(hwnd, nullptr, 0)`，
   把输入法上下文从窗口上解绑。这一层能消掉游戏画面上方的**候选窗 / 组词条**，但**挡不住按键被消费**——
   Win10/11 的微软拼音是 TSF 实现，会在消息循环里吃键，窗口照样收不到 `WM_KEYDOWN`
2. **物理按键通道**（真正解决问题的一层，`Engine/Platform/Windows/WindowsKeyboard.*`）：
   - **游戏按键以物理状态为唯一来源**（`Detail::IsPhysicalKey()` 判定，覆盖字母 / 数字 / `F1`-`F12` /
     方向键 / 修饰键 / 常用符号）：`Input::IsKeyPressed` 直接读 `GetAsyncKeyState`，不信 GLFW 的状态
     （GLFW 的状态可能因吞键而残留，混用会「松开了还在走 / 按了没反应」）
   - `WindowsWindow::OnUpdate` 每帧调 `SyncPhysicalKeys()`：按物理状态的**边沿**产生
     `KeyPressedEvent` / `KeyReleasedEvent`；`WindowsWindow` 的 GLFW 按键回调对这批键**一律丢弃**
   - 为什么必须二选一（踩过的坑）：`GetAsyncKeyState` 比消息队列**早 1~2 帧**，两路都发会出现一次按键两个事件，
     表现为「ESC 暂停后菜单一闪而过、P 按了没反应」；而如果只做"补发漏掉的"，GLFW 一旦漏收抬起，
     后续按下会被它判成 REPEAT，游戏里同样失效
   - 代价：被接管的键**没有按住重复事件**（现有三个工程都只用 `IsRepeat()` 过滤重复，不依赖它）；每帧约 90 次
     `GetAsyncKeyState`，几十微秒。键位映射借 `glfwGetKeyScancode` + `MapVirtualKey` 缓存，
     按**物理位置**识别，切中文输入法 / 其它布局都不影响 WASD

验证方法（模拟「按键被吞」）：临时让 `Input::IsKeyPressed` 只走物理通道、并屏蔽 GLFW 的按键回调——
实测 Enter 能开始游戏、按住 D 角色正常移动（坐标 0 → 682）。
双通道回归：ESC 按一次**只**暂停、2.5 秒后菜单仍在，再按一次继续；P 同样一次生效。

**给游戏的提醒：不要用 `ImGui::IsKeyPressed` 做「同一个键既开又关」的兜底。** 引擎接管按键后，
ImGui 仍然会收到系统那份按键（它走自己的 GLFW 后端），在暂停面板打开时（面板是交互窗口，会吃掉
游戏层的事件）就很容易出现「刚开就被自己关掉」。Survivors 的做法可以参考：兜底只用
`IsKeyPressed(key, /*repeat=*/false)`（只在按下那一帧为真），并且开面板后给 0.25 秒的时间宽限
（`m_pauseGuardTime`），避免系统消息比物理状态晚到几帧时被同一个按键关掉。

需要**文本输入**的工程显式调用 `Application::Get().GetWindow().SetImeEnabled(true)`（Win32 实现是
`IACE_DEFAULT` 把默认上下文挂回去），用完再 `false`。注意：在 ImGui `InputText` 已经激活时挂回 IME，
系统会重挂输入法上下文，ImGui 会掉一次焦点（输入框要再点一下），所以应在显示输入框之前打开；
不做「按 `WantTextInput` 每帧自动开关」——实测那样输入框刚激活就会被踢掉。不挂 IME 时 `InputText` 只能输 ASCII。

IME 候选窗定位：`ImGuiLayer::OnAttach` 把 `ImGui::GetMainViewport()->PlatformHandleRaw` 设成原生 HWND
（`Window::GetNativeDisplayHandle()`），ImGui 的 `Platform_SetImeDataFn` 才能把候选窗放到光标处。

这一条取代了早先「不碰 `ImmAssociateContext`」的约定：游戏窗口本来就不该接受输入法（SDL / UE 也是这么做的），
`GetAsyncKeyState` 也只是**读**物理状态、不做任何钩子，输入法本身在其它窗口不受影响。

仍然保留的约定：

- **任何依赖键盘的重要操作都要同时提供鼠标路径**：例 Survivors 的升级卡做成大按钮（鼠标必中），键盘 `1/2/3` 只是额外路径。
  即使 IME 已解绑，也值得保留这条降级路径（远程桌面 / 其它平台 / 用户环境差异）
- 自动化验证（本仓库的测试 / 截图脚本）优先用**真实鼠标点击**：`SetCursorPos` + `mouse_event`——
  点击本身还能把窗口带到前台，顺带解决 `PostMessage` 在非前台窗口不生效的问题；
  必须用键盘时先 `SetForegroundWindow`（窗口前台后 `PostMessage`/`keybd_event` 才稳定生效）
- 中文输入法不会影响 `Input::IsKeyPressed` 对**已经到达 GLFW 的按键**的轮询；问题出在按键根本没送到应用，
  不要为此改游戏输入逻辑（更不要换成 `WantCaptureKeyboard` 屏蔽，见 [平台跳跃](平台跳跃.md)）
