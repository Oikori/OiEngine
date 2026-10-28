#pragma once

#include "Base.h"
#include "Engine/Events/Event.h"

namespace OiEngine {
	class OI_API Layer {
	public:
        explicit Layer(const std::string& name = "Layer");
		virtual ~Layer() = default;

		virtual void OnAttach() {}
		virtual void OnDetach() {}
		virtual void OnUpdate() {}
		virtual void OnFixedUpdate() {}
		virtual void OnImGuiRender() {}
		virtual void OnEvent(Event& event) {}

		const std::string& GetName() const { return m_debugName; }
	protected:
		std::string m_debugName;
	};
}
