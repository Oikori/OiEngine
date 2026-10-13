#include "Timer.h"

namespace OiEngine {
    float Timer::s_fixedDeltaTime = 1.0f / 60.0f;
    std::chrono::time_point<std::chrono::high_resolution_clock> Timer::m_start;
    std::chrono::time_point<std::chrono::high_resolution_clock> Timer::m_preFrame;
    std::chrono::microseconds Timer::m_deltaTime;

    void Timer::Init()
    {
        m_preFrame = m_start = std::chrono::high_resolution_clock::now();
    }

    void Timer::Update()
    {
        auto now = std::chrono::high_resolution_clock::now();
        m_deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(now - m_preFrame);
        m_preFrame = now;
    }

    float Timer::GetTimeSinceStart()
    {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(now - m_start).count() * 0.001f * 0.001f;
    }

    float Timer::GetDeltaTime()
    {
        return m_deltaTime.count() * 0.001f * 0.001f;
    }
}
