#include "SolarSystem.h"

#include <cmath>
#include <imgui.h>
#include <cstdint>
#include <iostream>
#include <utility>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

using namespace OiEngine;

namespace {
    constexpr float PI = 3.14159265358979323846f;
    constexpr uint32_t SPHERE_SEGMENTS = 48;  // 球体经度分段
    constexpr uint32_t SPHERE_RINGS = 24;     // 球体纬度分段
    constexpr uint32_t ORBIT_SEGMENTS = 256;  // 轨道圆分段

    // ---------- 网格生成 ----------

    // 顶点格式：位置 (Float3) + 颜色 (Float4)，共 7 个 float
    void PushVertex(std::vector<float>& vertices, const glm::vec3& position, const glm::vec4& color)
    {
        vertices.push_back(position.x);
        vertices.push_back(position.y);
        vertices.push_back(position.z);
        vertices.push_back(color.x);
        vertices.push_back(color.y);
        vertices.push_back(color.z);
        vertices.push_back(color.w);
    }

    // UV 球体网格（球心位于局部原点，半径已烘焙进顶点，便于变换矩阵直接用平移）
    void AppendSphere(std::vector<float>& vertices, std::vector<uint32_t>& indices, float radius, const glm::vec4& color)
    {
        for (uint32_t ring = 0; ring <= SPHERE_RINGS; ++ring) {
            const float phi = PI * static_cast<float>(ring) / SPHERE_RINGS;  // 0（北极）-> PI（南极）
            for (uint32_t segment = 0; segment <= SPHERE_SEGMENTS; ++segment) {
                const float theta = 2.0f * PI * static_cast<float>(segment) / SPHERE_SEGMENTS;
                PushVertex(vertices,
                    glm::vec3(radius * std::sin(phi) * std::cos(theta),
                                  radius * std::cos(phi),
                                  radius * std::sin(phi) * std::sin(theta)),
                    color);
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

    // 平面圆环网格（位于 XZ 平面，用于土星环）
    void AppendRing(std::vector<float>& vertices, std::vector<uint32_t>& indices,
                    float innerRadius, float outerRadius, const glm::vec4& color)
    {
        for (uint32_t segment = 0; segment <= SPHERE_SEGMENTS; ++segment) {
            const float angle = 2.0f * PI * static_cast<float>(segment) / SPHERE_SEGMENTS;
            const float c = std::cos(angle);
            const float s = std::sin(angle);
            PushVertex(vertices, glm::vec3(c * innerRadius, 0.0f, s * innerRadius), color);
            PushVertex(vertices, glm::vec3(c * outerRadius, 0.0f, s * outerRadius), color);
        }

        for (uint32_t segment = 0; segment < SPHERE_SEGMENTS; ++segment) {
            const uint32_t inner = segment * 2;
            const uint32_t outer = inner + 1;
            indices.push_back(inner);
            indices.push_back(outer);
            indices.push_back(inner + 2);
            indices.push_back(inner + 2);
            indices.push_back(outer);
            indices.push_back(outer + 2);
        }
    }

    Ref<VertexArray> CreateVertexArray(std::vector<float>& vertices)
    {
        Ref<VertexArray> vertexArray = VertexArray::Create();

        Ref<VertexBuffer> vertexBuffer = VertexBuffer::Create(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(float)));
        vertexBuffer->SetLayout({
            { ShaderDataType::Float3, "a_Position" },
            { ShaderDataType::Float4, "a_Color" },
        });
        vertexArray->AddVertexBuffer(vertexBuffer);
        return vertexArray;
    }

    Ref<VertexArray> CreateMesh(std::vector<float>& vertices, std::vector<uint32_t>& indices)
    {
        Ref<VertexArray> vertexArray = CreateVertexArray(vertices);

        Ref<IndexBuffer> indexBuffer = IndexBuffer::Create(indices.data(), static_cast<uint32_t>(indices.size()));
        vertexArray->SetIndexBuffer(indexBuffer);
        return vertexArray;
    }

    // 着色器资源见 assets/shader/scenes/solar_system_*.glsl

    // 行星参数：轨道半径、天体半径、颜色、公转角速度（rad/s）、初始相位、是否有环
    struct PlanetDefinition {
        float orbitRadius;
        float radius;
        glm::vec4 color;
        float angularSpeed;
        float startAngle;
        bool hasRing;
    };

    const PlanetDefinition PLANETS[] = {
        {  2.2f, 0.13f, glm::vec4(0.72f, 0.68f, 0.62f, 1.0f), 0.650f, 0.0f, false },  // 水星
        {  3.0f, 0.20f, glm::vec4(0.93f, 0.76f, 0.42f, 1.0f), 0.420f, 1.9f, false },  // 金星
        {  3.9f, 0.22f, glm::vec4(0.25f, 0.50f, 0.90f, 1.0f), 0.300f, 3.6f, false },  // 地球
        {  4.9f, 0.17f, glm::vec4(0.85f, 0.38f, 0.22f, 1.0f), 0.220f, 5.1f, false },  // 火星
        {  6.8f, 0.44f, glm::vec4(0.88f, 0.72f, 0.55f, 1.0f), 0.120f, 2.4f, false },  // 木星
        {  8.6f, 0.36f, glm::vec4(0.85f, 0.78f, 0.58f, 1.0f), 0.085f, 4.3f, true  },  // 土星（带环）
        { 10.4f, 0.26f, glm::vec4(0.55f, 0.82f, 0.88f, 1.0f), 0.060f, 0.9f, false },  // 天王星
        { 12.0f, 0.25f, glm::vec4(0.30f, 0.42f, 0.88f, 1.0f), 0.048f, 5.8f, false },  // 海王星
    };
}

// ---------- SolarSystemCamera ----------

glm::vec3 SolarSystemCamera::GetPosition() const
{
    return glm::vec3(0.0f, 11.0f, 19.0f);
}

glm::mat4 SolarSystemCamera::GetViewMatrix() const
{
    return glm::lookAt(GetPosition(), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 SolarSystemCamera::GetProjectionMatrix() const
{
    const uint32_t width = Application::Get().GetWindow().GetWidth();
    const uint32_t height = Application::Get().GetWindow().GetHeight();
    const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 16.0f / 9.0f;
    return glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
}

// ---------- SolarSystemLayer ----------

void SolarSystemLayer::OnAttach()
{
    m_litShader = Shader::Create("assets/shader/scenes/solar_system_lit.glsl");
    m_unlitShader = Shader::Create("assets/shader/scenes/solar_system_unlit.glsl");
    m_glowShader = Shader::Create("assets/shader/scenes/solar_system_glow.glsl");

    // 太阳
    {
        std::vector<float> vertices;
        std::vector<uint32_t> indices;
        AppendSphere(vertices, indices, 1.0f, glm::vec4(1.0f, 0.88f, 0.42f, 1.0f));
        m_sunMesh = CreateMesh(vertices, indices);
    }

    // 太阳光晕：稍大的半透明球
    {
        std::vector<float> vertices;
        std::vector<uint32_t> indices;
        AppendSphere(vertices, indices, 1.35f, glm::vec4(1.0f, 0.72f, 0.26f, 0.55f));
        m_glowMesh = CreateMesh(vertices, indices);
    }

    // 行星
    for (const auto& definition : PLANETS) {
        CelestialBody body;
        body.orbitRadius = definition.orbitRadius;
        body.angularSpeed = definition.angularSpeed;
        body.startAngle = definition.startAngle;

        std::vector<float> vertices;
        std::vector<uint32_t> indices;
        AppendSphere(vertices, indices, definition.radius, definition.color);
        body.mesh = CreateMesh(vertices, indices);

        if (definition.hasRing) {
            std::vector<float> ringVertices;
            std::vector<uint32_t> ringIndices;
            AppendRing(ringVertices, ringIndices,
                       definition.radius * 1.5f, definition.radius * 2.3f,
                       glm::vec4(0.82f, 0.76f, 0.58f, 1.0f));
            body.ringMesh = CreateMesh(ringVertices, ringIndices);
        }

        m_planets.push_back(std::move(body));
    }

    // 所有行星的轨道线合并为一个顶点数组，一次绘制
    {
        std::vector<float> vertices;
        const glm::vec4 orbitColor(0.42f, 0.48f, 0.62f, 0.4f);
        for (const auto& definition : PLANETS) {
            for (uint32_t segment = 0; segment < ORBIT_SEGMENTS; ++segment) {
                const float angle0 = 2.0f * PI * static_cast<float>(segment) / ORBIT_SEGMENTS;
                const float angle1 = 2.0f * PI * static_cast<float>(segment + 1) / ORBIT_SEGMENTS;
                PushVertex(vertices,
                    glm::vec3(std::cos(angle0) * definition.orbitRadius, 0.0f, std::sin(angle0) * definition.orbitRadius),
                    orbitColor);
                PushVertex(vertices,
                    glm::vec3(std::cos(angle1) * definition.orbitRadius, 0.0f, std::sin(angle1) * definition.orbitRadius),
                    orbitColor);
            }
        }
        m_orbitMesh = CreateVertexArray(vertices);
        m_orbitVertexCount = static_cast<uint32_t>(vertices.size() / 7);
    }

    // 光照：微弱冷色环境光 + 位于太阳位置的点光源（行星面向太阳一侧被照亮）
    m_scene.ambientColor = glm::vec3(0.35f, 0.42f, 0.60f);
    m_scene.ambientIntensity = m_ambientIntensity;
    m_scene.pointLights.push_back({
        .position = glm::vec3(0.0f, 0.0f, 0.0f),
        .range = m_sunRange,
        .color = glm::vec3(1.0f, 0.86f, 0.58f),
        .intensity = m_sunIntensity,
    });

    RenderCommand::SetClearColor(glm::vec4(0.02f, 0.02f, 0.05f, 1.0f));
}

void SolarSystemLayer::OnDetach() {}

void SolarSystemLayer::OnUpdate()
{
    if (!m_active) {
        return;
    }

    // 本场景为非 HDR 路径：显式声明引擎全局渲染状态，
    // 否则从 HDR 场景（F5 阴影演示 / F6 室内多光源）切过来会残留 HDR 开关（颜色被二次 tonemap → 整体泛白）
    Renderer::SetHDREnabled(false);
    Renderer::SetLightCullingEnabled(false);  // 本场景不读 cluster 列表，避免空跑 compute 剔除

    RenderCommand::Clear();

    // 时间推进受 UI 控制（暂停 / 倍速），不直接依赖全局启动时间
    if (!m_paused) {
        m_simulationTime += Timer::GetDeltaTime() * m_timeScale;
    }

    const float elapsed = m_simulationTime;

    // 按公转角速度更新行星位置（XZ 平面上的圆周运动）
    for (auto& planet : m_planets) {
        const float angle = planet.startAngle + planet.angularSpeed * elapsed;
        planet.position = glm::vec3(std::cos(angle) * planet.orbitRadius,
                                        0.0f,
                                        std::sin(angle) * planet.orbitRadius);
    }

    // 同步 UI 调整到光照参数
    m_scene.ambientIntensity = m_ambientIntensity;
    PointLight& sunLight = m_scene.pointLights.front();
    sunLight.intensity = m_sunIntensity;
    sunLight.range = m_sunRange;

    // 开始场景：先上传相机 / 光照 UBO，此后所有绘制都能从 UBO 读取
    Renderer::BeginScene(m_camera, m_scene);

    // 1. 轨道线
    UseUnlitShader(glm::mat4(1.0f));
    RenderCommand::DrawLines(m_orbitMesh, m_orbitVertexCount);

    // 2. 太阳（均匀发光，不走光照）
    UseUnlitShader(glm::mat4(1.0f));
    RenderCommand::DrawIndexed(m_sunMesh);

    // 3. 行星（引擎光照系统：环境光 + 太阳点光源，Lambert 漫反射）
    for (const auto& planet : m_planets) {
        Renderer::Submit(m_litShader, planet.mesh, glm::translate(glm::mat4(1.0f), planet.position));
    }

    // 4. 土星环
    for (const auto& planet : m_planets) {
        if (planet.ringMesh) {
            UseUnlitShader(glm::translate(glm::mat4(1.0f), planet.position));
            RenderCommand::DrawIndexed(planet.ringMesh);
        }
    }

    // 5. 太阳光晕（半透明，最后绘制以保证混合正确）
    Renderer::Submit(m_glowShader, m_glowMesh);

    Renderer::EndScene();
}

void SolarSystemLayer::OnImGuiRender()
{
    if (!m_active) {
        return;
    }

    // AlwaysAutoResize：面板宽度随内容自适应，不受 imgui.ini 记录的旧尺寸影响
    ImGui::Begin("Solar System", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("FPS: %.1f (%.2f ms)", io.Framerate, io.Framerate > 0.0f ? 1000.0f / io.Framerate : 0.0f);
    ImGui::Text("Simulation time: %.1f s (%s)", m_simulationTime, m_paused ? "paused" : "running");

    ImGui::Separator();

    ImGui::SliderFloat("Time scale", &m_timeScale, 0.0f, 5.0f, "%.2fx");
    if (ImGui::Button(m_paused ? "Resume" : "Pause")) {
        m_paused = !m_paused;
    }

    ImGui::Separator();
    ImGui::Text("Lighting");
    ImGui::SliderFloat("Ambient", &m_ambientIntensity, 0.0f, 0.5f, "%.3f");
    ImGui::SliderFloat("Sun intensity", &m_sunIntensity, 0.0f, 800.0f, "%.0f");
    ImGui::SliderFloat("Sun range", &m_sunRange, 10.0f, 80.0f, "%.1f");

    ImGui::Separator();
    ImGui::Checkbox("Show ImGui demo window", &m_showDemoWindow);
    ImGui::TextDisabled("F1 飞机大战  F2 太阳系");
    ImGui::End();

    if (m_showDemoWindow) {
        ImGui::ShowDemoWindow(&m_showDemoWindow);
    }
}

void SolarSystemLayer::OnEvent(Event& event) {}

// 轨道线、环等几何不参与光照；相机 uniform 由 BeginScene 上传的 UBO 提供，这里只需模型变换
void SolarSystemLayer::UseUnlitShader(const glm::mat4& transform)
{
    m_unlitShader->Bind();
    m_unlitShader->SetMat4("u_Transform", transform);
}
