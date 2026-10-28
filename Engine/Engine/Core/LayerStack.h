#pragma once

#include <vector>

#include "Base.h"
#include "Layer.h"

namespace OiEngine {
    // Layer 栈：普通层从下往上插入，overlay（如 UI 层）始终位于栈顶
    // 事件按栈顶优先（逆序）分发，overlay 能最先拦截输入；渲染按正序进行，overlay 最后绘制
    // 注意：LayerStack 不持有层的所有权，OnAttach/OnDetach 与 delete 由 Application 统一管理
    class OI_ENGINE_API LayerStack {
    public:
        LayerStack() = default;
        ~LayerStack() = default;

        void PushLayer(Layer* layer);
        void PushOverlay(Layer* overlay);

        // Pop 仅将层移出栈，层对象的生命周期由调用方负责
        void PopLayer(Layer* layer);
        void PopOverlay(Layer* overlay);

        // 清空栈（同样不负责层对象生命周期）
        void Clear();

        std::vector<Layer*>::iterator begin() { return m_layers.begin(); }
        std::vector<Layer*>::iterator end() { return m_layers.end(); }
        std::vector<Layer*>::reverse_iterator rbegin() { return m_layers.rbegin(); }
        std::vector<Layer*>::reverse_iterator rend() { return m_layers.rend(); }

        std::vector<Layer*>::const_iterator begin() const { return m_layers.begin(); }
        std::vector<Layer*>::const_iterator end() const { return m_layers.end(); }
        std::vector<Layer*>::const_reverse_iterator rbegin() const { return m_layers.rbegin(); }
        std::vector<Layer*>::const_reverse_iterator rend() const { return m_layers.rend(); }

    private:
        std::vector<Layer*> m_layers;
        size_t m_layerInsertIndex = 0;  // 下一个普通层的插入位置（位于所有 overlay 之前）
    };
}
