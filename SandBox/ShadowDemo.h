#pragma once

#include <vector>

#include "OiEngine.h"

// 阴影演示相机：围绕 target 环绕，yaw / pitch / distance 可在 UI 中实时调整
class ShadowDemoCamera : public OiEngine::Camera {
public:
    glm::vec3 GetPosition() const override;
    glm::mat4 GetViewMatrix() const override;
    glm::mat4 GetProjectionMatrix() const override;

    glm::vec3 target{0.0f, 1.5f, 0.0f};
    float yaw = glm::radians(215.0f);   // 水平角（默认位于太阳对面，顺光观察）
    float pitch = glm::radians(24.0f);  // 俯仰角
    float distance = 17.0f;
};

// 方向光阴影演示：地面 + 一组投射阴影的几何体，太阳方向 / 阴影偏移 / 相机均可调
class ShadowDemoLayer : public OiEngine::Layer {
public:
    ShadowDemoLayer() : Layer("ShadowDemo") {}

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate() override;
    void OnImGuiRender() override;
    void OnEvent(OiEngine::Event& event) override;

    // 场景切换：未激活时跳过更新与渲染
    void SetActive(bool active) { m_active = active; }
    bool IsActive() const { return m_active; }

private:
    // 参与投影的几何体：单位网格 + 每帧变换
    struct Instance {
        OiEngine::Ref<OiEngine::VertexArray> mesh;
        glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
        glm::vec3 position{0.0f};
        glm::vec3 scale{1.0f};
        glm::vec3 basePosition{0.0f};  // 上下浮动（bob）的基准位置
        float spinSpeed = 0.0f;        // 绕 Y 轴自转速度（rad/s）
        float angle = 0.0f;
        float bobAmplitude = 0.0f;     // 悬浮浮动幅度
        float bobPhase = 0.0f;
    };

    glm::mat4 InstanceTransform(const Instance& instance) const;
    glm::vec3 SunDirection() const;  // 由 UI 的方位角 / 仰角得到指向太阳的方向
    void SubmitInstance(const Instance& instance);

private:
    OiEngine::Ref<OiEngine::Shader> m_litShader;
    OiEngine::Ref<OiEngine::Shader> m_depthShader;
    OiEngine::Ref<OiEngine::Shader> m_sunShader;
    OiEngine::Ref<OiEngine::VertexArray> m_groundMesh;
    OiEngine::Ref<OiEngine::VertexArray> m_boxMesh;
    OiEngine::Ref<OiEngine::VertexArray> m_sphereMesh;
    std::vector<Instance> m_instances;
    OiEngine::Ref<OiEngine::ShadowMap> m_shadowMap;
    ShadowDemoCamera m_camera;
    OiEngine::SceneEnvironment m_scene;  // 环境光 + 太阳方向光

    // UI 调试状态
    bool m_active = false;  // 场景是否激活（由 SceneSwitcher 控制）
    float m_time = 0.0f;
    float m_sunYaw = glm::radians(35.0f);    // 太阳方位角（弧度，SliderAngle 直接编辑）
    float m_sunPitch = glm::radians(34.0f);  // 太阳仰角
    float m_sunIntensity = 1.35f;
    float m_ambientIntensity = 0.16f;
    float m_shadowBias = 0.0025f;
    float m_shadowRadius = 13.0f;  // 阴影正交视锥覆盖的包围球半径
    bool m_autoRotate = true;
    bool m_showShadowMap = false;
    bool m_showDemoWindow = false;
};
