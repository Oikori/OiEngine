#pragma once

#include <cstdint>
#include <vector>

#include "OiEngine.h"

// 太阳系相机：从斜上方俯视原点，透视投影
class SolarSystemCamera : public OiEngine::Camera {
public:
    OiEngine::Math::Vector3 GetPosition() const override;
    OiEngine::Math::Matrix4 GetViewMatrix() const override;
    OiEngine::Math::Matrix4 GetProjectionMatrix() const override;
};

class SolarSystemLayer : public OiEngine::Layer {
public:
    SolarSystemLayer() : Layer("SolarSystem") {}

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate() override;
    void OnImGuiRender() override;
    void OnEvent(OiEngine::Event& event) override;

private:
    struct CelestialBody {
        float orbitRadius = 0.0f;
        float angularSpeed = 0.0f;
        float startAngle = 0.0f;
        OiEngine::Math::Vector3 position;
        OiEngine::Ref<OiEngine::VertexArray> mesh;
        OiEngine::Ref<OiEngine::VertexArray> ringMesh;  // 土星环（未索引的天体为空）
    };

    // 轨道线、环等几何不参与光照，需要手动设置相机的 uniform
    void UseUnlitShader(const OiEngine::Math::Matrix4& transform);

private:
    OiEngine::Ref<OiEngine::Shader> m_litShader;
    OiEngine::Ref<OiEngine::Shader> m_unlitShader;
    OiEngine::Ref<OiEngine::Shader> m_glowShader;
    OiEngine::Ref<OiEngine::VertexArray> m_sunMesh;
    OiEngine::Ref<OiEngine::VertexArray> m_glowMesh;
    OiEngine::Ref<OiEngine::VertexArray> m_orbitMesh;
    uint32_t m_orbitVertexCount = 0;
    std::vector<CelestialBody> m_planets;
    SolarSystemCamera m_camera;

    // UI 调试状态
    float m_timeScale = 1.0f;
    bool m_paused = false;
    float m_simulationTime = 0.0f;
    bool m_showDemoWindow = false;
};
