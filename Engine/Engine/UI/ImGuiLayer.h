#pragma once

#include "Engine/Core/Layer.h"
#include "Engine/Events/Event.h"

namespace OiEngine {
    // Dear ImGui 封装层：由 Application 内部创建，作为 overlay 置于 LayerStack 顶部
    // - 业务层在 OnImGuiRender() 中直接调用 ImGui:: API 绘制调试界面
    // - 本层最先收到事件，UI 占用鼠标/键盘时按 WantCapture* 屏蔽游戏层输入
    class OI_API ImGuiLayer : public Layer {
    public:
        ImGuiLayer();
        ~ImGuiLayer() override;

        void OnAttach() override;
        void OnDetach() override;
        void OnEvent(Event& event) override;

        // 每帧绘制前后由 Application 调用
        void Begin();
        void End();

        // UI 是否拦截鼠标/键盘输入（默认开启）
        void SetBlockEvents(bool block) { m_blockEvents = block; }

    private:
        bool m_blockEvents = true;
        bool m_initialized = false;
    };
}
