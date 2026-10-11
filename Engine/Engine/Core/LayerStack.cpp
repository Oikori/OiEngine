#include "LayerStack.h"

#include <algorithm>

namespace OiEngine {
    void LayerStack::PushLayer(Layer* layer)
    {
        m_layers.emplace(m_layers.begin() + static_cast<std::ptrdiff_t>(m_layerInsertIndex), layer);
        ++m_layerInsertIndex;
    }

    void LayerStack::PushOverlay(Layer* overlay)
    {
        m_layers.emplace_back(overlay);
    }

    void LayerStack::PopLayer(Layer* layer)
    {
        const auto it = std::find(m_layers.begin(), m_layers.end(), layer);
        if (it != m_layers.end()) {
            const size_t index = static_cast<size_t>(std::distance(m_layers.begin(), it));
            m_layers.erase(it);
            if (index < m_layerInsertIndex) {
                --m_layerInsertIndex;
            }
        }
    }

    void LayerStack::PopOverlay(Layer* overlay)
    {
        const auto it = std::find(m_layers.begin(), m_layers.end(), overlay);
        if (it != m_layers.end()) {
            m_layers.erase(it);
        }
    }
}
