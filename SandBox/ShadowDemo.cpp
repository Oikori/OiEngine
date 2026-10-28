#include "ShadowDemo.h"

#include <cmath>
#include <cstdint>
#include <imgui.h>
#include <iostream>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

using namespace OiEngine;

namespace {
    constexpr float PI = 3.14159265358979323846f;
    constexpr uint32_t SPHERE_SEGMENTS = 48;  // 球体经度分段
    constexpr uint32_t SPHERE_RINGS = 24;     // 球体纬度分段
    constexpr uint32_t SHADOW_TEXTURE_UNIT = 1;  // 光照着色器采样阴影贴图的纹理单元（与 u_ShadowMap 一致）

    // ---------- 网格生成 ----------

    // 顶点格式：位置 (Float3) + 法线 (Float3) + 颜色 (Float4)，共 10 个 float
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

    // 单位球（半径 1，球心位于局部原点）；球体法线即位置方向
    void AppendSphere(std::vector<float>& vertices, std::vector<uint32_t>& indices)
    {
        const glm::vec4 white(1.0f, 1.0f, 1.0f, 1.0f);

        for (uint32_t ring = 0; ring <= SPHERE_RINGS; ++ring) {
            const float phi = PI * static_cast<float>(ring) / SPHERE_RINGS;  // 0（北极）-> PI（南极）
            for (uint32_t segment = 0; segment <= SPHERE_SEGMENTS; ++segment) {
                const float theta = 2.0f * PI * static_cast<float>(segment) / SPHERE_SEGMENTS;
                const glm::vec3 position(std::sin(phi) * std::cos(theta),
                                         std::cos(phi),
                                         std::sin(phi) * std::sin(theta));
                PushVertex(vertices, position, position, white);
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

    // 轴对齐单位立方体（边长 1，-0.5 ~ 0.5），每面独立法线
    void AppendBox(std::vector<float>& vertices, std::vector<uint32_t>& indices)
    {
        constexpr float h = 0.5f;
        const glm::vec3 positions[8] = {
            { -h, -h, -h }, {  h, -h, -h }, {  h,  h, -h }, { -h,  h, -h },  // z = -h
            { -h, -h,  h }, {  h, -h,  h }, {  h,  h,  h }, { -h,  h,  h },  // z = +h
        };
        const glm::vec3 normals[6] = {
            {  0.0f,  0.0f,  1.0f },  // +z
            {  0.0f,  0.0f, -1.0f },  // -z
            { -1.0f,  0.0f,  0.0f },  // -x
            {  1.0f,  0.0f,  0.0f },  // +x
            {  0.0f, -1.0f,  0.0f },  // -y
            {  0.0f,  1.0f,  0.0f },  // +y
        };
        // 每面的 4 个角（外侧看逆时针）
        const uint32_t faces[6][4] = {
            { 4, 5, 6, 7 },  // +z
            { 0, 3, 2, 1 },  // -z
            { 0, 4, 7, 3 },  // -x
            { 1, 2, 6, 5 },  // +x
            { 0, 1, 5, 4 },  // -y
            { 3, 7, 6, 2 },  // +y
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

    // 地面：单个方形平面（y = 0，法线朝上）
    void AppendGround(std::vector<float>& vertices, std::vector<uint32_t>& indices, float halfSize)
    {
        const glm::vec3 normal(0.0f, 1.0f, 0.0f);
        const glm::vec4 white(1.0f, 1.0f, 1.0f, 1.0f);
        PushVertex(vertices, glm::vec3(-halfSize, 0.0f, -halfSize), normal, white);
        PushVertex(vertices, glm::vec3(-halfSize, 0.0f,  halfSize), normal, white);
        PushVertex(vertices, glm::vec3( halfSize, 0.0f,  halfSize), normal, white);
        PushVertex(vertices, glm::vec3( halfSize, 0.0f, -halfSize), normal, white);
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

// ---------- ShadowDemoCamera ----------

glm::vec3 ShadowDemoCamera::GetPosition() const
{
    const float cosPitch = std::cos(pitch);
    return target + distance * glm::vec3(cosPitch * std::cos(yaw), std::sin(pitch), cosPitch * std::sin(yaw));
}

glm::mat4 ShadowDemoCamera::GetViewMatrix() const
{
    return glm::lookAt(GetPosition(), target, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 ShadowDemoCamera::GetProjectionMatrix() const
{
    const uint32_t width = Application::Get().GetWindow().GetWidth();
    const uint32_t height = Application::Get().GetWindow().GetHeight();
    const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 16.0f / 9.0f;
    return glm::perspective(glm::radians(45.0f), aspect, 0.1f, 200.0f);
}

// ---------- ShadowDemoLayer ----------

void ShadowDemoLayer::OnAttach()
{
    m_litShader = Shader::Create("assets/shader/scenes/shadow_demo_lit.glsl");
    m_depthShader = Shader::Create("assets/shader/scenes/shadow_demo_depth.glsl");
    m_sunShader = Shader::Create("assets/shader/scenes/shadow_demo_sun.glsl");
    m_shadowMap = CreateRef<ShadowMap>(2048);

    // 地面（只接收阴影，不参与投影）
    {
        std::vector<float> vertices;
        std::vector<uint32_t> indices;
        AppendGround(vertices, indices, 24.0f);
        m_groundMesh = CreateMesh(vertices, indices);
    }

    // 单位立方体与单位球（所有投影物体共用）
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

    // 场景布置：中心高塔 + 旋转方块 + 悬浮球 + 矮墙等，全部参与阴影投影；
    // 每个实例带独立 PBR 材质（金属度 / 粗糙度不同，便于对比镜面高光）
    const auto addInstance = [this](const glm::vec4& color, const glm::vec3& position, const glm::vec3& scale,
                                    bool sphere, float spinSpeed, float bobAmplitude, float bobPhase) -> Instance&
    {
        Instance instance;
        instance.mesh = sphere ? m_sphereMesh : m_boxMesh;
        instance.color = color;
        instance.position = position;
        instance.basePosition = position;
        instance.scale = scale;
        instance.spinSpeed = spinSpeed;
        instance.bobAmplitude = bobAmplitude;
        instance.bobPhase = bobPhase;
        m_instances.push_back(instance);
        return m_instances.back();
    };

    Instance& tower = addInstance(glm::vec4(0.85f, 0.36f, 0.30f, 1.0f), glm::vec3( 0.0f, 2.0f,  0.0f), glm::vec3(2.0f, 4.0f, 2.0f), false, 0.0f, 0.0f, 0.0f);   // 中心高塔
    tower.roughness = 0.75f;

    Instance& metalBox = addInstance(glm::vec4(0.30f, 0.55f, 0.90f, 1.0f), glm::vec3(-4.2f, 0.9f,  1.6f), glm::vec3(1.8f, 1.8f, 1.8f), false, 0.9f, 0.0f, 0.0f);  // 旋转方块（金属）
    metalBox.metallic = 0.9f;
    metalBox.roughness = 0.25f;

    Instance& glossySphere = addInstance(glm::vec4(0.25f, 0.80f, 0.70f, 1.0f), glm::vec3( 3.6f, 2.6f,  0.4f), glm::vec3(1.6f, 1.6f, 1.6f), true,  0.0f, 0.6f, 0.0f);  // 悬浮球（光滑非金属）
    glossySphere.roughness = 0.15f;
    m_glossySphereIndex = m_instances.size() - 1;  // UI 里可实时调节该球的材质

    Instance& goldBox = addInstance(glm::vec4(0.90f, 0.75f, 0.30f, 1.0f), glm::vec3( 2.6f, 0.6f,  3.4f), glm::vec3(1.2f, 1.2f, 1.2f), false, 1.4f, 0.0f, 0.0f);   // 小方块（金属）
    goldBox.metallic = 1.0f;
    goldBox.roughness = 0.40f;

    Instance& wall = addInstance(glm::vec4(0.50f, 0.46f, 0.74f, 1.0f), glm::vec3(-0.5f, 0.5f, -5.6f), glm::vec3(9.0f, 1.0f, 0.8f), false, 0.0f, 0.0f, 0.0f);   // 矮墙
    wall.roughness = 0.90f;

    Instance& orb = addInstance(glm::vec4(0.90f, 0.55f, 0.25f, 1.0f), glm::vec3(-2.4f, 0.6f, -2.6f), glm::vec3(1.2f, 1.2f, 1.2f), true,  0.0f, 0.0f, 0.0f);   // 小球（半金属）
    orb.metallic = 0.6f;
    orb.roughness = 0.30f;

    // 光照：暖色太阳方向光（每帧更新方向）+ 微弱冷色环境光；PBR 下环境光同时提供环境镜面近似
    m_scene.ambientColor = glm::vec3(0.45f, 0.52f, 0.65f);
    m_scene.ambientIntensity = m_ambientIntensity;
    m_scene.directionalLights.push_back(DirectionalLight{});

    RenderCommand::SetClearColor(glm::vec4(0.03f, 0.04f, 0.07f, 1.0f));

    // 阴影贴图固定绑定到约定纹理单元，只需设置一次
    m_litShader->Bind();
    m_litShader->SetInt("u_ShadowMap", SHADOW_TEXTURE_UNIT);
}

void ShadowDemoLayer::OnDetach() {}

void ShadowDemoLayer::OnUpdate()
{
    if (!m_active) {
        return;
    }

    // HDR 路径：本场景输出线性 HDR 颜色，曝光 / tonemap 交给引擎后处理链
    Renderer::SetHDREnabled(true);
    Renderer::SetExposure(m_exposure);
    // 本场景着色器遍历全部光源、不读 cluster 列表：关掉剔除，避免残留 F6 的全局开关空跑 compute
    Renderer::SetLightCullingEnabled(false);

    RenderCommand::Clear();

    const float deltaTime = Timer::GetDeltaTime();
    m_time += deltaTime;

    // 实例动画：自转 + 悬浮浮动
    for (auto& instance : m_instances) {
        instance.angle += instance.spinSpeed * deltaTime;
        if (instance.bobAmplitude != 0.0f) {
            instance.position.y = instance.basePosition.y + std::sin(m_time * 1.8f + instance.bobPhase) * instance.bobAmplitude;
        }
    }

    if (m_autoRotate) {
        m_camera.yaw += deltaTime * 0.15f;
    }

    // 太阳：方向由 UI 角度决定（direction 为传播方向，与指向太阳的方向相反）
    const glm::vec3 toSun = SunDirection();
    DirectionalLight& sun = m_scene.directionalLights.front();
    sun.direction = -toSun;
    sun.color = glm::vec3(1.0f, 0.95f, 0.86f);
    sun.intensity = m_sunIntensity;
    m_scene.ambientIntensity = m_ambientIntensity;
    m_shadowMap->SetBias(m_shadowBias);

    // 1. 阴影 pass：从光源视角把所有投影物体渲染到深度贴图
    const DirectionalLightShadow lightShadow = ComputeDirectionalLightShadow(sun, glm::vec3(0.0f, 2.0f, 0.0f), m_shadowRadius);
    Renderer::BeginShadowPass(m_shadowMap, lightShadow.view, lightShadow.projection);
    for (const auto& instance : m_instances) {
        Renderer::Submit(m_depthShader, instance.mesh, InstanceTransform(instance));
    }
    Renderer::EndShadowPass(m_shadowMap);

    // 2. 主场景：阴影贴图绑定到约定纹理单元，光照着色器里做 PBR 着色（方向光含 PCF 阴影）
    Renderer::BeginScene(m_camera, m_scene);
    m_shadowMap->BindDepthTexture(SHADOW_TEXTURE_UNIT);

    // 地面（只接收阴影，不参与投影；粗糙的非金属材质）
    m_litShader->Bind();
    m_litShader->SetMat4("u_Transform", glm::mat4(1.0f));
    m_litShader->SetFloat4("u_Albedo", glm::vec4(0.55f, 0.58f, 0.62f, 1.0f));
    m_litShader->SetFloat("u_Metallic", 0.0f);
    m_litShader->SetFloat("u_Roughness", 0.85f);
    m_litShader->SetFloat("u_AO", 1.0f);
    RenderCommand::DrawIndexed(m_groundMesh);

    // 投影物体
    for (const auto& instance : m_instances) {
        SubmitInstance(instance);
    }

    // 太阳指示球（不参与投影，仅提示光的方向）
    m_sunShader->Bind();
    m_sunShader->SetMat4("u_Transform",
        glm::translate(glm::mat4(1.0f), toSun * 24.0f) * glm::scale(glm::mat4(1.0f), glm::vec3(1.8f, 1.8f, 1.8f)));
    RenderCommand::DrawIndexed(m_sphereMesh);

    Renderer::EndScene();
}

void ShadowDemoLayer::OnImGuiRender()
{
    if (!m_active) {
        return;
    }

    // AlwaysAutoResize：面板宽度随内容自适应，不受 imgui.ini 记录的旧尺寸影响
    ImGui::Begin("Shadow Demo", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("FPS: %.1f (%.2f ms)", io.Framerate, io.Framerate > 0.0f ? 1000.0f / io.Framerate : 0.0f);

    ImGui::Separator();
    ImGui::Text("Lighting (PBR)");
    ImGui::SliderAngle("Sun yaw", &m_sunYaw, 0.0f, 360.0f);
    ImGui::SliderAngle("Sun pitch", &m_sunPitch, 10.0f, 85.0f);
    ImGui::SliderFloat("Sun intensity", &m_sunIntensity, 0.0f, 8.0f, "%.2f");
    ImGui::SliderFloat("Ambient", &m_ambientIntensity, 0.0f, 1.0f, "%.3f");
    ImGui::SliderFloat("Exposure", &m_exposure, 0.1f, 3.0f, "%.2f");

    // 悬浮球材质：实时对比金属度 / 粗糙度对高光的影响
    ImGui::Separator();
    ImGui::Text("Material (glossy sphere)");
    Instance& glossySphere = m_instances[m_glossySphereIndex];
    ImGui::SliderFloat("Metallic", &glossySphere.metallic, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Roughness", &glossySphere.roughness, 0.05f, 1.0f, "%.2f");

    ImGui::Separator();
    ImGui::Text("Shadow");
    ImGui::SliderFloat("Bias", &m_shadowBias, 0.0f, 0.01f, "%.4f");
    ImGui::SliderFloat("Ortho radius", &m_shadowRadius, 6.0f, 20.0f, "%.1f");
    ImGui::Checkbox("Show shadow map", &m_showShadowMap);

    ImGui::Separator();
    ImGui::Text("Camera");
    ImGui::Checkbox("Auto rotate", &m_autoRotate);
    ImGui::SliderAngle("Camera yaw", &m_camera.yaw, 0.0f, 360.0f);
    ImGui::SliderAngle("Camera pitch", &m_camera.pitch, 5.0f, 85.0f);
    ImGui::SliderFloat("Distance", &m_camera.distance, 8.0f, 35.0f, "%.1f");

    ImGui::Separator();
    ImGui::Checkbox("Show ImGui demo window", &m_showDemoWindow);
    ImGui::TextDisabled("F1 飞机大战  F2 太阳系  F5 阴影演示");
    ImGui::End();

    if (m_showShadowMap) {
        ImGui::Begin("Shadow Map", &m_showShadowMap);
        // 纹理原点在左下，ImGui 图像原点在左上，翻转 V 轴
        ImGui::Image(static_cast<ImTextureID>(m_shadowMap->GetDepthTextureRendererID()), ImVec2(256.0f, 256.0f),
                     ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
        ImGui::End();
    }

    if (m_showDemoWindow) {
        ImGui::ShowDemoWindow(&m_showDemoWindow);
    }
}

void ShadowDemoLayer::OnEvent(Event& event) {}

glm::mat4 ShadowDemoLayer::InstanceTransform(const Instance& instance) const
{
    glm::mat4 transform = glm::translate(glm::mat4(1.0f), instance.position);
    transform = glm::rotate(transform, instance.angle, glm::vec3(0.0f, 1.0f, 0.0f));
    transform = glm::scale(transform, instance.scale);
    return transform;
}

glm::vec3 ShadowDemoLayer::SunDirection() const
{
    const float cosPitch = std::cos(m_sunPitch);
    return glm::vec3(cosPitch * std::cos(m_sunYaw), std::sin(m_sunPitch), cosPitch * std::sin(m_sunYaw));
}

void ShadowDemoLayer::SubmitInstance(const Instance& instance)
{
    m_litShader->Bind();
    m_litShader->SetMat4("u_Transform", InstanceTransform(instance));
    m_litShader->SetFloat4("u_Albedo", instance.color);
    m_litShader->SetFloat("u_Metallic", instance.metallic);
    m_litShader->SetFloat("u_Roughness", instance.roughness);
    m_litShader->SetFloat("u_AO", instance.ao);
    RenderCommand::DrawIndexed(instance.mesh);
}
