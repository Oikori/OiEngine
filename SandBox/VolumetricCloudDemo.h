#pragma once

#include <cstdint>

#include "OiEngine.h"

// 体积云演示相机：自由飞行（yaw / pitch / fov），世界单位 = 米
class VolumetricCloudCamera : public OiEngine::Camera {
public:
    glm::vec3 GetPosition() const override { return position; }
    glm::mat4 GetViewMatrix() const override;
    glm::mat4 GetProjectionMatrix() const override;
    float GetNearClip() const override { return nearClip; }
    float GetFarClip() const override { return farClip; }

    glm::vec3 Forward() const;
    glm::vec3 Right() const;
    glm::vec3 Up() const;

    glm::vec3 position{0.0f, 4800.0f, 13000.0f};
    float yaw = 0.0f;                       // 0 = 朝 -Z；正值向右转
    float pitch = glm::radians(-10.0f);     // 负值俯视
    float fov = glm::radians(58.0f);
    float nearClip = 10.0f;
    float farClip = 120000.0f;
};

// 仿《地平线》体积云场景：
//   1) 程序化雪山地形 -> RGBA16F + 深度离屏目标（真实几何，提供视差与遮挡）
//   2) 半分辨率光线步进体积云（天气图 + 3D Perlin-Worley 噪声 + 单次散射）
//   3) 全分辨率合成：天空 / 太阳 / 地形 / 云层 + ACES tonemap
class VolumetricCloudLayer : public OiEngine::Layer {
public:
    VolumetricCloudLayer() : Layer("VolumetricCloud") {}

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate() override;
    void OnImGuiRender() override;
    void OnEvent(OiEngine::Event& event) override;

    void SetActive(bool active) { m_active = active; }
    bool IsActive() const { return m_active; }

private:
    struct CloudSettings {
        float coverage = 0.58f;       // 云量（0 晴 ~ 1 阴）
        float density = 1.00f;        // 密度倍率
        float cloudBase = 1200.0f;    // 云底高度（米）
        float cloudTop = 4600.0f;     // 云顶高度（米，局部塔顶会低于这个 slab 上限）
        float extinction = 0.0060f;   // 消光系数（每米 / 单位密度）
        float detailStrength = 0.35f; // 高频细节侵蚀强度（0 = 完全平滑，1 = 最强）
        float ambient = 0.24f;        // 云的环境光强度
        float denoise = 0.60f;        // 云层空间降噪强度（深度感知 5x5 高斯）
        float windSpeed = 14.0f;      // 风速（米/秒），驱动天气图与细节噪声
        int steps = 144;              // 主光线步进次数
        int lightSteps = 5;           // 朝太阳的步进次数
        float renderScale = 1.0f;     // 体积云离屏分辨率比例（0.4 ~ 1.0）
    };

    void EnsureInitialized();
    void InitializeResources();
    void BuildNoiseTextures();
    void BuildTerrain();
    void CreateFullscreenQuad();
    void EnsureRenderTargets();
    void ResizeRenderTargets(uint32_t width, uint32_t height);

    void UpdateCamera(float deltaTime);

    void RenderTerrain();
    void RenderClouds();
    void RenderComposite();

    void ApplyPreset(int preset);
    void ResetCamera();

    glm::vec3 SunDirection() const;
    glm::vec2 WeatherOffset() const;

private:
    bool m_active = false;
    bool m_initialized = false;
    float m_time = 0.0f;
    uint32_t m_frameIndex = 0;

    // 渲染资源
    OiEngine::Ref<OiEngine::Shader> m_terrainShader;
    OiEngine::Ref<OiEngine::Shader> m_cloudShader;
    OiEngine::Ref<OiEngine::Shader> m_compositeShader;
    OiEngine::Ref<OiEngine::VertexArray> m_terrainMesh;
    OiEngine::Ref<OiEngine::VertexArray> m_fullscreenQuad;
    OiEngine::Ref<OiEngine::Texture2D> m_weatherMap;
    OiEngine::Ref<OiEngine::Texture3D> m_shapeNoise;

    // 离屏目标：场景（HDR 颜色 + 深度）与半分辨率云层
    OiEngine::Ref<OiEngine::Framebuffer> m_sceneFbo;
    OiEngine::Ref<OiEngine::Framebuffer> m_cloudFbo;
    uint32_t m_sceneWidth = 0;
    uint32_t m_sceneHeight = 0;
    uint32_t m_cloudWidth = 0;
    uint32_t m_cloudHeight = 0;

    VolumetricCloudCamera m_camera;
    CloudSettings m_cloud;
    glm::vec2 m_windOffset{0.0f, 0.0f};

    // 光照 / 大气
    float m_sunAzimuth = glm::radians(-22.0f);   // 相对 -Z 的方位角
    float m_sunElevation = glm::radians(6.0f);   // 太阳高度角
    float m_sunIntensity = 5.0f;
    float m_exposure = 1.05f;
    glm::vec3 m_sunColor{1.00f, 0.72f, 0.40f};
    glm::vec3 m_skyZenith{0.025f, 0.065f, 0.180f};
    glm::vec3 m_skyHorizon{0.92f, 0.34f, 0.09f};
    glm::vec3 m_hazeColor{0.40f, 0.36f, 0.36f};
    glm::vec3 m_skyAmbient{0.20f, 0.26f, 0.38f};
    float m_fogDensity = 0.000040f;

    // 相机操作
    float m_moveSpeed = 700.0f;
    bool m_dragging = false;
    glm::vec2 m_lastMouse{0.0f, 0.0f};
    bool m_autoCruise = false;
    float m_cruiseSpeed = 80.0f;

    // UI
    int m_debugView = 0;  // 0 最终 / 1 云透过率 / 2 场景 / 3 深度
    bool m_showHelp = true;
    bool m_showPanel = true;
    bool m_panelConfigured = false;
};
