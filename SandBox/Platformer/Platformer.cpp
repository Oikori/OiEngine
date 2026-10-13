#include "Platformer.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <random>
#include <vector>

#include <imgui.h>

#include "RenderHelpers.h"
#include "Engine/Core/Input.h"
#include "Engine/Events/KeyEvent.h"

using namespace OiEngine;
using namespace OiEngine::SandBox;

namespace {

    constexpr float PI = 3.14159265358979323846f;

    // ---- 玩家碰撞体（世界单位 = 像素，与 PlaneGame 同一约定）----
    constexpr float PLAYER_WIDTH = 40.0f;
    constexpr float PLAYER_HEIGHT = 56.0f;
    constexpr float PLAYER_HALF_WIDTH = PLAYER_WIDTH * 0.5f;
    constexpr float PLAYER_HALF_HEIGHT = PLAYER_HEIGHT * 0.5f;

    constexpr float COIN_RADIUS = 13.0f;
    constexpr float FLAG_HEIGHT = 150.0f;

    // ---- 世界与视图 ----
    constexpr float WORLD_MIN_X = -490.0f;
    constexpr float WORLD_MAX_X = 3380.0f;
    constexpr float WORLD_KILL_Y = -620.0f;        // 摔到该高度以下即整关重置
    constexpr float VIEW_HALF_HEIGHT = 450.0f;     // 正交半高（世界单位与窗口像素 1:1）
    constexpr float CAMERA_LOOKAHEAD = 70.0f;      // 相机朝移动方向的前瞻量
    constexpr float CLOUD_LOOP = 2600.0f;          // 云层视差图案的循环宽度

    // ---- 配色 ----
    const glm::vec4 SKY_TOP(0.30f, 0.56f, 0.86f, 1.0f);
    const glm::vec4 SKY_BOTTOM(0.80f, 0.91f, 0.97f, 1.0f);
    const glm::vec4 GRASS_BODY(0.45f, 0.32f, 0.24f, 1.0f);
    const glm::vec4 GRASS_TOP(0.46f, 0.79f, 0.39f, 1.0f);
    const glm::vec4 STONE_BODY(0.36f, 0.40f, 0.47f, 1.0f);
    const glm::vec4 STONE_TOP(0.62f, 0.68f, 0.76f, 1.0f);
    const glm::vec4 PLAYER_BODY(0.95f, 0.60f, 0.29f, 1.0f);
    const glm::vec4 PLAYER_BELLY(1.00f, 0.80f, 0.48f, 1.0f);
    const glm::vec4 EYE_WHITE(1.00f, 1.00f, 1.00f, 1.0f);
    const glm::vec4 EYE_PUPIL(0.22f, 0.16f, 0.14f, 1.0f);
    const glm::vec4 COIN_GOLD(1.00f, 0.82f, 0.30f, 1.0f);
    const glm::vec4 COIN_CORE(1.00f, 0.95f, 0.68f, 1.0f);
    const glm::vec4 FLAG_RED(0.92f, 0.30f, 0.36f, 1.0f);
    const glm::vec4 FLAG_POLE(0.58f, 0.62f, 0.70f, 1.0f);
    const glm::vec4 DUST(0.96f, 0.97f, 1.00f, 0.80f);

    std::mt19937 g_rng(0x5EED1234u);

    float RandomFloat(float min, float max)
    {
        return std::uniform_real_distribution<float>(min, max)(g_rng);
    }

    // ---------- 程序化几何（PlaneGame 同款顶点格式：Float3 位置 + Float4 颜色）----------

    struct MeshBuilder {
        std::vector<float> vertices;
        std::vector<uint32_t> indices;

        uint32_t AddVertex(const glm::vec2& p, float z, const glm::vec4& color)
        {
            vertices.push_back(p.x);
            vertices.push_back(p.y);
            vertices.push_back(z);
            vertices.push_back(color.x);
            vertices.push_back(color.y);
            vertices.push_back(color.z);
            vertices.push_back(color.w);
            return static_cast<uint32_t>(vertices.size() / 7 - 1);
        }

        void AddTriangle(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c,
                         const glm::vec4& color, float z = 0.0f)
        {
            const uint32_t base = AddVertex(a, z, color);
            AddVertex(b, z, color);
            AddVertex(c, z, color);
            indices.push_back(base);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
        }

        // 凸多边形：以首顶点为扇心三角化
        void AddPolygon(const std::vector<glm::vec2>& points, const glm::vec4& color, float z = 0.0f)
        {
            if (points.size() < 3) {
                return;
            }
            const uint32_t base = static_cast<uint32_t>(vertices.size() / 7);
            for (const auto& point : points) {
                AddVertex(point, z, color);
            }
            for (size_t i = 1; i + 1 < points.size(); ++i) {
                indices.push_back(base);
                indices.push_back(base + static_cast<uint32_t>(i));
                indices.push_back(base + static_cast<uint32_t>(i + 1));
            }
        }

        void AddPolygon(std::initializer_list<glm::vec2> points, const glm::vec4& color, float z = 0.0f)
        {
            AddPolygon(std::vector<glm::vec2>(points), color, z);
        }
    };

    Ref<VertexArray> BuildMesh(MeshBuilder& builder)
    {
        Ref<VertexArray> vertexArray = VertexArray::Create();

        Ref<VertexBuffer> vertexBuffer = VertexBuffer::Create(
            builder.vertices.data(), static_cast<uint32_t>(builder.vertices.size() * sizeof(float)));
        vertexBuffer->SetLayout({
            { ShaderDataType::Float3, "a_Position" },
            { ShaderDataType::Float4, "a_Color" },
        });
        vertexArray->AddVertexBuffer(vertexBuffer);

        Ref<IndexBuffer> indexBuffer = IndexBuffer::Create(builder.indices.data(),
                                                           static_cast<uint32_t>(builder.indices.size()));
        vertexArray->SetIndexBuffer(indexBuffer);
        return vertexArray;
    }

    void AddCircle(MeshBuilder& builder, const glm::vec2& center, float radius,
                   const glm::vec4& color, float z, int segments = 18)
    {
        std::vector<glm::vec2> points;
        points.reserve(static_cast<size_t>(segments));
        for (int i = 0; i < segments; ++i) {
            const float angle = 2.0f * PI * static_cast<float>(i) / static_cast<float>(segments);
            points.push_back(center + glm::vec2(std::cos(angle), std::sin(angle)) * radius);
        }
        builder.AddPolygon(points, color, z);
    }

    // 圆角矩形（玩家身体）；逆时针环绕，供多边形三角化
    void AddRoundedBox(MeshBuilder& builder, const glm::vec2& center, float halfWidth, float halfHeight,
                       float cornerRadius, const glm::vec4& color, float z)
    {
        constexpr int CORNER_SEGMENTS = 5;
        const float innerX = halfWidth - cornerRadius;
        const float innerY = halfHeight - cornerRadius;
        const glm::vec2 innerCorners[4] = {
            { innerX, innerY }, { -innerX, innerY }, { -innerX, -innerY }, { innerX, -innerY },
        };
        const float startAngles[4] = { 0.0f, PI * 0.5f, PI, PI * 1.5f };

        std::vector<glm::vec2> outline;
        outline.reserve(4 * (CORNER_SEGMENTS + 1));
        for (int corner = 0; corner < 4; ++corner) {
            for (int i = 0; i <= CORNER_SEGMENTS; ++i) {
                const float angle = startAngles[corner] + PI * 0.5f * static_cast<float>(i) / CORNER_SEGMENTS;
                outline.push_back(center + innerCorners[corner] +
                                  glm::vec2(std::cos(angle), std::sin(angle)) * cornerRadius);
            }
        }
        builder.AddPolygon(outline, color, z);
    }

    // ---------- 网格资源 ----------

    Ref<VertexArray> CreateSolidMesh()
    {
        // 单位矩形：宽 1 高 1、中心在原点，便于用 Scale 复用（平台 / 旗杆 / 旗顶）
        MeshBuilder builder;
        const glm::vec4 white(1.0f, 1.0f, 1.0f, 1.0f);
        const uint32_t base = builder.AddVertex(glm::vec2(-0.5f, 0.5f), 0.0f, white);
        builder.AddVertex(glm::vec2(0.5f, 0.5f), 0.0f, white);
        builder.AddVertex(glm::vec2(-0.5f, -0.5f), 0.0f, white);
        builder.AddVertex(glm::vec2(0.5f, -0.5f), 0.0f, white);
        builder.indices.insert(builder.indices.end(), { base + 0, base + 2, base + 1, base + 1, base + 2, base + 3 });
        return BuildMesh(builder);
    }

    Ref<VertexArray> CreateSkyMesh()
    {
        // 天空渐变：顶点色固定为顶部深蓝 → 底部浅蓝
        MeshBuilder builder;
        const uint32_t base = builder.AddVertex(glm::vec2(-0.5f, 0.5f), 0.0f, SKY_TOP);
        builder.AddVertex(glm::vec2(0.5f, 0.5f), 0.0f, SKY_TOP);
        builder.AddVertex(glm::vec2(-0.5f, -0.5f), 0.0f, SKY_BOTTOM);
        builder.AddVertex(glm::vec2(0.5f, -0.5f), 0.0f, SKY_BOTTOM);
        builder.indices.insert(builder.indices.end(), { base + 0, base + 2, base + 1, base + 1, base + 2, base + 3 });
        return BuildMesh(builder);
    }

    // 径向光晕（中心亮、边缘透明）；着色器以局部坐标长度作衰减，故模型半径即 1.0
    Ref<VertexArray> CreateGlowMesh()
    {
        MeshBuilder builder;
        AddCircle(builder, glm::vec2(0.0f, 0.0f), 1.0f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 0.0f, 24);
        return BuildMesh(builder);
    }

    Ref<VertexArray> CreateCoinMesh()
    {
        // 菱形金币（外径 0.5）+ 内部亮芯
        MeshBuilder builder;
        builder.AddPolygon({ { 0.0f, 0.5f }, { 0.5f, 0.0f }, { 0.0f, -0.5f }, { -0.5f, 0.0f } }, COIN_GOLD, 0.0f);
        builder.AddPolygon({ { 0.0f, 0.24f }, { 0.24f, 0.0f }, { 0.0f, -0.24f }, { -0.24f, 0.0f } }, COIN_CORE, 0.02f);
        return BuildMesh(builder);
    }

    Ref<VertexArray> CreatePlayerMesh()
    {
        // 局部坐标：宽 1（±0.5）、高 1.4（±0.7），绘制时按 40x56 缩放；
        // 瞳孔统一向 +X 偏移，缩放翻转后自然朝向移动方向
        MeshBuilder builder;
        AddRoundedBox(builder, glm::vec2(0.0f, 0.0f), 0.5f, 0.7f, 0.28f, PLAYER_BODY, 0.0f);
        AddRoundedBox(builder, glm::vec2(0.0f, -0.30f), 0.28f, 0.15f, 0.13f, PLAYER_BELLY, 0.01f);
        AddCircle(builder, glm::vec2(-0.17f, 0.30f), 0.115f, EYE_WHITE, 0.02f);
        AddCircle(builder, glm::vec2(0.17f, 0.30f), 0.115f, EYE_WHITE, 0.02f);
        AddCircle(builder, glm::vec2(-0.15f, 0.30f), 0.052f, EYE_PUPIL, 0.03f);
        AddCircle(builder, glm::vec2(0.20f, 0.30f), 0.052f, EYE_PUPIL, 0.03f);
        return BuildMesh(builder);
    }

    Ref<VertexArray> CreateFlagMesh()
    {
        // 三角旗面：附着点在左侧中点，向右展开
        MeshBuilder builder;
        builder.AddPolygon({ { 0.0f, 0.5f }, { 0.9f, 0.25f }, { 0.0f, 0.0f } }, FLAG_RED, 0.0f);
        return BuildMesh(builder);
    }
}

// ---------- PlatformerCamera ----------

glm::vec3 PlatformerCamera::GetPosition() const
{
    return glm::vec3(m_center.x, m_center.y, 10.0f);
}

glm::mat4 PlatformerCamera::GetViewMatrix() const
{
    // 相机中心平移至原点（相机效果应体现在这里返回的视图矩阵中，随 UBO 上传）
    return Translate(glm::vec3(-m_center.x, -m_center.y, 0.0f));
}

glm::mat4 PlatformerCamera::GetProjectionMatrix() const
{
    const uint32_t width = Application::Get().GetWindow().GetWidth();
    const uint32_t height = Application::Get().GetWindow().GetHeight();
    const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 16.0f / 9.0f;
    const float halfWidth = VIEW_HALF_HEIGHT * aspect;
    return Orthographic(-halfWidth, halfWidth, -VIEW_HALF_HEIGHT, VIEW_HALF_HEIGHT, -10.0f, 10.0f);
}

// ---------- PlatformerLayer ----------

void PlatformerLayer::OnAttach()
{
    CreateShaders();
    CreateMeshes();
    BuildLevel();
    ResetLevel();

    RenderCommand::SetClearColor(glm::vec4(0.62f, 0.78f, 0.92f, 1.0f));

    OI_INFO("Platformer OnAttach");
}

void PlatformerLayer::OnDetach() {}

void PlatformerLayer::CreateShaders()
{
    const char* VERTEX_SOURCE = R"(
        #version 450 core

        layout(location = 0) in vec3 a_Position;
        layout(location = 1) in vec4 a_Color;

        layout(std140, binding = 0) uniform Camera {
            mat4 u_Projection;
            mat4 u_View;
            vec4 u_CameraPosition;  // xyz: 世界空间位置（本着色器未用）
        };

        uniform mat4 u_Transform;

        out vec4 v_Color;
        out vec2 v_Local;

        void main()
        {
            gl_Position = u_Projection * u_View * u_Transform * vec4(a_Position, 1.0);
            v_Color = a_Color;
            v_Local = a_Position.xy;  // 局部坐标，用于径向渐变
        }
    )";

    const char* FRAGMENT_SOURCE = R"(
        #version 450 core

        layout(location = 0) out vec4 o_Color;

        in vec4 v_Color;
        in vec2 v_Local;

        uniform vec4 u_Tint;
        uniform float u_Falloff;  // 0 = 保留顶点色，> 0 = 径向衰减系数（光晕/粒子）

        void main()
        {
            vec4 color = v_Color * u_Tint;
            if (u_Falloff > 0.0) {
                float distance = length(v_Local);
                color.a *= max(0.0, 1.0 - distance) * u_Falloff;
            }
            if (color.a <= 0.001) {
                discard;
            }
            o_Color = color;
        }
    )";

    m_shader = Shader::Create("Platformer", VERTEX_SOURCE, FRAGMENT_SOURCE);
}

void PlatformerLayer::CreateMeshes()
{
    m_solidMesh = CreateSolidMesh();
    m_skyMesh = CreateSkyMesh();
    m_glowMesh = CreateGlowMesh();
    m_coinMesh = CreateCoinMesh();
    m_playerMesh = CreatePlayerMesh();
    m_flagMesh = CreateFlagMesh();
}

void PlatformerLayer::BuildLevel()
{
    m_platforms.clear();
    m_coins.clear();
    m_clouds.clear();

    // 左端挡墙（防止跑出关卡）；顶部略低于跳跃高度上限，避免玩家翻过去
    AddBlock(-470.0f, -100.0f, 20.0f, 150.0f, false);

    // 地面与缺口（掉进缺口即坠落重置）
    AddGround(-125.0f, 375.0f, -400.0f);   // x ∈ [-500, 250]
    AddGround(630.0f, 210.0f, -400.0f);    // x ∈ [420, 840]

    // 阶梯上行（每级抬升 90~120，最高跳高约 164，需按住跳键）
    AddBlock(930.0f, -280.0f, 75.0f, 60.0f, true);
    AddBlock(1130.0f, -190.0f, 60.0f, 60.0f, true);
    AddBlock(1330.0f, -90.0f, 55.0f, 60.0f, true);
    AddBlock(1590.0f, -40.0f, 110.0f, 60.0f, true);
    // 起伏段（含一块窄台，考验落点控制）
    AddBlock(1880.0f, -140.0f, 70.0f, 60.0f, true);
    AddBlock(2130.0f, -240.0f, 65.0f, 60.0f, true);
    AddBlock(2350.0f, -240.0f, 45.0f, 60.0f, true);
    AddBlock(2560.0f, -160.0f, 60.0f, 60.0f, true);
    AddBlock(2760.0f, -80.0f, 55.0f, 60.0f, true);
    // 终点大平台
    AddBlock(3125.0f, -80.0f, 255.0f, 60.0f, true);

    // 金币：沿跳跃弧线摆放
    AddCoin(150.0f, -320.0f);
    AddCoin(930.0f, -210.0f);
    AddCoin(1130.0f, -120.0f);
    AddCoin(1330.0f, -20.0f);
    AddCoin(1590.0f, 30.0f);
    AddCoin(1880.0f, -70.0f);
    AddCoin(2130.0f, -170.0f);
    AddCoin(2350.0f, -170.0f);
    AddCoin(2760.0f, -10.0f);

    m_flagPosition = glm::vec2(3150.0f, -80.0f);
    m_spawnPoint = glm::vec2(100.0f, -400.0f + PLAYER_HALF_HEIGHT);

    // 云层：视差背景，随机分布并错开 z
    for (int i = 0; i < 12; ++i) {
        Cloud cloud;
        cloud.x = RandomFloat(0.0f, CLOUD_LOOP);
        cloud.y = RandomFloat(170.0f, 420.0f);
        cloud.scale = RandomFloat(110.0f, 240.0f);
        cloud.z = -2.75f + 0.01f * static_cast<float>(i);
        cloud.color = glm::vec4(1.0f, 1.0f, 1.0f, RandomFloat(0.22f, 0.45f));
        m_clouds.push_back(cloud);
    }
}

void PlatformerLayer::AddGround(float centerX, float halfWidth, float top)
{
    constexpr float THICKNESS = 50.0f;
    Platform platform;
    platform.position = glm::vec2(centerX, top - THICKNESS * 0.5f);
    platform.halfSize = glm::vec2(halfWidth, THICKNESS * 0.5f);
    platform.bodyColor = GRASS_BODY;
    platform.topColor = GRASS_TOP;
    m_platforms.push_back(platform);
}

void PlatformerLayer::AddBlock(float centerX, float top, float halfWidth, float halfHeight, bool grass)
{
    Platform platform;
    platform.position = glm::vec2(centerX, top - halfHeight);
    platform.halfSize = glm::vec2(halfWidth, halfHeight);
    platform.bodyColor = grass ? GRASS_BODY : STONE_BODY;
    platform.topColor = grass ? GRASS_TOP : STONE_TOP;
    m_platforms.push_back(platform);
}

void PlatformerLayer::AddCoin(float x, float y)
{
    Coin coin;
    coin.position = glm::vec2(x, y);
    coin.phase = static_cast<float>(m_coins.size()) * 0.9f;
    m_coins.push_back(coin);
}

void PlatformerLayer::ResetLevel()
{
    m_playerPosition = m_spawnPoint;
    m_velocity = glm::vec2(0.0f, 0.0f);
    m_grounded = false;
    m_wasGrounded = false;
    m_coyoteTimer = 0.0f;
    m_jumpBufferTimer = 0.0f;
    m_jumpHeld = false;
    m_jumpHoldTime = 0.0f;
    m_roseInAir = false;
    m_apexActive = false;
    m_fastFalling = false;
    m_facing = 1;
    m_squash = 1.0f;
    m_landSpeed = 0.0f;

    for (auto& coin : m_coins) {
        coin.collected = false;
    }
    m_coinsCollected = 0;
    m_time = 0.0f;
    m_state = GameState::Playing;
    m_puffs.clear();

    // 相机直接就位，避免从上一个位置平滑拉过来
    m_camera.SetCenter(glm::vec2(CameraTargetX(m_spawnPoint.x), 0.0f));
}

float PlatformerLayer::CameraTargetX(float playerX) const
{
    const uint32_t width = Application::Get().GetWindow().GetWidth();
    const uint32_t height = Application::Get().GetWindow().GetHeight();
    const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 16.0f / 9.0f;
    const float halfWidth = VIEW_HALF_HEIGHT * aspect;

    const float minX = WORLD_MIN_X + halfWidth;
    const float maxX = WORLD_MAX_X - halfWidth;
    if (minX >= maxX) {
        return (WORLD_MIN_X + WORLD_MAX_X) * 0.5f;
    }
    return glm::clamp(playerX + static_cast<float>(m_facing) * CAMERA_LOOKAHEAD, minX, maxX);
}

void PlatformerLayer::UpdatePlayer(float deltaTime)
{
    const ImGuiIO& io = ImGui::GetIO();
    // 只有"文本输入激活"时才屏蔽游戏键盘输入；仅窗口聚焦/键盘导航不算，
    // 否则打开 F4 面板后角色会不听使唤（WantCaptureKeyboard 可能为 true）
    const bool acceptInput = !io.WantTextInput && m_state == GameState::Playing;

    const bool moveLeft = acceptInput && (Input::IsKeyPressed(Key::A) || Input::IsKeyPressed(Key::Left));
    const bool moveRight = acceptInput && (Input::IsKeyPressed(Key::D) || Input::IsKeyPressed(Key::Right));
    const bool jumpDown = acceptInput &&
        (Input::IsKeyPressed(Key::Space) || Input::IsKeyPressed(Key::W) || Input::IsKeyPressed(Key::Up));
    const bool fastFallDown = acceptInput && (Input::IsKeyPressed(Key::S) || Input::IsKeyPressed(Key::Down));

    // ---- 跳跃缓冲：新按下时刷新，落地前提前按下也算数 ----
    if (jumpDown && !m_jumpDownPrev) {
        m_jumpBufferTimer = m_tuning.jumpBufferTime;
    } else {
        m_jumpBufferTimer = std::max(0.0f, m_jumpBufferTimer - deltaTime);
    }
    m_jumpDownPrev = jumpDown;

    // ---- 起跳（地面或 Coyote 宽限期内）----
    if (m_jumpBufferTimer > 0.0f && (m_grounded || m_coyoteTimer > 0.0f)) {
        m_velocity.y = m_tuning.jumpVelocity;
        m_grounded = false;
        m_coyoteTimer = 0.0f;
        m_jumpBufferTimer = 0.0f;
        m_jumpHeld = true;
        m_jumpHoldTime = 0.0f;
        m_roseInAir = false;
        m_apexActive = false;
        m_squash = 1.10f;
        SpawnPuff(m_playerPosition + glm::vec2(0.0f, -PLAYER_HALF_HEIGHT), 5, DUST, 95.0f, 15.0f, 420.0f);
    }

    // ---- 3) 可变跳跃高度：按住跳跃键的时长决定高度，松开瞬间削减上升速度 ----
    if (m_jumpHeld) {
        if (jumpDown) {
            m_jumpHoldTime += deltaTime;
        } else {
            m_jumpHeld = false;
            if (m_velocity.y > m_tuning.jumpCutMinVelocity) {
                m_velocity.y *= m_tuning.jumpCutScale;
            }
        }
    }

    // ---- 水平移动：地面响应更灵敏，空中控制稍弱 ----
    const float moveInput = (moveRight ? 1.0f : 0.0f) - (moveLeft ? 1.0f : 0.0f);
    if (moveInput != 0.0f) {
        const float accel = m_grounded ? m_tuning.groundAccel : m_tuning.airAccel;
        m_velocity.x = glm::clamp(m_velocity.x + moveInput * accel * deltaTime,
                                  -m_tuning.maxRunSpeed, m_tuning.maxRunSpeed);
        m_facing = moveInput > 0.0f ? 1 : -1;
    } else {
        const float drag = (m_grounded ? m_tuning.groundFriction : m_tuning.airDrag) * deltaTime;
        if (std::abs(m_velocity.x) <= drag) {
            m_velocity.x = 0.0f;
        } else {
            m_velocity.x -= m_velocity.x > 0.0f ? drag : -drag;
        }
    }

    // ---- 2) 顶点滞空：靠近最高点时重力临时减弱 ----
    const bool fallingPhase = m_velocity.y <= 0.0f;
    m_fastFalling = !m_grounded && fastFallDown && fallingPhase;

    if (m_fastFalling) {
        m_apexActive = false;   // 主动快速下坠时不享受滞空
    } else if (m_apexActive) {
        if (std::abs(m_velocity.y) > m_tuning.apexBand * m_tuning.apexExitScale) {
            m_apexActive = false;
        }
    } else if (m_roseInAir && std::abs(m_velocity.y) <= m_tuning.apexBand) {
        // 只有"跳起后由上升滑过顶点"才生效；走出平台边缘自然落下不算
        m_apexActive = true;
    }

    // ---- 重力合成：1) 上升/下落非对称 → 2) 滞空带减弱 → 快速下坠叠加 ----
    float gravity = fallingPhase ? m_tuning.fallGravity : m_tuning.riseGravity;
    if (m_apexActive) {
        gravity = m_tuning.riseGravity * m_tuning.apexGravityScale;
    }
    if (m_fastFalling) {
        gravity += m_tuning.fastFallExtra;
    }

    m_velocity.y -= gravity * deltaTime;
    m_velocity.y = std::max(m_velocity.y, -m_tuning.maxFallSpeed);

    if (m_velocity.y > m_tuning.apexBand) {
        m_roseInAir = true;   // 记录明显上升过（获得顶点滞空资格）
    }

    // ---- 移动与碰撞（m_grounded 由碰撞解算负责重置）----
    m_grounded = false;
    MoveAndCollide(glm::vec2(m_velocity.x * deltaTime, m_velocity.y * deltaTime));

    // ---- 落地 / 离地处理 ----
    if (m_grounded) {
        if (!m_wasGrounded && m_landSpeed > 180.0f) {
            const float impact = glm::clamp(m_landSpeed / 2400.0f, 0.0f, 1.0f);
            m_squash = 1.0f - 0.18f * impact;
            SpawnPuff(m_playerPosition + glm::vec2(0.0f, -PLAYER_HALF_HEIGHT),
                      4 + static_cast<int>(impact * 5.0f), DUST, 80.0f + 120.0f * impact, 13.0f, 500.0f);
        }
        m_coyoteTimer = m_tuning.coyoteTime;
        m_apexActive = false;
        m_roseInAir = false;
    } else {
        m_coyoteTimer = std::max(0.0f, m_coyoteTimer - deltaTime);
    }
    m_wasGrounded = m_grounded;
    m_landSpeed = 0.0f;

    // ---- 世界边界与坠落重置 ----
    m_playerPosition.x = glm::clamp(m_playerPosition.x,
                                    WORLD_MIN_X + PLAYER_HALF_WIDTH + 4.0f,
                                    WORLD_MAX_X - PLAYER_HALF_WIDTH - 4.0f);
    if (m_playerPosition.y < WORLD_KILL_Y) {
        ++m_falls;
        ResetLevel();
        return;
    }

    if (m_state != GameState::Playing) {
        return;
    }

    // ---- 金币收集 ----
    for (auto& coin : m_coins) {
        if (coin.collected) {
            continue;
        }
        if (std::abs(coin.position.x - m_playerPosition.x) < PLAYER_HALF_WIDTH + COIN_RADIUS &&
            std::abs(coin.position.y - m_playerPosition.y) < PLAYER_HALF_HEIGHT + COIN_RADIUS) {
            coin.collected = true;
            ++m_coinsCollected;
            SpawnPuff(coin.position, 9, glm::vec4(1.0f, 0.87f, 0.42f, 1.0f), 150.0f, 12.0f, -60.0f);
        }
    }

    // ---- 终点旗 ----
    const glm::vec2 flagCenter = m_flagPosition + glm::vec2(0.0f, FLAG_HEIGHT * 0.5f);
    if (std::abs(flagCenter.x - m_playerPosition.x) < PLAYER_HALF_WIDTH + 26.0f &&
        std::abs(flagCenter.y - m_playerPosition.y) < PLAYER_HALF_HEIGHT + FLAG_HEIGHT * 0.5f) {
        m_state = GameState::Victory;
        SpawnPuff(flagCenter, 16, glm::vec4(1.0f, 0.87f, 0.42f, 1.0f), 220.0f, 16.0f, -80.0f);
    }
}

bool PlatformerLayer::OverlapsPlatform(const Platform& platform) const
{
    return std::abs(m_playerPosition.x - platform.position.x) < (PLAYER_HALF_WIDTH + platform.halfSize.x) &&
           std::abs(m_playerPosition.y - platform.position.y) < (PLAYER_HALF_HEIGHT + platform.halfSize.y);
}

void PlatformerLayer::MoveAndCollide(const glm::vec2& delta)
{
    // 子步进：单步位移不超过 8px，避免高速下落穿过薄平台
    constexpr float MAX_STEP = 8.0f;
    const float distance = glm::length(delta);
    const int steps = std::max(1, static_cast<int>(std::ceil(distance / MAX_STEP)));
    const glm::vec2 stepDelta = delta / static_cast<float>(steps);

    for (int step = 0; step < steps; ++step) {
        // X 轴：撞到侧面时推回并清零水平速度
        if (stepDelta.x != 0.0f) {
            m_playerPosition.x += stepDelta.x;
            for (const auto& platform : m_platforms) {
                if (!OverlapsPlatform(platform)) {
                    continue;
                }
                m_playerPosition.x = stepDelta.x > 0.0f
                    ? platform.position.x - platform.halfSize.x - PLAYER_HALF_WIDTH
                    : platform.position.x + platform.halfSize.x + PLAYER_HALF_WIDTH;
                m_velocity.x = 0.0f;
            }
        }

        // Y 轴：向下落在顶面（置 grounded），向上撞到底面
        if (stepDelta.y != 0.0f) {
            m_playerPosition.y += stepDelta.y;
            for (const auto& platform : m_platforms) {
                if (!OverlapsPlatform(platform)) {
                    continue;
                }
                if (stepDelta.y > 0.0f) {
                    m_playerPosition.y = platform.position.y - platform.halfSize.y - PLAYER_HALF_HEIGHT;
                } else {
                    m_playerPosition.y = platform.position.y + platform.halfSize.y + PLAYER_HALF_HEIGHT;
                    m_landSpeed = std::max(m_landSpeed, -m_velocity.y);
                    m_grounded = true;
                }
                m_velocity.y = 0.0f;
            }
        }
    }
}

void PlatformerLayer::SpawnPuff(const glm::vec2& position, int count, const glm::vec4& color,
                                float speed, float size, float gravity)
{
    for (int i = 0; i < count; ++i) {
        Puff puff;
        puff.position = position + glm::vec2(RandomFloat(-6.0f, 6.0f), RandomFloat(-4.0f, 6.0f));
        const float angle = RandomFloat(0.0f, 2.0f * PI);
        puff.velocity = glm::vec2(std::cos(angle), std::sin(angle)) * speed * RandomFloat(0.35f, 1.0f);
        puff.gravity = gravity;
        puff.lifetime = RandomFloat(0.30f, 0.62f);
        puff.size = size * RandomFloat(0.7f, 1.3f);
        puff.color = color;
        m_puffs.push_back(puff);
    }
}

void PlatformerLayer::UpdateParticles(float deltaTime)
{
    for (auto& puff : m_puffs) {
        puff.age += deltaTime;
        puff.velocity.y -= puff.gravity * deltaTime;
        puff.velocity *= std::max(0.0f, 1.0f - 3.2f * deltaTime);   // 空气阻尼
        puff.position += puff.velocity * deltaTime;
    }
    m_puffs.erase(std::remove_if(m_puffs.begin(), m_puffs.end(),
                                 [](const Puff& puff) { return puff.age >= puff.lifetime; }),
                  m_puffs.end());
}

void PlatformerLayer::UpdateCamera(float deltaTime)
{
    const float targetX = CameraTargetX(m_playerPosition.x);

    glm::vec2 center = m_camera.GetCenter();
    center.x += (targetX - center.x) * std::min(1.0f, deltaTime * 7.0f);
    center.y = 0.0f;
    m_camera.SetCenter(center);
}

void PlatformerLayer::OnUpdate()
{
    if (!m_active) {
        return;
    }

    const float deltaTime = std::min(Timer::GetDeltaTime(), 0.05f);

    RenderCommand::Clear();

    if (m_state == GameState::Playing) {
        m_time += deltaTime;
    }
    UpdatePlayer(deltaTime);

    // 挤压拉伸回弹（落地压扁 / 起跳拉伸后恢复）
    m_squash += (1.0f - m_squash) * std::min(1.0f, deltaTime * 11.0f);

    UpdateParticles(deltaTime);
    UpdateCamera(deltaTime);

    m_cloudDrift += deltaTime * 10.0f;

    Renderer::BeginScene(m_camera);
    RenderWorld();
    Renderer::EndScene();
}

void PlatformerLayer::DrawMesh(const Ref<VertexArray>& mesh, const glm::mat4& transform, const glm::vec4& tint)
{
    m_shader->SetMat4("u_Transform", transform);
    m_shader->SetFloat4("u_Tint", tint);
    RenderCommand::DrawIndexed(mesh);
}

void PlatformerLayer::RenderWorld()
{
    // 手动渲染路径：先绑定着色器再设置 uniform（相机矩阵由 BeginScene 上传的 UBO 提供）
    m_shader->Bind();

    const glm::vec2 cameraCenter = m_camera.GetCenter();
    const uint32_t width = Application::Get().GetWindow().GetWidth();
    const uint32_t height = Application::Get().GetWindow().GetHeight();
    const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 16.0f / 9.0f;
    const float halfWidth = VIEW_HALF_HEIGHT * aspect;

    const glm::vec4 white(1.0f, 1.0f, 1.0f, 1.0f);

    // ---- 天空：全屏渐变，跟随相机 ----
    m_shader->SetFloat("u_Falloff", 0.0f);
    DrawMesh(m_skyMesh, Translate(glm::vec3(cameraCenter.x, 0.0f, -9.0f)) *
                            Scale(glm::vec3(halfWidth * 2.0f, VIEW_HALF_HEIGHT * 2.0f, 1.0f)), white);

    // ---- 云层：视差滚动（相机移动 1 单位、云层移动 0.7 单位），图案循环避免出界 ----
    m_shader->SetFloat("u_Falloff", 1.0f);
    const float scroll = m_cloudDrift + cameraCenter.x * 0.7f;
    for (const auto& cloud : m_clouds) {
        float offsetX = std::fmod(cloud.x - scroll, CLOUD_LOOP);
        if (offsetX < -CLOUD_LOOP * 0.5f) {
            offsetX += CLOUD_LOOP;
        }
        if (offsetX > CLOUD_LOOP * 0.5f) {
            offsetX -= CLOUD_LOOP;
        }
        const float x = cameraCenter.x + offsetX;

        // 一朵云由三个重叠光晕（大-中-小、错位）拼出蓬松轮廓
        DrawMesh(m_glowMesh, Translate(glm::vec3(x - cloud.scale * 0.42f, cloud.y + 6.0f, cloud.z)) *
                                 Scale(glm::vec3(cloud.scale * 0.72f, cloud.scale * 0.44f, 1.0f)), cloud.color);
        DrawMesh(m_glowMesh, Translate(glm::vec3(x + cloud.scale * 0.18f, cloud.y + 16.0f, cloud.z)) *
                                 Scale(glm::vec3(cloud.scale, cloud.scale * 0.56f, 1.0f)), cloud.color);
        DrawMesh(m_glowMesh, Translate(glm::vec3(x + cloud.scale * 0.62f, cloud.y, cloud.z)) *
                                 Scale(glm::vec3(cloud.scale * 0.6f, cloud.scale * 0.38f, 1.0f)), cloud.color);
    }
    m_shader->SetFloat("u_Falloff", 0.0f);

    // ---- 平台：主体 + 顶面高光条（z 错开，避免同 z 重叠被深度测试剔除）----
    for (const auto& platform : m_platforms) {
        DrawMesh(m_solidMesh, Translate(glm::vec3(platform.position, -0.4f)) *
                              Scale(glm::vec3(platform.halfSize * 2.0f, 1.0f)), platform.bodyColor);
        DrawMesh(m_solidMesh,
                 Translate(glm::vec3(platform.position.x, platform.position.y + platform.halfSize.y - 5.0f, -0.38f)) *
                     Scale(glm::vec3(platform.halfSize.x * 2.0f, 10.0f, 1.0f)), platform.topColor);
    }

    // ---- 终点旗 ----
    const float flagTop = m_flagPosition.y + FLAG_HEIGHT;
    DrawMesh(m_solidMesh, Translate(glm::vec3(m_flagPosition.x, m_flagPosition.y + FLAG_HEIGHT * 0.5f, -0.30f)) *
                              Scale(glm::vec3(8.0f, FLAG_HEIGHT, 1.0f)), FLAG_POLE);
    DrawMesh(m_flagMesh, Translate(glm::vec3(m_flagPosition.x + 4.0f, flagTop - 44.0f, -0.28f)) *
                             Scale(glm::vec3(64.0f, 96.0f, 1.0f)), FLAG_RED);
    DrawMesh(m_solidMesh, Translate(glm::vec3(m_flagPosition.x, flagTop, -0.28f)) *
                              Scale(glm::vec3(16.0f, 16.0f, 1.0f)), glm::vec4(1.0f, 0.84f, 0.36f, 1.0f));

    // ---- 金币：绕 Y 轴旋转感（横向压缩模拟）+ 光晕 ----
    for (const auto& coin : m_coins) {
        if (coin.collected) {
            continue;
        }
        const float spin = 0.28f + 0.72f * std::abs(std::cos(m_time * 3.2f + coin.phase));

        m_shader->SetFloat("u_Falloff", 1.0f);
        DrawMesh(m_glowMesh, Translate(glm::vec3(coin.position, -0.16f)) *
                                 Scale(glm::vec3(52.0f, 52.0f, 1.0f)),
                 glm::vec4(1.0f, 0.82f, 0.35f, 0.45f));
        m_shader->SetFloat("u_Falloff", 0.0f);

        DrawMesh(m_coinMesh, Translate(glm::vec3(coin.position, -0.15f)) *
                                 Scale(glm::vec3(COIN_RADIUS * 2.0f * spin, COIN_RADIUS * 2.0f, 1.0f)), white);
    }

    // ---- 玩家：挤压拉伸 + 朝向翻转（无背面剔除，负缩放安全）----
    const float playerScaleY = m_squash;
    const float playerScaleX = 2.0f - m_squash;
    DrawMesh(m_playerMesh,
             Translate(glm::vec3(m_playerPosition, 0.0f)) *
                 Scale(glm::vec3(static_cast<float>(m_facing) * PLAYER_WIDTH * playerScaleX,
                                 PLAYER_HEIGHT * playerScaleY, 1.0f)),
             white);

    // ---- 粒子（光晕）：z 错开到玩家之前，逐颗微偏移避免互相剔除 ----
    m_shader->SetFloat("u_Falloff", 1.0f);
    for (const auto& puff : m_puffs) {
        const float t = std::min(puff.age / puff.lifetime, 1.0f);
        const float alpha = (1.0f - t) * (1.0f - t) * puff.color.w;
        const float size = puff.size * (0.7f + 0.6f * t);
        const float z = 0.12f + 0.05f * (1.0f - t);
        DrawMesh(m_glowMesh, Translate(glm::vec3(puff.position, z)) * Scale(glm::vec3(size, size, 1.0f)),
                 glm::vec4(puff.color.x, puff.color.y, puff.color.z, alpha));
    }
    m_shader->SetFloat("u_Falloff", 0.0f);
}

void PlatformerLayer::RenderHud()
{
    // ---- 左上信息栏 ----
    ImGui::SetNextWindowPos(ImVec2(14.0f, 14.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.35f);
    if (ImGui::Begin("PlatformerHud", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav)) {
        ImGui::Text("平台跳跃");
        ImGui::Text("金币 %d / %d    用时 %.1f s    坠落 %d", m_coinsCollected,
                    static_cast<int>(m_coins.size()), m_time, m_falls);
        ImGui::TextDisabled("A/D 或 ←/→ 移动    空格/W/↑ 跳跃（按住跳更高，轻点跳得低）");
        ImGui::TextDisabled("↓/S 快速下坠    R 重开    F4 手感调参    Esc 退出");
        if (m_state == GameState::Victory) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.35f, 1.0f), "通关！用时 %.2f 秒，金币 %d/%d",
                               m_time, m_coinsCollected, static_cast<int>(m_coins.size()));
            ImGui::TextDisabled("按 R 再来一局");
        }
    }
    ImGui::End();

    // ---- F4 手感调参面板 ----
    if (!m_showDebug) {
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(14.0f, 208.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350.0f, 470.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Platformer 手感调参 (F4)")) {
        const float lightJump = m_tuning.jumpVelocity * m_tuning.jumpCutScale;
        ImGui::Text("理论跳高：轻点 %.0f px  /  满按 %.0f px",
                    lightJump * lightJump / (2.0f * m_tuning.riseGravity),
                    m_tuning.jumpVelocity * m_tuning.jumpVelocity / (2.0f * m_tuning.riseGravity));

        // 实时状态放在顶部，窗口不够高时也不会被截断
        ImGui::Text("%s | %s | %s",
                    m_grounded ? "地面" : "空中",
                    m_apexActive ? "顶点滞空" : "—",
                    m_fastFalling ? "快速下坠" : "—");
        ImGui::Text("vy %.0f px/s    跳跃按住 %.2f s（%s）", m_velocity.y, m_jumpHoldTime,
                    m_jumpHeld ? "按住中" : "已松开");
        ImGui::TextDisabled("ImGui 键盘捕获 %s / 文本输入 %s", io.WantCaptureKeyboard ? "开" : "关",
                            io.WantTextInput ? "开" : "关");
        ImGui::Separator();

        if (ImGui::CollapsingHeader("1) 加速下落", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat("上升重力", &m_tuning.riseGravity, 10.0f, 400.0f, 6000.0f, "%.0f");
            ImGui::DragFloat("下落重力", &m_tuning.fallGravity, 10.0f, 400.0f, 8000.0f, "%.0f");
            ImGui::DragFloat("快速下坠附加", &m_tuning.fastFallExtra, 10.0f, 0.0f, 6000.0f, "%.0f");
            ImGui::DragFloat("终端速度", &m_tuning.maxFallSpeed, 10.0f, 400.0f, 3000.0f, "%.0f");
        }
        if (ImGui::CollapsingHeader("2) 顶点滞空", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat("滞空速度带", &m_tuning.apexBand, 1.0f, 0.0f, 400.0f, "%.0f");
            ImGui::DragFloat("滞空重力系数", &m_tuning.apexGravityScale, 0.01f, 0.05f, 1.0f, "%.2f");
            ImGui::DragFloat("滞空退出系数", &m_tuning.apexExitScale, 0.05f, 1.0f, 4.0f, "%.2f");
        }
        if (ImGui::CollapsingHeader("3) 可变跳高", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat("起跳初速度", &m_tuning.jumpVelocity, 5.0f, 200.0f, 1400.0f, "%.0f");
            ImGui::DragFloat("松键削减", &m_tuning.jumpCutScale, 0.01f, 0.10f, 1.0f, "%.2f");
            ImGui::DragFloat("削减下限", &m_tuning.jumpCutMinVelocity, 5.0f, 0.0f, 800.0f, "%.0f");
        }
        if (ImGui::CollapsingHeader("宽容与移动")) {
            ImGui::DragFloat("Coyote 时间", &m_tuning.coyoteTime, 0.005f, 0.0f, 0.30f, "%.3f");
            ImGui::DragFloat("跳跃缓冲", &m_tuning.jumpBufferTime, 0.005f, 0.0f, 0.30f, "%.3f");
            ImGui::DragFloat("最大移速", &m_tuning.maxRunSpeed, 5.0f, 100.0f, 800.0f, "%.0f");
        }
        if (ImGui::Button("恢复默认参数")) {
            m_tuning = Tuning{};
        }
    }
    ImGui::End();
}

void PlatformerLayer::OnImGuiRender()
{
    if (!m_active) {
        return;
    }
    RenderHud();
}

void PlatformerLayer::OnEvent(Event& event)
{
    if (!m_active) {
        return;
    }

    if (event.GetEventType() != EventType::KeyPressed) {
        return;
    }

    const auto& keyEvent = static_cast<KeyPressedEvent&>(event);
    if (keyEvent.IsRepeat()) {
        return;
    }

    switch (keyEvent.GetKeyCode()) {
        case Key::R:
            ResetLevel();
            break;
        case Key::F4:
            m_showDebug = !m_showDebug;
            break;
        case Key::Escape:
            Application::Get().Close();
            break;
        default:
            break;
    }
}
