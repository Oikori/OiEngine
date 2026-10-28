#include "WindowsKeyboard.h"

#include <vector>

#include <windows.h>

#include <GLFW/glfw3.h>

#include "Engine/Events/KeyEvent.h"

namespace OiEngine::Detail {
    namespace {
        constexpr int KEY_LIMIT = 512;

        // GLFW key code -> VK（0 = 还没查过，-1 = 没有对应 VK）
        int s_vkCache[KEY_LIMIT];
        // 上一帧的物理按下状态（按 GLFW key code 索引）
        bool s_physDown[KEY_LIMIT];

        // 由物理通道接管的按键：字母、数字、F1-F12、方向键、修饰键与常用符号。
        // 只覆盖引擎与各工程实际会用到的键，避免每帧对几百个 VK 做无用的系统调用
        const std::vector<int>& GameKeys()
        {
            static const std::vector<int> keys = [] {
                std::vector<int> list;
                for (int key = GLFW_KEY_A; key <= GLFW_KEY_Z; ++key) {
                    list.push_back(key);
                }
                for (int key = GLFW_KEY_0; key <= GLFW_KEY_9; ++key) {
                    list.push_back(key);
                }
                for (int key = GLFW_KEY_F1; key <= GLFW_KEY_F12; ++key) {
                    list.push_back(key);
                }
                const int extra[] = {
                    GLFW_KEY_SPACE, GLFW_KEY_ENTER, GLFW_KEY_ESCAPE, GLFW_KEY_TAB, GLFW_KEY_BACKSPACE,
                    GLFW_KEY_LEFT, GLFW_KEY_RIGHT, GLFW_KEY_UP, GLFW_KEY_DOWN,
                    GLFW_KEY_LEFT_SHIFT, GLFW_KEY_RIGHT_SHIFT, GLFW_KEY_LEFT_CONTROL,
                    GLFW_KEY_RIGHT_CONTROL, GLFW_KEY_LEFT_ALT, GLFW_KEY_RIGHT_ALT,
                    GLFW_KEY_COMMA, GLFW_KEY_PERIOD, GLFW_KEY_SLASH, GLFW_KEY_SEMICOLON,
                    GLFW_KEY_APOSTROPHE, GLFW_KEY_LEFT_BRACKET, GLFW_KEY_RIGHT_BRACKET,
                    GLFW_KEY_BACKSLASH, GLFW_KEY_GRAVE_ACCENT, GLFW_KEY_MINUS, GLFW_KEY_EQUAL,
                    GLFW_KEY_INSERT, GLFW_KEY_DELETE, GLFW_KEY_HOME, GLFW_KEY_END,
                    GLFW_KEY_PAGE_UP, GLFW_KEY_PAGE_DOWN,
                };
                for (const int key : extra) {
                    list.push_back(key);
                }
                return list;
            }();
            return keys;
        }

        // GLFW key code -> Win32 VK：借 GLFW 的扫描码再转 VK，结果缓存（含左右 Shift/Ctrl/Alt 的区分）
        int VkForGlfwKey(int glfwKey)
        {
            if (glfwKey < 0 || glfwKey >= KEY_LIMIT) {
                return -1;
            }
            if (s_vkCache[glfwKey] != 0) {
                return s_vkCache[glfwKey];
            }
            int vk = -1;
            const int scancode = glfwGetKeyScancode(glfwKey);
            if (scancode > 0) {
                const UINT mapped = MapVirtualKeyW(static_cast<UINT>(scancode), MAPVK_VSC_TO_VK_EX);
                if (mapped != 0) {
                    vk = static_cast<int>(mapped);
                }
            }
            s_vkCache[glfwKey] = vk;
            return vk;
        }

        bool SamplePhysicallyDown(int glfwKey)
        {
            const int vk = VkForGlfwKey(glfwKey);
            if (vk <= 0) {
                return false;
            }
            // 高位置位 = 当前按下；低位的「自上次调用以来按过」不用，保持纯状态语义
            return (GetAsyncKeyState(vk) & 0x8000) != 0;
        }
    }

    bool IsPhysicallyDown(int glfwKey)
    {
        return SamplePhysicallyDown(glfwKey);
    }

    bool IsPhysicalKey(int glfwKey)
    {
        if (glfwKey < 0 || glfwKey >= KEY_LIMIT) {
            return false;
        }
        // 每个键只查一次（GameKeys 固定不变）
        static bool managed[KEY_LIMIT] = {};
        static bool initialized = false;
        if (!initialized) {
            for (const int key : GameKeys()) {
                managed[key] = true;
            }
            initialized = true;
        }
        return managed[glfwKey];
    }

    void SyncPhysicalKeys(bool windowFocused, const std::function<void(Event&)>& dispatch)
    {
        for (const int key : GameKeys()) {
            const bool down = windowFocused && SamplePhysicallyDown(key);
            if (down == s_physDown[key]) {
                continue;   // 没有变化（按住 / 一直没按）
            }
            s_physDown[key] = down;
            if (!dispatch) {
                continue;
            }
            if (down) {
                KeyPressedEvent event(key, false);
                dispatch(event);
            } else {
                KeyReleasedEvent event(key);
                dispatch(event);
            }
        }
    }
}
