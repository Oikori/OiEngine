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

    // 带简单 Lambert 光照的着色器（用于行星，位置方向即球面法线）
    const char* LIT_VERTEX_SHADER = R"(
        #version 450 core

        layout(location = 0) in vec3 a_Position;
        layout(location = 1) in vec4 a_Color;

        uniform mat4 u_Transform;
        uniform mat4 u_view;
        uniform mat4 u_projection;

        out vec3 v_Normal;
        out vec4 v_Color;

        void main()
        {
            gl_Position = u_projection * u_view * u_Transform * vec4(a_Position, 1.0);
            v_Normal = a_Position;
            v_Color = a_Color;
        }
    )";

    const char* LIT_FRAGMENT_SHADER = R"(
        #version 450 core

        layout(location = 0) out vec4 o_Color;

        in vec3 v_Normal;
        in vec4 v_Color;

        void main()
        {
            vec3 normal = normalize(v_Normal);
            vec3 lightDirection = normalize(vec3(0.5, 0.8, 0.6));
            float diffuse = max(dot(normal, lightDirection), 0.0);
            o_Color = vec4(v_Color.rgb * (0.35 + 0.65 * diffuse), v_Color.a);
        }
    )";

    // 不受光照影响的着色器（用于太阳、轨道线、土星环、光晕）
    const char* UNLIT_VERTEX_SHADER = R"(
        #version 450 core

        layout(location = 0) in vec3 a_Position;
        layout(location = 1) in vec4 a_Color;

        uniform mat4 u_Transform;
        uniform mat4 u_view;
        uniform mat4 u_projection;

        out vec4 v_Color;

        void main()
        {
            gl_Position = u_projection * u_view * u_Transform * vec4(a_Position, 1.0);
            v_Color = a_Color;
        }
    )";

    const char* UNLIT_FRAGMENT_SHADER = R"(
        #version 450 core

        layout(location = 0) out vec4 o_Color;

        in vec4 v_Color;

        void main()
        {
            o_Color = v_Color;
        }
    )";

    // 太阳光晕：正对相机的区域最亮，边缘渐隐（Renderer::Submit 会自动设置 u_cameraPosition）
    const char* GLOW_VERTEX_SHADER = R"(
        #version 450 core

        layout(location = 0) in vec3 a_Position;
        layout(location = 1) in vec4 a_Color;

        uniform mat4 u_Transform;
        uniform mat4 u_view;
        uniform mat4 u_projection;

        out vec3 v_WorldPosition;
        out vec3 v_Normal;
        out vec4 v_Color;

        void main()
        {
            vec4 worldPosition = u_Transform * vec4(a_Position, 1.0);
            gl_Position = u_projection * u_view * worldPosition;
            v_WorldPosition = worldPosition.xyz;
            v_Normal = a_Position;  // 球心位于局部原点，位置方向即外法线方向
            v_Color = a_Color;
        }
    )";

    const char* GLOW_FRAGMENT_SHADER = R"(
        #version 450 core

        layout(location = 0) out vec4 o_Color;

        in vec3 v_WorldPosition;
        in vec3 v_Normal;
        in vec4 v_Color;

        uniform vec3 u_cameraPosition;

        void main()
        {
            vec3 viewDirection = normalize(u_cameraPosition - v_WorldPosition);
            float facing = abs(dot(normalize(v_Normal), viewDirection));
            o_Color = vec4(v_Color.rgb, v_Color.a * pow(facing, 1.5));
        }
    )";

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
    m_litShader = Shader::Create("SolarSystemLit", LIT_VERTEX_SHADER, LIT_FRAGMENT_SHADER);
    m_unlitShader = Shader::Create("SolarSystemUnlit", UNLIT_VERTEX_SHADER, UNLIT_FRAGMENT_SHADER);
    m_glowShader = Shader::Create("SolarSystemGlow", GLOW_VERTEX_SHADER, GLOW_FRAGMENT_SHADER);

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

    RenderCommand::SetClearColor(glm::vec4(0.02f, 0.02f, 0.05f, 1.0f));

    std::cout << "SolarSystem OnAttach" << std::endl;
}

void SolarSystemLayer::OnDetach() {}

void SolarSystemLayer::OnUpdate()
{
    if (!m_active) {
        return;
    }

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

    // 1. 轨道线
    UseUnlitShader(glm::mat4(1.0f));
    RenderCommand::DrawLines(m_orbitMesh, m_orbitVertexCount);

    // 2. 太阳（均匀发光，不走光照）
    UseUnlitShader(glm::mat4(1.0f));
    RenderCommand::DrawIndexed(m_sunMesh);

    // 3. 行星（Lambert 光照体现球体体积感）
    Renderer::BeginScene(m_camera);
    for (const auto& planet : m_planets) {
        Renderer::Submit(m_litShader, planet.mesh, glm::translate(glm::mat4(1.0f), planet.position));
    }
    Renderer::EndScene();

    // 4. 土星环
    for (const auto& planet : m_planets) {
        if (planet.ringMesh) {
            UseUnlitShader(glm::translate(glm::mat4(1.0f), planet.position));
            RenderCommand::DrawIndexed(planet.ringMesh);
        }
    }

    // 5. 太阳光晕（半透明，最后绘制以保证混合正确）
    Renderer::BeginScene(m_camera);
    Renderer::Submit(m_glowShader, m_glowMesh);
    Renderer::EndScene();
}

void SolarSystemLayer::OnImGuiRender()
{
    if (!m_active) {
        return;
    }

    ImGui::Begin("Solar System");

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("FPS: %.1f (%.2f ms)", io.Framerate, io.Framerate > 0.0f ? 1000.0f / io.Framerate : 0.0f);
    ImGui::Text("Simulation time: %.1f s (%s)", m_simulationTime, m_paused ? "paused" : "running");

    ImGui::Separator();

    ImGui::SliderFloat("Time scale", &m_timeScale, 0.0f, 5.0f, "%.2fx");
    if (ImGui::Button(m_paused ? "Resume" : "Pause")) {
        m_paused = !m_paused;
    }

    ImGui::Separator();
    ImGui::Checkbox("Show ImGui demo window", &m_showDemoWindow);
    ImGui::TextDisabled("F1 飞机大战  F2 太阳系");
    ImGui::End();

    if (m_showDemoWindow) {
        ImGui::ShowDemoWindow(&m_showDemoWindow);
    }
}

void SolarSystemLayer::OnEvent(Event& event) {}

// 轨道线、环等几何不参与光照，需要手动设置相机的 uniform
void SolarSystemLayer::UseUnlitShader(const glm::mat4& transform)
{
    m_unlitShader->Bind();
    m_unlitShader->SetMat4("u_Transform", transform);
    m_unlitShader->SetMat4("u_view", m_camera.GetViewMatrix());
    m_unlitShader->SetMat4("u_projection", m_camera.GetProjectionMatrix());
}
