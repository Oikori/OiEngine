# ImGui 与输入

> 返回 [AGENTS.md](../AGENTS.md)。相关：[平台跳跃](平台跳跃.md)、[幸存者](幸存者.md)。

## Dear ImGui 集成

- 引擎内置 `ImGuiLayer`（overlay，`Application::Init` 自动创建）：业务层实现 `Layer::OnImGuiRender()` 后直接用 `ImGui::` API 绘制；渲染时机在场景之后、SwapBuffers 之前，最小化时跳过
- 事件：overlay 逆序最先收到事件，UI 占用鼠标/键盘（`WantCaptureMouse/Keyboard`）时标记 `handled`，不再传入游戏层（有 1 帧延迟，正常）
- SandBox 直接 `#include <imgui.h>` 即可：include 路径与 `IMGUI_API=dllimport` 经 Engine target 导出
- dll 边界：imgui 源码以 OBJECT 库并入 `Engine.dll`，编译时 `IMGUI_API/IMGUI_IMPL_API=__declspec(dllexport)`；**必须用 `target_compile_options` 传 `-D`，不能用 `target_compile_definitions`（CMake 会拆解值中的括号，生成畸形的 `-D`）**
- 版本为 docking 分支（tag `v1.92.9b-docking`）；升级：`git submodule update --remote vendor/imgui`
- 中文界面需在 `ImGuiLayer::OnAttach` 里加载字体：`io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msyh.ttc", 18.0f, nullptr, io.Fonts->GetGlyphRangesChineseSimplifiedCommon())`（默认字体不含中文）
- 已知问题：勾选「使用以前版本的微软拼音输入法」（旧版 IME 兼容模式）时，Ctrl+Space 切换输入法（先放 Ctrl 再放 Space）后 ImGui slider 点击会变成输入框且无法输入。根源：旧版 IME 吞掉 Ctrl 的释放事件，GLFW 一直认为 Ctrl 按下，ImGui 把点击判定为 Ctrl+Click。**引擎无 bug，新版输入法正常**；不加 workaround，建议用户关闭旧版兼容模式。若将来需要兜底：每帧用 `GetAsyncKeyState` 复核修饰键物理状态并**直接覆盖 `io.KeyCtrl` 等字段**（注意 ImGui 1.92 起 `io.AddKeyEvent` 只入队、`NewFrame()` 里才处理，且默认开启的 trickle 规则会把同键连续变化拆到多帧，走事件队列的修正会慢一帧/失效，必须放在 `NewFrame()` 之后直接赋值）

## 输入法（IME）会吞按键（实测 2026-10）

中文输入法开启时，按键先被输入法处理：**数字键最容易丢**（`1/2/3` 被当作候选词选择），游戏层收不到 `KeyPressed`；
用 `keybd_event` / `SendInput` / `PostMessage` 注入的自动化按键也可能同样被吞。表现是「脚本按了键，但角色/界面完全没反应」——
排查输入问题时**先确认输入法与窗口焦点，再怀疑 GLFW 回调**。

约定：

- **任何依赖键盘的重要操作都要同时提供鼠标路径**：例 Survivors 的升级卡做成大按钮（鼠标必中），键盘 `1/2/3` 只是额外路径。
  只给数字键/单键快捷键的流程在中文输入法下可能直接卡死，无法继续
- 不写 `SetWindowsHookEx` / `ImmAssociateContext` 之类的抢键 hack：输入法是用户环境，游戏侧只做兼容与降级
- 自动化验证（本仓库的测试 / 截图脚本）优先用**真实鼠标点击**：`SetCursorPos` + `mouse_event`——
  点击本身还能把窗口带到前台，顺带解决 `PostMessage` 在非前台窗口不生效的问题；
  必须用键盘时先 `SetForegroundWindow` 并切到英文输入法，或干脆改用鼠标
- 中文输入法不会影响 `Input::IsKeyPressed` 对**已经到达 GLFW 的按键**的轮询；问题出在按键根本没送到应用，
  不要为此改游戏输入逻辑（更不要换成 `WantCaptureKeyboard` 屏蔽，见 [平台跳跃](平台跳跃.md)）
