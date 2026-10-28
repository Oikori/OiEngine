#pragma once

#include <chrono>

#include "Base.h"

namespace OiEngine {
	class OI_ENGINE_API Timer {
	public:
        static void Init();
	    static void Update();
	    static float GetTimeSinceStart();
	    static float GetDeltaTime();

	    static float s_fixedDeltaTime;
	private:
		static std::chrono::time_point<std::chrono::high_resolution_clock> m_start;
		static std::chrono::time_point<std::chrono::high_resolution_clock> m_preFrame;
	    static std::chrono::microseconds m_deltaTime;
	};
}
