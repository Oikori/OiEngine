#pragma once

#include <functional>

#include "Engine/Events/Event.h"

// Windows 平台的「物理按键」通道。
//
// 为什么需要它：中文输入法（Win10/11 的微软拼音是 TSF 实现）会在消息循环里消费按键，窗口根本收不到
// `WM_KEYDOWN`，GLFW 的按键状态也不会更新——表现是「角色不动 / 快捷键没反应」。
// 把 IME 从窗口上解绑（`ImmAssociateContextEx`）只能消掉候选窗，挡不住这种「按键被吞」。
//
// 做法：用 `GetAsyncKeyState` 读物理按键状态（在消息循环 / 输入法之前那一层），
// **游戏按键（`IsPhysicalKey`）以它为唯一事实来源**：
//   1) `Input::IsKeyPressed` 直接读它（GLFW 的状态可能因为按键被吞而残留 / 卡住，不能混用）；
//   2) `SyncPhysicalKeys` 每帧比较前后状态，转成 KeyPressed / KeyReleased 事件。
// GLFW 的按键回调对这批键**一律丢弃**，否则同一次按键会出现两份事件
// （`GetAsyncKeyState` 比消息队列早 1~2 帧；另外 GLFW 若漏收抬起，后续按下会被它判成 REPEAT）。
// 代价：被接管的键没有「按住重复」事件（引擎目前的各工程都只用来过滤重复，不需要它）。
// 只在 Windows 平台编译（Engine/Platform/Windows/）。
namespace OiEngine::Detail {
    // 每帧调用一次（WindowsWindow::OnUpdate，紧跟 glfwPollEvents 之后）：
    // 采样物理按键，把状态变化转成 KeyPressed / KeyReleased 事件
    void SyncPhysicalKeys(bool windowFocused, const std::function<void(Event&)>& dispatch);

    // 物理按下状态（入参是 GLFW 的 key code）。窗口失焦时由调用方自行短路
    bool IsPhysicallyDown(int glfwKey);

    // 这个键是否由物理通道接管（GLFW 的按键回调要丢弃它的 press / release / repeat）
    bool IsPhysicalKey(int glfwKey);
}
