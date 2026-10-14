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

    // ---------- 着色器 ----------

    // 光照着色器：相比太阳系版本增加了顶点法线（立方体需要）、u_Tint 与方向光阴影（3x3 PCF）
    const char* LIT_VERTEX_SHADER = R"(
        #version 450 core

        layout(location = 0) in vec3 a_Position;
        layout(location = 1) in vec3 a_Normal;
        layout(location = 2) in vec4 a_Color;

        layout(std140, binding = 0) uniform Camera {
            mat4 u_Projection;
            mat4 u_View;
            vec4 u_CameraPosition;  // xyz: 世界空间位置（本着色器未用）
        };

        uniform mat4 u_Transform;

        out vec3 v_WorldPosition;
        out vec3 v_Normal;
        out vec4 v_Color;

        void main()
        {
            vec4 worldPosition = u_Transform * vec4(a_Position, 1.0);
            gl_Position = u_Projection * u_View * worldPosition;
            v_WorldPosition = worldPosition.xyz;
            // 法线用逆转置矩阵变换，兼容非均匀缩放
            v_Normal = mat3(transpose(inverse(u_Transform))) * a_Normal;
            v_Color = a_Color;
        }
    )";

    const char* LIT_FRAGMENT_SHADER = R"(
        #version 450 core

        #define MAX_LIGHTS 32

        // 与 C++ 侧 LightingData / LightData 的 std140 布局严格对应
        struct Light {
            vec4 positionType;   // xyz: 世界空间位置（Point/Spot）或传播方向（Directional）；w: 类型
            vec4 directionRange; // xyz: 方向（Spot）；w: 影响范围（Point/Spot）
            vec4 colorIntensity; // rgb: 颜色；w: 强度
            vec4 params;         // x: 内锥角余弦；y: 外锥角余弦（Spot）
        };

        layout(std140, binding = 1) uniform Lighting {
            vec4 u_Ambient;      // rgb: 颜色；a: 强度
            int u_LightCount;
            Light u_Lights[MAX_LIGHTS];
        };

        // 与 C++ 侧 ShadowData 的 std140 布局严格对应，由 Renderer::BeginShadowPass 上传
        layout(std140, binding = 2) uniform Shadow {
            mat4 u_LightSpaceMatrix;
            vec4 u_ShadowParams;  // x: 1/阴影贴图尺寸（texel 大小）；y: 深度偏移基数
        };

        uniform sampler2D u_ShadowMap;
        uniform vec4 u_Tint;

        layout(location = 0) out vec4 o_Color;

        in vec3 v_WorldPosition;
        in vec3 v_Normal;
        in vec4 v_Color;

        // 方向光阴影：世界坐标投影到光空间，与深度图比较（3x3 PCF），返回可见度（1 完全受光 / 0 全阴影）
        float ComputeVisibility(vec3 normal, vec3 lightDirection)
        {
            vec4 lightSpacePosition = u_LightSpaceMatrix * vec4(v_WorldPosition, 1.0);
            vec3 projected = lightSpacePosition.xyz / lightSpacePosition.w * 0.5 + 0.5;
            if (projected.z > 1.0) {
                return 1.0;  // 超出光源视锥远端
            }

            // 斜率缩放偏移：掠射角表面需要更大的偏移，避免自阴影条纹
            float bias = max(u_ShadowParams.y * (1.0 - dot(normal, lightDirection)), u_ShadowParams.y * 0.2);
            float texel = u_ShadowParams.x;

            float visibility = 0.0;
            for (int x = -1; x <= 1; ++x) {
                for (int y = -1; y <= 1; ++y) {
                    float shadowDepth = texture(u_ShadowMap, projected.xy + vec2(x, y) * texel).r;
                    visibility += projected.z - bias > shadowDepth ? 0.0 : 1.0;
                }
            }
            return visibility / 9.0;
        }

        void main()
        {
            vec3 normal = normalize(v_Normal);
            vec3 color = u_Ambient.rgb * u_Ambient.a;

            for (int i = 0; i < u_LightCount; ++i) {
                const int type = int(u_Lights[i].positionType.w + 0.5);

                vec3 lightDirection;  // 从着色点指向光源
                float attenuation = 1.0;

                if (type == 0) {  // 方向光：positionType.xyz 为传播方向，取反得到指向光源的方向；按方向光阴影衰减
                    lightDirection = -normalize(u_Lights[i].positionType.xyz);
                    attenuation = ComputeVisibility(normal, lightDirection);
                } else {  // 点光源 / 聚光灯
                    vec3 toLight = u_Lights[i].positionType.xyz - v_WorldPosition;
                    float distance = length(toLight);
                    lightDirection = toLight / max(distance, 0.0001);

                    float falloff = clamp(1.0 - distance / max(u_Lights[i].directionRange.w, 0.0001), 0.0, 1.0);
                    attenuation = falloff * falloff;

                    if (type == 2) {
                        float theta = dot(lightDirection, normalize(-u_Lights[i].directionRange.xyz));
                        float epsilon = max(u_Lights[i].params.x - u_Lights[i].params.y, 0.0001);
                        attenuation *= clamp((theta - u_Lights[i].params.y) / epsilon, 0.0, 1.0);
                    }
                }

                float diffuse = max(dot(normal, lightDirection), 0.0);
                color += u_Lights[i].colorIntensity.rgb * u_Lights[i].colorIntensity.w * diffuse * attenuation;
            }

            o_Color = vec4(v_Color.rgb * u_Tint.rgb * color, v_Color.a * u_Tint.a);
        }
    )";

    // 阴影 pass 的深度着色器：只做光源相机变换，不写颜色（帧缓冲只有深度附件）
    const char* DEPTH_VERTEX_SHADER = R"(
        #version 450 core

        layout(location = 0) in vec3 a_Position;

        layout(std140, binding = 0) uniform Camera {
            mat4 u_Projection;
            mat4 u_View;
            vec4 u_CameraPosition;
        };

        uniform mat4 u_Transform;

        void main()
        {
            gl_Position = u_Projection * u_View * u_Transform * vec4(a_Position, 1.0);
        }
    )";

    const char* DEPTH_FRAGMENT_SHADER = R"(
        #version 450 core

        void main()
        {
        }
    )";

    // 太阳指示球：不受光照的平涂
    const char* SUN_VERTEX_SHADER = R"(
        #version 450 core

        layout(location = 0) in vec3 a_Position;

        layout(std140, binding = 0) uniform Camera {
            mat4 u_Projection;
            mat4 u_View;
            vec4 u_CameraPosition;
        };

        uniform mat4 u_Transform;

        void main()
        {
            gl_Position = u_Projection * u_View * u_Transform * vec4(a_Position, 1.0);
        }
    )";

    const char* SUN_FRAGMENT_SHADER = R"(
        #version 450 core

        layout(location = 0) out vec4 o_Color;

        void main()
        {
            o_Color = vec4(1.0, 0.84, 0.48, 1.0);
        }
    )";
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
    m_litShader = Shader::Create("ShadowDemoLit", LIT_VERTEX_SHADER, LIT_FRAGMENT_SHADER);
    m_depthShader = Shader::Create("ShadowDemoDepth", DEPTH_VERTEX_SHADER, DEPTH_FRAGMENT_SHADER);
    m_sunShader = Shader::Create("ShadowDemoSun", SUN_VERTEX_SHADER, SUN_FRAGMENT_SHADER);
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

    // 场景布置：中心高塔 + 旋转方块 + 悬浮球 + 矮墙等，全部参与阴影投影
    const auto addInstance = [this](const glm::vec4& color, const glm::vec3& position, const glm::vec3& scale,
                                    bool sphere, float spinSpeed, float bobAmplitude, float bobPhase)
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
    };

    addInstance(glm::vec4(0.85f, 0.36f, 0.30f, 1.0f), glm::vec3( 0.0f, 2.0f,  0.0f), glm::vec3(2.0f, 4.0f, 2.0f), false, 0.0f, 0.0f, 0.0f);   // 中心高塔
    addInstance(glm::vec4(0.30f, 0.55f, 0.90f, 1.0f), glm::vec3(-4.2f, 0.9f,  1.6f), glm::vec3(1.8f, 1.8f, 1.8f), false, 0.9f, 0.0f, 0.0f);   // 旋转方块
    addInstance(glm::vec4(0.25f, 0.80f, 0.70f, 1.0f), glm::vec3( 3.6f, 2.6f,  0.4f), glm::vec3(1.6f, 1.6f, 1.6f), true,  0.0f, 0.6f, 0.0f);   // 悬浮球
    addInstance(glm::vec4(0.90f, 0.75f, 0.30f, 1.0f), glm::vec3( 2.6f, 0.6f,  3.4f), glm::vec3(1.2f, 1.2f, 1.2f), false, 1.4f, 0.0f, 0.0f);   // 小方块
    addInstance(glm::vec4(0.50f, 0.46f, 0.74f, 1.0f), glm::vec3(-0.5f, 0.5f, -5.6f), glm::vec3(9.0f, 1.0f, 0.8f), false, 0.0f, 0.0f, 0.0f);   // 矮墙
    addInstance(glm::vec4(0.90f, 0.55f, 0.25f, 1.0f), glm::vec3(-2.4f, 0.6f, -2.6f), glm::vec3(1.2f, 1.2f, 1.2f), true,  0.0f, 0.0f, 0.0f);   // 小球

    // 光照：暖色太阳方向光（每帧更新方向）+ 微弱冷色环境光
    m_scene.ambientColor = glm::vec3(0.45f, 0.52f, 0.65f);
    m_scene.ambientIntensity = m_ambientIntensity;
    m_scene.directionalLights.push_back(DirectionalLight{});

    RenderCommand::SetClearColor(glm::vec4(0.03f, 0.04f, 0.07f, 1.0f));

    // 阴影贴图固定绑定到约定纹理单元，只需设置一次
    m_litShader->Bind();
    m_litShader->SetInt("u_ShadowMap", SHADOW_TEXTURE_UNIT);

    std::cout << "ShadowDemo OnAttach" << std::endl;
}

void ShadowDemoLayer::OnDetach() {}

void ShadowDemoLayer::OnUpdate()
{
    if (!m_active) {
        return;
    }

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

    // 2. 主场景：阴影贴图绑定到约定纹理单元，光照着色器里做 PCF 采样
    Renderer::BeginScene(m_camera, m_scene);
    m_shadowMap->BindDepthTexture(SHADOW_TEXTURE_UNIT);

    // 地面（只接收阴影）
    m_litShader->Bind();
    m_litShader->SetMat4("u_Transform", glm::mat4(1.0f));
    m_litShader->SetFloat4("u_Tint", glm::vec4(0.55f, 0.58f, 0.62f, 1.0f));
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
    ImGui::Text("Sun");
    ImGui::SliderAngle("Sun yaw", &m_sunYaw, 0.0f, 360.0f);
    ImGui::SliderAngle("Sun pitch", &m_sunPitch, 10.0f, 85.0f);
    ImGui::SliderFloat("Sun intensity", &m_sunIntensity, 0.0f, 3.0f, "%.2f");
    ImGui::SliderFloat("Ambient", &m_ambientIntensity, 0.0f, 0.5f, "%.3f");

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
    ImGui::TextDisabled("F1 飞机大战  F2 太阳系  F3 平台跳跃  F5 阴影演示");
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
    m_litShader->SetFloat4("u_Tint", instance.color);
    RenderCommand::DrawIndexed(instance.mesh);
}
