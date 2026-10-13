#pragma once

#include <cstdint>
#include <vector>

#include "OiEngine.h"

// 太阳系相机：从斜上方俯视原点，透视投影
class SolarSystemCamera : public OiEngine::Camera {
public:
    glm::vec3 GetPosition() const override;
    glm::mat4 GetViewMatrix() const override;
    glm::mat4 GetProjectionMatrix() const override;
};

class SolarSystemLayer : public OiEngine::Layer {
public:
    SolarSystemLayer() : Layer("SolarSystem") {}

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate() override;
    void OnImGuiRender() override;
    void OnEvent(OiEngine::Event& event) override;

    // 场景切换：未激活时跳过更新与渲染
    void SetActive(bool active) { m_active = active; }
    bool IsActive() const { return m_active; }

private:
    struct CelestialBody {
        float orbitRadius = 0.0f;
        float angularSpeed = 0.0f;
        float startAngle = 0.0f;
        glm::vec3 position;
        OiEngine::Ref<OiEngine::VertexArray> mesh;
        OiEngine::Ref<OiEngine::VertexArray> ringMesh;  // 土星环（未索引的天体为空）
    };

    // 轨道线、环等几何不参与光照，需要手动设置相机的 uniform
    void UseUnlitShader(const glm::mat4& transform);

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
    OiEngine::LightingEnvironment m_lighting;  // 环境光 + 位于太阳位置的点光源

    // UI 调试状态
    bool m_active = true;  // 场景是否激活（由 SceneSwitcher 控制）
    float m_timeScale = 1.0f;
    bool m_paused = false;
    float m_simulationTime = 0.0f;
    float m_ambientIntensity = 0.06f;
    float m_sunIntensity = 1.2f;
    float m_sunRange = 45.0f;
    bool m_showDemoWindow = false;
};
