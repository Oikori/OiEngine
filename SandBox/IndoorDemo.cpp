#include "IndoorDemo.h"

#include <cmath>
#include <cstdint>
#include <imgui.h>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

#include "Engine/Core/Application.h"

using namespace OiEngine;

namespace {
    constexpr float PI = 3.14159265358979323846f;
    constexpr uint32_t SPHERE_SEGMENTS = 32;  // 球体经度分段
    constexpr uint32_t SPHERE_RINGS = 16;     // 球体纬度分段

    // 纹理单元约定：0 = 聚光灯阴影数组；1 = 方向光阴影贴图；2/3 = 环境探针（irradiance / prefilter）
    constexpr uint32_t SPOT_SHADOW_TEXTURE_UNIT = 0;
    constexpr uint32_t SUN_SHADOW_TEXTURE_UNIT = 1;
    constexpr uint32_t PROBE_IRRADIANCE_TEXTURE_UNIT = 2;
    constexpr uint32_t PROBE_PREFILTER_TEXTURE_UNIT = 3;

    // 方向光阴影覆盖的包围球（房间 + 家具）
    const glm::vec3 SHADOW_FOCUS(0.0f, 2.0f, 0.0f);
    constexpr float SHADOW_RADIUS = 11.0f;

    // 吊灯（聚光灯，投射阴影）
    const glm::vec3 DOWNLIGHT_POSITIONS[4] = {
        { -4.0f, 3.86f, -3.0f }, { 4.0f, 3.86f, -3.0f },
        { -4.0f, 3.86f,  3.0f }, { 4.0f, 3.86f,  3.0f },
    };
    // 壁灯（点光源）
    const glm::vec3 WALL_LAMP_POSITIONS[4] = {
        { -7.7f, 2.5f, -3.0f }, { -7.7f, 2.5f, 3.0f },
        {  7.7f, 2.5f, -3.0f }, {  7.7f, 2.5f, 3.0f },
    };
    const glm::vec3 WALL_LAMP_COLORS[4] = {
        { 1.0f, 0.72f, 0.42f }, { 1.0f, 0.72f, 0.42f },
        { 1.0f, 0.72f, 0.42f }, { 1.0f, 0.72f, 0.42f },
    };
    // 台灯（点光源）
    const glm::vec3 TABLE_LAMP_POSITIONS[2] = {
        { -1.25f, 1.06f, 0.5f }, { 1.25f, 1.06f, 0.5f },
    };

    // ---------- 网格生成（顶点格式：位置 + 法线 + 颜色，共 10 个 float） ----------

    void PushVertex(std::vector<float>& vertices, const glm::vec3& position, const glm::vec3& normal, const glm::vec4& color)
    {
        vertices.push_back(position.x);
        vertices.push_back(position.y);
        vertices.push_back(position.z);
        vertices.push_back(normal.x);
        vertices.push_back(normal.y);
        vertices.push_back(normal.z);
        vertices.push_back(color.x);
        vertices.push_back(color.y);
        vertices.push_back(color.z);
        vertices.push_back(color.w);
    }

    // 轴对齐单位立方体（边长 1，-0.5 ~ 0.5），每面独立法线
    void AppendBox(std::vector<float>& vertices, std::vector<uint32_t>& indices)
    {
        constexpr float h = 0.5f;
        const glm::vec3 positions[8] = {
            { -h, -h, -h }, {  h, -h, -h }, {  h,  h, -h }, { -h,  h, -h },
            { -h, -h,  h }, {  h, -h,  h }, {  h,  h,  h }, { -h,  h,  h },
        };
        const glm::vec3 normals[6] = {
            {  0.0f,  0.0f,  1.0f }, {  0.0f,  0.0f, -1.0f },
            { -1.0f,  0.0f,  0.0f }, {  1.0f,  0.0f,  0.0f },
            {  0.0f, -1.0f,  0.0f }, {  0.0f,  1.0f,  0.0f },
        };
        const uint32_t faces[6][4] = {
            { 4, 5, 6, 7 }, { 0, 3, 2, 1 }, { 0, 4, 7, 3 },
            { 1, 2, 6, 5 }, { 0, 1, 5, 4 }, { 3, 7, 6, 2 },
        };

        const glm::vec4 white(1.0f, 1.0f, 1.0f, 1.0f);
        for (uint32_t face = 0; face < 6; ++face) {
            const uint32_t base = static_cast<uint32_t>(vertices.size() / 10);
            for (uint32_t corner = 0; corner < 4; ++corner) {
                PushVertex(vertices, positions[faces[face][corner]], normals[face], white);
            }
            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 0);
            indices.push_back(base + 2);
            indices.push_back(base + 3);
        }
    }

    // 单位球（半径 0.5，球心在局部原点）；球体法线即位置方向
    void AppendSphere(std::vector<float>& vertices, std::vector<uint32_t>& indices)
    {
        const glm::vec4 white(1.0f, 1.0f, 1.0f, 1.0f);

        for (uint32_t ring = 0; ring <= SPHERE_RINGS; ++ring) {
            const float phi = PI * static_cast<float>(ring) / SPHERE_RINGS;
            for (uint32_t segment = 0; segment <= SPHERE_SEGMENTS; ++segment) {
                const float theta = 2.0f * PI * static_cast<float>(segment) / SPHERE_SEGMENTS;
                const glm::vec3 position(0.5f * std::sin(phi) * std::cos(theta),
                                         0.5f * std::cos(phi),
                                         0.5f * std::sin(phi) * std::sin(theta));
                PushVertex(vertices, position, glm::normalize(position), white);
            }
        }

        for (uint32_t ring = 0; ring < SPHERE_RINGS; ++ring) {
            for (uint32_t segment = 0; segment < SPHERE_SEGMENTS; ++segment) {
                const uint32_t topLeft = ring * (SPHERE_SEGMENTS + 1) + segment;
                const uint32_t bottomLeft = topLeft + SPHERE_SEGMENTS + 1;
                indices.push_back(topLeft);
                indices.push_back(bottomLeft);
                indices.push_back(topLeft + 1);
                indices.push_back(topLeft + 1);
                indices.push_back(bottomLeft);
                indices.push_back(bottomLeft + 1);
            }
        }
    }

    // 单位方形平面（XZ 平面，1 x 1，法线朝 +Y）
    void AppendQuad(std::vector<float>& vertices, std::vector<uint32_t>& indices)
    {
        const glm::vec3 normal(0.0f, 1.0f, 0.0f);
        const glm::vec4 white(1.0f, 1.0f, 1.0f, 1.0f);
        PushVertex(vertices, glm::vec3(-0.5f, 0.0f, -0.5f), normal, white);
        PushVertex(vertices, glm::vec3(-0.5f, 0.0f,  0.5f), normal, white);
        PushVertex(vertices, glm::vec3( 0.5f, 0.0f,  0.5f), normal, white);
        PushVertex(vertices, glm::vec3( 0.5f, 0.0f, -0.5f), normal, white);
        indices = { 0, 1, 2, 0, 2, 3 };
    }

    Ref<VertexArray> CreateMesh(std::vector<float>& vertices, std::vector<uint32_t>& indices)
    {
        Ref<VertexArray> vertexArray = VertexArray::Create();

        Ref<VertexBuffer> vertexBuffer = VertexBuffer::Create(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(float)));
        vertexBuffer->SetLayout({
            { ShaderDataType::Float3, "a_Position" },
            { ShaderDataType::Float3, "a_Normal" },
            { ShaderDataType::Float4, "a_Color" },
        });
        vertexArray->AddVertexBuffer(vertexBuffer);

        Ref<IndexBuffer> indexBuffer = IndexBuffer::Create(indices.data(), static_cast<uint32_t>(indices.size()));
        vertexArray->SetIndexBuffer(indexBuffer);
        return vertexArray;
    }
}

// ---------- IndoorCamera ----------

glm::vec3 IndoorCamera::GetPosition() const
{
    const float cosPitch = std::cos(pitch);
    const glm::vec3 orbit = target + distance * glm::vec3(cosPitch * std::cos(yaw), std::sin(pitch), cosPitch * std::sin(yaw));

    // 把机位夹在房间内（轨道相机很容易转到墙外 / 天花板外，室内观察必须限制在房间内部）
    return glm::clamp(orbit, glm::vec3(-7.5f, 0.4f, -5.5f), glm::vec3(7.5f, 3.7f, 5.5f));
}

glm::mat4 IndoorCamera::GetViewMatrix() const
{
    return glm::lookAt(GetPosition(), target, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 IndoorCamera::GetProjectionMatrix() const
{
    const uint32_t width = Application::Get().GetWindow().GetWidth();
    const uint32_t height = Application::Get().GetWindow().GetHeight();
    const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 16.0f / 9.0f;
    return glm::perspective(glm::radians(fovDegrees), aspect, GetNearClip(), GetFarClip());
}

// ---------- IndoorDemoLayer ----------

void IndoorDemoLayer::OnAttach()
{
    m_litShader = Shader::Create("assets/shader/scenes/indoor_demo_lit.glsl");
    m_depthShader = Shader::Create("assets/shader/scenes/indoor_demo_depth.glsl");
    m_unlitShader = Shader::Create("assets/shader/scenes/indoor_demo_unlit.glsl");
    m_probeShader = Shader::Create("assets/shader/scenes/indoor_demo_probe.glsl");

    m_sunShadowMap = CreateRef<ShadowMap>(4096);
    m_sunShadowMap->SetBias(0.0028f);
    m_spotShadowMaps = ShadowMapArray::Create(1024, MaxShadowCasters);
    m_spotShadowMaps->SetBias(0.0025f);
    m_probe = EnvironmentProbe::Create(256, 32, 128);

    {
        std::vector<float> vertices;
        std::vector<uint32_t> indices;
        AppendBox(vertices, indices);
        m_boxMesh = CreateMesh(vertices, indices);
    }
    {
        std::vector<float> vertices;
        std::vector<uint32_t> indices;
        AppendSphere(vertices, indices);
        m_sphereMesh = CreateMesh(vertices, indices);
    }
    {
        std::vector<float> vertices;
        std::vector<uint32_t> indices;
        AppendQuad(vertices, indices);
        m_quadMesh = CreateMesh(vertices, indices);
    }

    BuildRoom();

    // 光照：微弱冷色环境光（室内间接光的粗略近似）+ 阳光方向光
    m_scene.ambientColor = glm::vec3(0.42f, 0.48f, 0.62f);
    m_scene.ambientIntensity = m_ambientIntensity;
    m_scene.directionalLights.push_back(DirectionalLight{});
    AddFixtureLights();
    // 初始的额外点光源数量（UI 滑动条变化时也会重建）
    RebuildFillLights();

    // 环境探针：把房间（含阳光与自发光灯具）捕获成 cubemap，供间接光使用
    RecomputeProbe();

    // 纹理单元约定只需设置一次（两个采样器都固定绑定）
    m_litShader->Bind();
    m_litShader->SetInt("u_SunShadowMap", SUN_SHADOW_TEXTURE_UNIT);
    m_litShader->SetInt("u_SpotShadowMaps", SPOT_SHADOW_TEXTURE_UNIT);
    m_litShader->SetInt("u_IrradianceMap", PROBE_IRRADIANCE_TEXTURE_UNIT);
    m_litShader->SetInt("u_PrefilteredMap", PROBE_PREFILTER_TEXTURE_UNIT);
    m_litShader->SetFloat("u_SpotShadowTexel", 1.0f / static_cast<float>(m_spotShadowMaps->GetSize()));
    m_litShader->SetFloat("u_ProbeMaxLod", static_cast<float>(m_probe->GetPrefilterLevelCount() - 1));

    RenderCommand::SetClearColor(glm::vec4(0.02f, 0.022f, 0.03f, 1.0f));

    OI_INFO("IndoorDemo OnAttach: {0} surfaces, {1} shadow maps",
        m_surfaces.size(), m_spotShadowMaps->GetLayerCount());
}

void IndoorDemoLayer::OnDetach() {}

void IndoorDemoLayer::AddSurface(const glm::vec3& position, const glm::vec3& scale, const glm::vec4& color,
                                 float metallic, float roughness, bool castsShadow,
                                 const glm::vec3& emissive, const glm::vec3& rotation)
{
    Surface surface;
    surface.mesh = m_boxMesh;
    surface.position = position;
    surface.rotation = rotation;
    surface.scale = scale;
    surface.color = color;
    surface.emissive = emissive;
    surface.metallic = metallic;
    surface.roughness = roughness;
    surface.castsShadow = castsShadow;
    m_surfaces.push_back(surface);
}

void IndoorDemoLayer::BuildRoom()
{
    // 房间：x ∈ [-8, 8]，y ∈ [0, 4]，z ∈ [-6, 6]（-z 墙上有窗户）
    const glm::vec4 floorColor(0.34f, 0.33f, 0.32f, 1.0f);
    const glm::vec4 wallColor(0.72f, 0.70f, 0.66f, 1.0f);
    const glm::vec4 ceilingColor(0.80f, 0.80f, 0.78f, 1.0f);

    // 地面 / 天花板（墙体参与投影，接收面不投影）
    AddSurface({ 0.0f, -0.1f, 0.0f }, { 16.0f, 0.2f, 12.0f }, floorColor, 0.0f, 0.55f, true);
    AddSurface({ 0.0f, 4.1f, 0.0f }, { 16.0f, 0.2f, 12.0f }, ceilingColor, 0.0f, 0.9f, true);

    // 侧墙 / 后墙
    AddSurface({ -8.15f, 2.0f, 0.0f }, { 0.3f, 4.0f, 12.0f }, wallColor, 0.0f, 0.85f, true);
    AddSurface({  8.15f, 2.0f, 0.0f }, { 0.3f, 4.0f, 12.0f }, wallColor, 0.0f, 0.85f, true);
    AddSurface({  0.0f, 2.0f, 6.15f }, { 16.0f, 4.0f, 0.3f }, wallColor, 0.0f, 0.85f, true);

    // 前墙（带窗户洞口：x ∈ [-2.2, 2.2]，y ∈ [1.2, 3.2]）—— 四块拼出洞口
    AddSurface({ 0.0f, 0.6f, -6.15f }, { 16.0f, 1.2f, 0.3f }, wallColor, 0.0f, 0.85f, true);
    AddSurface({ 0.0f, 3.6f, -6.15f }, { 16.0f, 0.8f, 0.3f }, wallColor, 0.0f, 0.85f, true);
    AddSurface({ -5.1f, 2.2f, -6.15f }, { 5.8f, 2.0f, 0.3f }, wallColor, 0.0f, 0.85f, true);
    AddSurface({  5.1f, 2.2f, -6.15f }, { 5.8f, 2.0f, 0.3f }, wallColor, 0.0f, 0.85f, true);

    // 窗玻璃：自发光（HDR），相当于把室外天光"塞"进房间里
    {
        Surface pane;
        pane.mesh = m_quadMesh;
        pane.position = glm::vec3(0.0f, 2.2f, -6.0f);
        pane.rotation = glm::vec3(glm::radians(90.0f), 0.0f, 0.0f);  // +Y 法线转到 +Z（朝向室内）
        pane.scale = glm::vec3(4.4f, 1.0f, 2.0f);
        pane.color = glm::vec4(0.9f, 0.94f, 1.0f, 1.0f);
        pane.emissive = glm::vec3(1.25f, 1.5f, 2.0f);
        pane.roughness = 0.9f;
        m_surfaces.push_back(pane);
    }

    // 家具：桌子（桌面 + 两条支腿）、地毯、书架、柱子、桌面上的金属球与摆件
    const glm::vec4 woodColor(0.42f, 0.26f, 0.14f, 1.0f);
    AddSurface({ 0.0f, 0.86f, 0.5f }, { 3.2f, 0.14f, 1.8f }, woodColor, 0.0f, 0.35f, true);
    AddSurface({ -1.3f, 0.42f, 0.5f }, { 0.18f, 0.86f, 1.6f }, woodColor, 0.0f, 0.55f, true);
    AddSurface({  1.3f, 0.42f, 0.5f }, { 0.18f, 0.86f, 1.6f }, woodColor, 0.0f, 0.55f, true);

    {
        Surface rug;
        rug.mesh = m_quadMesh;
        rug.position = glm::vec3(0.0f, 0.01f, 0.5f);
        rug.scale = glm::vec3(5.4f, 1.0f, 3.6f);
        rug.color = glm::vec4(0.34f, 0.14f, 0.13f, 1.0f);
        rug.roughness = 0.95f;
        m_surfaces.push_back(rug);
    }

    // 书架（+x 墙）：三层隔板 + 两个侧板
    const glm::vec4 shelfColor(0.36f, 0.30f, 0.24f, 1.0f);
    for (int level = 0; level < 3; ++level) {
        AddSurface({ 7.0f, 1.1f + 0.8f * level, -2.5f }, { 1.6f, 0.1f, 3.0f }, shelfColor, 0.0f, 0.6f, true);
    }
    AddSurface({ 7.75f, 1.9f, -4.0f }, { 0.12f, 2.4f, 0.12f }, shelfColor, 0.0f, 0.6f, true);
    AddSurface({ 7.75f, 1.9f, -1.0f }, { 0.12f, 2.4f, 0.12f }, shelfColor, 0.0f, 0.6f, true);

    // 柱子（靠近窗户一侧，制造光影层次）
    const glm::vec4 columnColor(0.62f, 0.60f, 0.58f, 1.0f);
    AddSurface({ -5.0f, 2.0f, -1.0f }, { 0.6f, 4.0f, 0.6f }, columnColor, 0.0f, 0.8f, true);
    AddSurface({ -5.0f, 2.0f,  3.5f }, { 0.6f, 4.0f, 0.6f }, columnColor, 0.0f, 0.8f, true);

    // 桌面摆设：金属球（反射环境光）+ 两个木盒
    AddSurface({ 0.9f, 1.25f, 0.4f }, { 0.7f, 0.7f, 0.7f }, glm::vec4(0.95f, 0.93f, 0.88f, 1.0f), 1.0f, 0.12f, true);
    AddSurface({ -0.6f, 1.12f, 0.9f }, { 0.4f, 0.36f, 0.4f }, glm::vec4(0.55f, 0.32f, 0.18f, 1.0f), 0.0f, 0.45f, true);
    AddSurface({ -0.6f, 1.12f, 0.2f }, { 0.4f, 0.36f, 0.4f }, glm::vec4(0.20f, 0.42f, 0.30f, 1.0f), 0.0f, 0.45f, true);

    // 书架上的彩色书脊
    const glm::vec4 bookColors[6] = {
        { 0.70f, 0.25f, 0.22f, 1.0f }, { 0.22f, 0.38f, 0.68f, 1.0f }, { 0.75f, 0.62f, 0.22f, 1.0f },
        { 0.25f, 0.55f, 0.35f, 1.0f }, { 0.55f, 0.30f, 0.60f, 1.0f }, { 0.80f, 0.45f, 0.20f, 1.0f },
    };
    for (int i = 0; i < 6; ++i) {
        AddSurface({ 7.05f, 1.32f + 0.8f * (i % 3), -3.8f + 0.32f * i },
                   { 0.9f, 0.34f, 0.22f }, bookColors[i], 0.0f, 0.7f, true);
    }

    // 灯具外观（自发光，不投影以免挡住自己的光斑）—— 位置与光源一一对应
    for (int i = 0; i < 4; ++i) {
        Surface fixture;
        fixture.mesh = m_boxMesh;
        fixture.position = DOWNLIGHT_POSITIONS[i] + glm::vec3(0.0f, 0.06f, 0.0f);
        fixture.scale = glm::vec3(0.5f, 0.1f, 0.5f);
        fixture.color = glm::vec4(0.9f, 0.88f, 0.82f, 1.0f);
        fixture.emissive = glm::vec3(0.9f, 0.82f, 0.62f);
        fixture.roughness = 0.5f;
        m_surfaces.push_back(fixture);
    }
    for (int i = 0; i < 4; ++i) {
        Surface fixture;
        fixture.mesh = m_sphereMesh;
        fixture.position = WALL_LAMP_POSITIONS[i];
        fixture.scale = glm::vec3(0.34f);
        fixture.color = glm::vec4(1.0f, 0.86f, 0.62f, 1.0f);
        fixture.emissive = glm::vec3(1.6f, 1.05f, 0.45f);
        fixture.roughness = 0.4f;
        m_surfaces.push_back(fixture);
    }
    for (int i = 0; i < 2; ++i) {
        Surface fixture;
        fixture.mesh = m_sphereMesh;
        fixture.position = TABLE_LAMP_POSITIONS[i] + glm::vec3(0.0f, 0.18f, 0.0f);
        fixture.scale = glm::vec3(0.30f);
        fixture.color = glm::vec4(1.0f, 0.88f, 0.68f, 1.0f);
        fixture.emissive = glm::vec3(1.4f, 0.95f, 0.45f);
        fixture.roughness = 0.4f;
        m_surfaces.push_back(fixture);
    }
}

void IndoorDemoLayer::AddFixtureLights()
{
    // 吊灯：4 盏向下聚光灯，各自投射阴影（阴影贴图数组的第 i 层 = 阴影矩阵槽位 i）
    for (int i = 0; i < 4; ++i) {
        SpotLight downlight;
        downlight.position = DOWNLIGHT_POSITIONS[i];
        downlight.direction = glm::vec3(0.0f, -1.0f, 0.0f);
        downlight.innerAngle = glm::radians(18.0f);
        downlight.outerAngle = m_downlightOuterAngle;
        downlight.range = 9.0f;
        downlight.color = glm::vec3(1.0f, 0.92f, 0.78f);
        downlight.intensity = m_downlightIntensity;
        downlight.shadowIndex = m_useSpotShadows ? i : -1;
        downlight.shadowBias = 0.0025f;
        m_scene.spotLights.push_back(downlight);
    }
}

void IndoorDemoLayer::RebuildFillLights()
{
    m_fillLights.clear();

    // 压力测试光源：在房间内随机撒点，颜色按调色板循环，缓慢漂移制造动态光照
    const glm::vec3 palette[6] = {
        { 0.35f, 0.55f, 1.00f }, { 1.00f, 0.45f, 0.30f }, { 0.40f, 1.00f, 0.60f },
        { 1.00f, 0.80f, 0.35f }, { 0.75f, 0.45f, 1.00f }, { 0.35f, 0.90f, 1.00f },
    };

    uint32_t seed = 1337u;
    const auto nextRandom = [&seed]() {
        // xorshift：只用于演示用的伪随机布灯，不需要高质量随机数
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        return static_cast<float>(seed % 10000u) / 10000.0f;
    };

    for (int i = 0; i < m_fillLightCount; ++i) {
        PointLight light;
        light.position = glm::vec3(
            -7.2f + 14.4f * nextRandom(),
             0.4f +  3.2f * nextRandom(),
            -5.4f + 10.8f * nextRandom());
        // 室内小范围点光源（台灯 / 蜡烛量级）：range 决定它落在多少个 cluster 里，
        // 200 盏大 range 光源会把每个 cluster 的光源列表撑爆（UI 里的 overflow 计数会飙升）
        light.range = 1.2f + 1.3f * nextRandom();
        light.color = palette[i % 6];
        light.intensity = m_fillLightIntensity;
        m_fillLights.push_back(light);
    }
}

void IndoorDemoLayer::RecomputeProbe()
{
    const glm::vec3 probePosition(0.0f, 2.0f, 0.0f);  // 房间中心附近的视点，捕获整间屋子

    m_probe->Capture(probePosition, [this](const glm::mat4& faceView, const glm::mat4& faceProjection) {
        const DirectionalLight& sun = m_scene.directionalLights.front();

        m_probeShader->Bind();
        m_probeShader->SetMat4("u_FaceViewProjection", faceProjection * faceView);
        m_probeShader->SetFloat3("u_ProbeSunDirection", sun.direction);
        // 直射光已经由主光照计算，这里只用一部分强度近似"墙面/地面反射出去的光"，
        // 否则会把阳光在间接光里重复计一遍
        m_probeShader->SetFloat3("u_ProbeSunColor", sun.color * sun.intensity * m_probeSunScale);
        m_probeShader->SetFloat3("u_ProbeAmbient",
            m_scene.ambientColor * m_scene.ambientIntensity + glm::vec3(0.02f, 0.02f, 0.025f));

        for (const Surface& surface : m_surfaces) {
            m_probeShader->SetMat4("u_Transform", SurfaceTransform(surface));
            m_probeShader->SetFloat4("u_Albedo", surface.color);
            m_probeShader->SetFloat3("u_Emissive", surface.emissive);
            RenderCommand::DrawIndexed(surface.mesh);
        }
    });
}

void IndoorDemoLayer::OnUpdate()
{
    if (!m_active) {
        return;
    }

    // 每帧同步一次 VSync 开关（关闭后可看到真实帧耗时，便于对比剔除开 / 关的开销）
    if (m_vsync != Application::Get().GetWindow().IsVSync()) {
        Application::Get().GetWindow().SetVSync(m_vsync);
    }

    const float deltaTime = Timer::GetDeltaTime();
    m_time += deltaTime;

    // 本场景走 HDR 路径 + Forward+ cluster 剔除（两者都由引擎全局开关控制）
    Renderer::SetHDREnabled(true);
    Renderer::SetExposure(m_exposure);
    Renderer::SetLightCullingEnabled(m_useClustering);

    if (m_autoRotate) {
        m_camera.yaw += deltaTime * 0.08f;
    }

    // 动态光照：额外点光源缓慢漂移
    if (m_fillLightsAnimated) {
        for (std::size_t i = 0; i < m_fillLights.size(); ++i) {
            const float phase = m_time * 0.35f + static_cast<float>(i) * 1.7f;
            m_fillLights[i].position.x += std::sin(phase) * deltaTime * 0.35f;
            m_fillLights[i].position.z += std::cos(phase * 0.7f) * deltaTime * 0.35f;
            m_fillLights[i].position.x = glm::clamp(m_fillLights[i].position.x, -7.2f, 7.2f);
            m_fillLights[i].position.z = glm::clamp(m_fillLights[i].position.z, -5.4f, 5.4f);
            m_fillLights[i].intensity = m_fillLightIntensity * (0.75f + 0.25f * std::sin(phase * 1.3f));
        }
    } else {
        for (auto& light : m_fillLights) {
            light.intensity = m_fillLightIntensity;
        }
    }

    // ---- 同步 UI 调整到光照参数 ----
    m_scene.ambientIntensity = m_ambientIntensity;

    DirectionalLight& sun = m_scene.directionalLights.front();
    sun.direction = -glm::vec3(std::cos(m_sunPitch) * std::cos(m_sunYaw),
                               std::sin(m_sunPitch),
                               std::cos(m_sunPitch) * std::sin(m_sunYaw));
    sun.color = glm::vec3(1.0f, 0.94f, 0.84f);
    sun.intensity = m_sunIntensity;

    for (int i = 0; i < 4; ++i) {
        SpotLight& downlight = m_scene.spotLights[i];
        downlight.intensity = m_downlightIntensity;
        downlight.outerAngle = m_downlightOuterAngle;
        downlight.innerAngle = m_downlightOuterAngle * 0.45f;
        downlight.shadowIndex = m_useSpotShadows ? i : -1;
    }

    // 点光源列表：固定灯具（壁灯 / 台灯）+ 压力测试光源，每帧重建（几十~几百个，开销可忽略）
    m_scene.pointLights.clear();
    for (int i = 0; i < 4; ++i) {
        m_scene.pointLights.push_back({
            .position = WALL_LAMP_POSITIONS[i],
            .range = 7.0f,
            .color = WALL_LAMP_COLORS[i],
            .intensity = m_wallLampIntensity,
        });
    }
    for (int i = 0; i < 2; ++i) {
        m_scene.pointLights.push_back({
            .position = TABLE_LAMP_POSITIONS[i],
            .range = 4.5f,
            .color = glm::vec3(1.0f, 0.80f, 0.55f),
            .intensity = m_tableLampIntensity,
        });
    }
    m_scene.pointLights.insert(m_scene.pointLights.end(), m_fillLights.begin(), m_fillLights.end());

    m_visibleLightCount = static_cast<int>(m_scene.directionalLights.size() + m_scene.pointLights.size() + m_scene.spotLights.size());

    RenderCommand::Clear();

    // 阴影 pass 必须在 BeginScene 之前（期间相机 UBO 里是光源相机）
    RenderShadowPasses();

    Renderer::BeginScene(m_camera, m_scene);

    m_spotShadowMaps->BindDepthTexture(SPOT_SHADOW_TEXTURE_UNIT);
    m_sunShadowMap->BindDepthTexture(SUN_SHADOW_TEXTURE_UNIT);
    m_probe->BindIrradiance(PROBE_IRRADIANCE_TEXTURE_UNIT);
    m_probe->BindPrefiltered(PROBE_PREFILTER_TEXTURE_UNIT);

    m_litShader->Bind();
    m_litShader->SetInt("u_UseProbe", m_useProbe ? 1 : 0);
    m_litShader->SetFloat("u_ProbeIntensity", m_probeIntensity);

    for (const Surface& surface : m_surfaces) {
        SubmitSurface(surface);
    }

    SubmitGizmos();

    Renderer::EndScene();

    // 统计（cluster 计数需要读回 GPU，低频刷新避免每帧同步等待）
    if (++m_statsFrameCounter >= 30) {
        m_statsFrameCounter = 0;
        m_clusterStats = m_useClustering ? LightClustering::ReadStats() : LightClustering::Stats{};
    }
}

void IndoorDemoLayer::RenderShadowPasses()
{
    const DirectionalLight& sun = m_scene.directionalLights.front();

    if (m_useSunShadow) {
        const DirectionalLightShadow shadow = ComputeDirectionalLightShadow(sun, SHADOW_FOCUS, SHADOW_RADIUS);
        Renderer::BeginShadowPass(m_sunShadowMap, shadow.view, shadow.projection);
        for (const Surface& surface : m_surfaces) {
            if (surface.castsShadow) {
                Renderer::Submit(m_depthShader, surface.mesh, SurfaceTransform(surface));
            }
        }
        Renderer::EndShadowPass(m_sunShadowMap);
    }

    if (m_useSpotShadows) {
        for (const SpotLight& spot : m_scene.spotLights) {
            if (spot.shadowIndex < 0) {
                continue;
            }

            const SpotLightShadow shadow = ComputeSpotLightShadow(spot);
            // 光空间矩阵随光照 SSBO 上传（BeginScene），着色器按 flags.x 的槽位取样
            Renderer::SetLightShadowMatrix(static_cast<uint32_t>(spot.shadowIndex), shadow.lightSpaceMatrix);

            Renderer::BeginShadowPass(m_spotShadowMaps, static_cast<uint32_t>(spot.shadowIndex), shadow.view, shadow.projection);
            for (const Surface& surface : m_surfaces) {
                if (surface.castsShadow) {
                    Renderer::Submit(m_depthShader, surface.mesh, SurfaceTransform(surface));
                }
            }
            Renderer::EndShadowPass(m_spotShadowMaps);
        }
    }
}

glm::mat4 IndoorDemoLayer::SurfaceTransform(const Surface& surface) const
{
    glm::mat4 transform = glm::translate(glm::mat4(1.0f), surface.position);
    if (surface.rotation.x != 0.0f) transform = glm::rotate(transform, surface.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
    if (surface.rotation.y != 0.0f) transform = glm::rotate(transform, surface.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
    if (surface.rotation.z != 0.0f) transform = glm::rotate(transform, surface.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
    return glm::scale(transform, surface.scale);
}

void IndoorDemoLayer::SubmitSurface(const Surface& surface)
{
    m_litShader->Bind();
    m_litShader->SetMat4("u_Transform", SurfaceTransform(surface));
    m_litShader->SetFloat4("u_Albedo", surface.color);
    m_litShader->SetFloat3("u_Emissive", surface.emissive);
    m_litShader->SetFloat("u_Metallic", surface.metallic);
    m_litShader->SetFloat("u_Roughness", surface.roughness);
    m_litShader->SetFloat("u_AO", surface.ao);
    RenderCommand::DrawIndexed(surface.mesh);
}

void IndoorDemoLayer::SubmitGizmos()
{
    if (!m_showLightGizmos) {
        return;
    }

    // 压力测试光源的可视化：小圆球（平涂，颜色即光源颜色）
    m_unlitShader->Bind();
    for (const PointLight& light : m_fillLights) {
        const float scale = glm::clamp(light.range * 0.03f, 0.04f, 0.09f);
        const glm::mat4 transform = glm::scale(glm::translate(glm::mat4(1.0f), light.position), glm::vec3(scale));
        m_unlitShader->SetMat4("u_Transform", transform);
        m_unlitShader->SetFloat4("u_Color", glm::vec4(light.color, 1.0f));
        RenderCommand::DrawIndexed(m_sphereMesh);
    }
}

void IndoorDemoLayer::OnImGuiRender()
{
    if (!m_active) {
        return;
    }

    ImGui::Begin("Indoor Demo", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("FPS: %.1f (%.2f ms)", io.Framerate, io.Framerate > 0.0f ? 1000.0f / io.Framerate : 0.0f);
    ImGui::Text("Lights: %d total (%d point, %d spot, %d dir)",
        m_visibleLightCount, static_cast<int>(m_scene.pointLights.size()),
        static_cast<int>(m_scene.spotLights.size()), static_cast<int>(m_scene.directionalLights.size()));

    ImGui::Separator();
    ImGui::Text("HDR / Post");
    ImGui::SliderFloat("Exposure", &m_exposure, 0.05f, 4.0f, "%.2f");
    ImGui::Checkbox("VSync", &m_vsync);

    ImGui::Separator();
    ImGui::Text("Environment probe (IBL)");
    ImGui::Checkbox("Use probe", &m_useProbe);
    ImGui::SliderFloat("Probe intensity", &m_probeIntensity, 0.0f, 3.0f, "%.2f");
    ImGui::SliderFloat("Probe sun scale", &m_probeSunScale, 0.0f, 1.0f, "%.2f");
    if (ImGui::Button("Recompute probe")) {
        RecomputeProbe();
    }
    ImGui::TextDisabled("捕获整间屋子 -> irradiance + GGX prefilter");

    ImGui::Separator();
    ImGui::Text("Sun (window light)");
    ImGui::SliderAngle("Sun yaw", &m_sunYaw, 0.0f, 360.0f);
    ImGui::SliderAngle("Sun pitch", &m_sunPitch, 5.0f, 80.0f);
    ImGui::SliderFloat("Sun intensity", &m_sunIntensity, 0.0f, 40.0f, "%.1f");
    ImGui::Checkbox("Sun shadow", &m_useSunShadow);

    ImGui::Separator();
    ImGui::Text("Fixtures");
    ImGui::SliderFloat("Downlight intensity", &m_downlightIntensity, 0.0f, 250.0f, "%.1f");
    ImGui::SliderAngle("Downlight angle", &m_downlightOuterAngle, 10.0f, 60.0f);
    ImGui::Checkbox("Spot shadows", &m_useSpotShadows);
    ImGui::SliderFloat("Wall lamp intensity", &m_wallLampIntensity, 0.0f, 150.0f, "%.1f");
    ImGui::SliderFloat("Table lamp intensity", &m_tableLampIntensity, 0.0f, 100.0f, "%.1f");
    ImGui::SliderFloat("Ambient", &m_ambientIntensity, 0.0f, 0.5f, "%.3f");

    ImGui::Separator();
    ImGui::Text("Stress test");
    if (ImGui::SliderInt("Extra point lights", &m_fillLightCount, 0, 240)) {
        RebuildFillLights();
    }
    ImGui::SliderFloat("Extra intensity", &m_fillLightIntensity, 0.0f, 40.0f, "%.1f");
    ImGui::Checkbox("Animate extra lights", &m_fillLightsAnimated);
    ImGui::Checkbox("Show light gizmos", &m_showLightGizmos);
    ImGui::Checkbox("Light culling (Forward+)", &m_useClustering);

    if (m_useClustering) {
        const glm::ivec4 grid = LightClustering::GetGridInfo();
        ImGui::Text("Cluster grid: %d x %d tiles x %d slices = %d clusters",
            grid.x, grid.y, grid.z, grid.x * grid.y * grid.z);
        ImGui::Text("Lights per cluster: avg %.1f, max %u, overflow %u",
            m_clusterStats.averageLights, m_clusterStats.maxLights, m_clusterStats.overflowClusters);
    } else {
        ImGui::TextDisabled("Culling off: every pixel loops over all lights");
    }

    ImGui::Separator();
    ImGui::Text("Debug");
    ImGui::Checkbox("Show sun shadow map", &m_showShadowMap);

    ImGui::Separator();
    ImGui::Text("Camera");
    ImGui::Checkbox("Auto rotate", &m_autoRotate);
    ImGui::SliderAngle("Camera yaw", &m_camera.yaw, 0.0f, 360.0f);
    ImGui::SliderAngle("Camera pitch", &m_camera.pitch, -10.0f, 80.0f);
    ImGui::SliderFloat("Distance", &m_camera.distance, 2.0f, 13.0f, "%.1f");
    ImGui::SliderFloat("FOV", &m_camera.fovDegrees, 30.0f, 90.0f, "%.0f");

    ImGui::Separator();
    ImGui::Checkbox("Show ImGui demo window", &m_showDemoWindow);
    ImGui::TextDisabled("F1 飞机大战  F2 太阳系  F5 阴影演示  F6 室内多光源");
    ImGui::End();

    if (m_showShadowMap) {
        ImGui::Begin("Shadow Map (sun)", &m_showShadowMap);
        ImGui::Image(static_cast<ImTextureID>(m_sunShadowMap->GetDepthTextureRendererID()), ImVec2(256.0f, 256.0f),
                     ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
        ImGui::End();
    }

    if (m_showDemoWindow) {
        ImGui::ShowDemoWindow(&m_showDemoWindow);
    }
}

void IndoorDemoLayer::OnEvent(Event& event) {}
