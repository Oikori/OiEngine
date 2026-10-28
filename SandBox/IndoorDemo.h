#pragma once

#include <cstdint>
#include <vector>

#include "OiEngine.h"

// 室内场景相机：围绕房间中心环绕，近 / 远裁剪面与投影矩阵一致（cluster 深度切片依赖这两个值）
class IndoorCamera : public OiEngine::Camera {
public:
    glm::vec3 GetPosition() const override;
    glm::mat4 GetViewMatrix() const override;
    glm::mat4 GetProjectionMatrix() const override;
    float GetNearClip() const override { return 0.1f; }
    float GetFarClip() const override { return 100.0f; }

    glm::vec3 target{0.0f, 1.4f, -1.2f};
    float yaw = glm::radians(90.0f);    // 水平角（默认站在房间 +z 一侧，朝窗户方向看）
    float pitch = glm::radians(16.0f);  // 俯仰角
    float distance = 7.5f;
    float fovDegrees = 62.0f;
};

// 室内多光源演示：程序化房间（含窗户）+ 吊灯 / 壁灯 / 台灯 / 射灯（聚光灯阴影）+ 可加压到 200+ 动态光源；
// 走引擎的新光照路径：光照 SSBO（上限 256）+ Forward+ cluster 剔除 + HDR 离屏 + 全局 tonemap
class IndoorDemoLayer : public OiEngine::Layer {
public:
    IndoorDemoLayer() : Layer("IndoorDemo") {}

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate() override;
    void OnImGuiRender() override;
    void OnEvent(OiEngine::Event& event) override;

    // 场景切换：未激活时跳过更新与渲染
    void SetActive(bool active) { m_active = active; }
    bool IsActive() const { return m_active; }

private:
    // 场景中的一块几何：单位网格 + 变换 + PBR 材质
    struct Surface {
        OiEngine::Ref<OiEngine::VertexArray> mesh;
        glm::vec3 position{0.0f};
        glm::vec3 rotation{0.0f};  // 欧拉角（弧度，依次绕 X / Y / Z）
        glm::vec3 scale{1.0f};
        glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};  // albedo（线性空间）
        glm::vec3 emissive{0.0f};                 // 自发光（线性 HDR，可超过 1）
        float metallic = 0.0f;
        float roughness = 0.6f;
        float ao = 1.0f;
        bool castsShadow = false;
    };

    void AddSurface(const glm::vec3& position, const glm::vec3& scale, const glm::vec4& color,
                    float metallic, float roughness, bool castsShadow,
                    const glm::vec3& emissive = glm::vec3(0.0f), const glm::vec3& rotation = glm::vec3(0.0f));
    void BuildRoom();
    void AddFixtureLights();
    void RebuildFillLights();
    void RecomputeProbe();
    void RenderShadowPasses();
    void SubmitSurface(const Surface& surface);
    void SubmitGizmos();
    glm::mat4 SurfaceTransform(const Surface& surface) const;

private:
    OiEngine::Ref<OiEngine::Shader> m_litShader;
    OiEngine::Ref<OiEngine::Shader> m_depthShader;
    OiEngine::Ref<OiEngine::Shader> m_unlitShader;
    OiEngine::Ref<OiEngine::Shader> m_probeShader;   // 探针捕获用（简化材质）
    OiEngine::Ref<OiEngine::VertexArray> m_boxMesh;
    OiEngine::Ref<OiEngine::VertexArray> m_sphereMesh;
    OiEngine::Ref<OiEngine::VertexArray> m_quadMesh;
    OiEngine::Ref<OiEngine::ShadowMap> m_sunShadowMap;         // 方向光（从窗户进来的阳光）
    OiEngine::Ref<OiEngine::ShadowMapArray> m_spotShadowMaps;  // 聚光灯阴影（每盏一层）
    OiEngine::Ref<OiEngine::EnvironmentProbe> m_probe;         // 环境探针（IBL）

    std::vector<Surface> m_surfaces;         // 房间几何（不含光源 gizmo）
    OiEngine::SceneEnvironment m_scene;      // 环境光 + 固定灯具光源
    std::vector<OiEngine::PointLight> m_fillLights;  // 压力测试用的额外点光源
    IndoorCamera m_camera;

    // UI 调试状态
    bool m_active = false;      // 场景是否激活（由 SceneSwitcher 控制）
    float m_time = 0.0f;
    float m_exposure = 1.0f;
    float m_ambientIntensity = 0.08f;
    float m_sunYaw = glm::radians(280.0f);
    float m_sunPitch = glm::radians(21.0f);
    float m_sunIntensity = 16.0f;
    float m_downlightIntensity = 70.0f;
    float m_downlightOuterAngle = glm::radians(38.0f);
    float m_wallLampIntensity = 45.0f;
    float m_tableLampIntensity = 22.0f;
    float m_fillLightIntensity = 4.0f;
    int m_fillLightCount = 0;         // 额外点光源数量（压力测试）
    bool m_fillLightsAnimated = true; // 额外点光源是否缓慢漂移（制造动态光照）
    bool m_showLightGizmos = false;   // 是否可视化额外点光源的位置
    bool m_useClustering = true;
    bool m_useProbe = true;           // 环境探针（IBL）
    float m_probeIntensity = 0.7f;    // 探针环境光强度
    float m_probeSunScale = 0.45f;    // 捕获时阳光的缩放（避免间接光里重复计算直射阳光）
    bool m_useSpotShadows = true;
    bool m_useSunShadow = true;
    bool m_showShadowMap = false;
    bool m_vsync = true;
    bool m_autoRotate = false;
    bool m_showDemoWindow = false;

    // 每帧统计（UI 显示）
    int m_visibleLightCount = 0;
    OiEngine::LightClustering::Stats m_clusterStats;
    int m_statsFrameCounter = 0;
};
