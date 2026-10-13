#include "PlaneGame.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
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

    const glm::vec4 PLAYER_FUSELAGE(0.42f, 0.52f, 0.66f, 1.0f);
    const glm::vec4 PLAYER_WING(0.30f, 0.38f, 0.50f, 1.0f);
    const glm::vec4 PLAYER_CANOPY(0.55f, 0.88f, 1.00f, 1.0f);
    const glm::vec4 PLAYER_TAIL(0.24f, 0.32f, 0.44f, 1.0f);
    const glm::vec4 PLAYER_FLAME(0.40f, 0.85f, 1.00f, 0.85f);

    const glm::vec4 MISSILE_BODY(0.80f, 0.84f, 0.92f, 1.00f);
    const glm::vec4 MISSILE_NOSE(1.00f, 0.58f, 0.26f, 1.00f);
    const glm::vec4 MISSILE_FIN(0.50f, 0.58f, 0.72f, 1.00f);
    const glm::vec4 MISSILE_CORE(1.00f, 0.92f, 0.60f, 1.00f);
    const glm::vec4 PICKUP_BASE(0.16f, 0.52f, 0.70f, 1.00f);

    const glm::vec4 ENEMY_A_BODY(0.92f, 0.28f, 0.24f, 1.0f);
    const glm::vec4 ENEMY_A_TRIM(0.62f, 0.16f, 0.16f, 1.0f);
    const glm::vec4 ENEMY_A_CORE(1.00f, 0.88f, 0.55f, 1.0f);
    const glm::vec4 ENEMY_B_BODY(0.38f, 0.85f, 0.45f, 1.0f);
    const glm::vec4 ENEMY_B_TRIM(0.20f, 0.58f, 0.30f, 1.0f);
    const glm::vec4 ENEMY_B_CORE(0.85f, 1.00f, 0.88f, 1.0f);
    const glm::vec4 ENEMY_C_BODY(0.80f, 0.35f, 0.90f, 1.0f);
    const glm::vec4 ENEMY_C_TRIM(0.52f, 0.20f, 0.66f, 1.0f);
    const glm::vec4 ENEMY_C_CORE(1.00f, 0.90f, 0.60f, 1.0f);
    const glm::vec4 ENEMY_BULLET_A(1.00f, 0.42f, 0.30f, 1.0f);
    const glm::vec4 ENEMY_BULLET_C(1.00f, 0.78f, 0.30f, 1.0f);
    const glm::vec4 BULLET_CORE(1.0f, 1.0f, 1.0f, 1.0f);

    std::mt19937 g_rng(0x9E3779B9u);

    float RandomFloat(float min, float max)
    {
        return std::uniform_real_distribution<float>(min, max)(g_rng);
    }

    int RandomInt(int min, int max)
    {
        return std::uniform_int_distribution<int>(min, max)(g_rng);
    }

    // ---------- 玩家追踪导弹参数 ----------
    constexpr float MISSILE_SPEED = 600.0f;        // 巡航速度（世界单位/秒）
    constexpr float MISSILE_TURN_RATE = 3.4f;      // 最大转向角速度（弧度/秒）
    constexpr float MISSILE_LOCK_RANGE = 460.0f;   // 索敌半径（世界单位）
    constexpr float MISSILE_LOSE_RANGE = 680.0f;   // 目标超出该距离时放弃锁定
    constexpr float MISSILE_LOCK_CONE = 0.26f;     // 索敌锥角余弦（约机头前方 ±75°）
    constexpr float MISSILE_LIFETIME = 4.5f;       // 寿命（秒），超时自毁
    constexpr float MISSILE_DURATION = 10.0f;      // 拾取补给后的导弹模式时长（秒）

    // ---------- 导弹补给参数 ----------
    constexpr float PICKUP_FALL_SPEED = 95.0f;     // 下落速度（世界单位/秒）
    constexpr float PICKUP_WOBBLE_SPEED = 42.0f;   // 左右摆动的横向速度振幅
    constexpr float PICKUP_LIFETIME = 12.0f;       // 存在时间（秒），未拾取则消失

    // 投射物朝向换算：rotation 与绘制用 RotateZ 同一约定（+Y 为机头，绕 Z 逆时针），
    // 故朝向向量为 (-sin, cos)，世界方向对应的朝向角为 atan2(-x, y)
    glm::vec2 HeadingVector(float rotation)
    {
        return glm::vec2(-std::sin(rotation), std::cos(rotation));
    }

    float HeadingAngle(const glm::vec2& direction)
    {
        return std::atan2(-direction.x, direction.y);
    }

    // ---------- 程序化几何 ----------

    struct MeshBuilder {
        std::vector<float> vertices;  // 交错存储：位置 (Float3) + 颜色 (Float4)
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

        void AddPolygon(std::initializer_list<glm::vec2> points, const glm::vec4& color, float z = 0.0f)
        {
            if (points.size() < 3) {
                return;
            }
            const glm::vec2& first = *points.begin();
            auto it = points.begin() + 1;
            const glm::vec2* prev = &*it;
            for (++it; it != points.end(); ++it) {
                AddTriangle(first, *prev, *it, color, z);
                prev = &*it;
            }
        }
    };

    // 竖直方向渐变的矩形（用于子弹与光条）：top/bottom 分别指定上下端颜色
    void AddVerticalRect(MeshBuilder& builder, float halfWidth, float halfHeight,
                         const glm::vec4& top, const glm::vec4& bottom, float z = 0.0f)
    {
        const uint32_t index = static_cast<uint32_t>(builder.vertices.size() / 7);
        builder.AddVertex(glm::vec2(-halfWidth, halfHeight), z, top);
        builder.AddVertex(glm::vec2(halfWidth, halfHeight), z, top);
        builder.AddVertex(glm::vec2(-halfWidth, -halfHeight), z, bottom);
        builder.AddVertex(glm::vec2(halfWidth, -halfHeight), z, bottom);
        builder.indices.push_back(index + 0);
        builder.indices.push_back(index + 2);
        builder.indices.push_back(index + 1);
        builder.indices.push_back(index + 1);
        builder.indices.push_back(index + 2);
        builder.indices.push_back(index + 3);
    }

    // 端点透明、中心发亮的胶囊状子弹
    void AddBulletShape(MeshBuilder& builder, float halfWidth, float halfHeight,
                        const glm::vec4& color, const glm::vec4& core)
    {
        const float tip = 0.94f;
        const glm::vec4 fadedColor(0.0f, color.y * 0.2f, color.z * 0.2f, 0.0f);
        AddVerticalRect(builder, halfWidth, halfHeight * tip, color, fadedColor);
        AddVerticalRect(builder, halfWidth * 0.34f, halfHeight * 0.86f, core, core);
    }

    // 十字星闪光（爆炸核心）
    void AddStarBurst(MeshBuilder& builder, float length, float thickness, const glm::vec4& color)
    {
        builder.AddPolygon({
            { 0.0f, length }, { thickness / length * length * 0.18f, 0.0f },
            { 0.0f, -length }, { -thickness * 0.18f, 0.0f },
        }, color);
        builder.AddTriangle({ length, 0.0f }, { 0.0f, thickness }, { 0.0f, -thickness }, color);
        builder.AddTriangle({ -length, 0.0f }, { 0.0f, -thickness }, { 0.0f, thickness }, color);
    }

    // 径向光晕（中心亮、边缘透明）；着色器以局部坐标长度作衰减，故模型半径即 1.0
    void AddRadialGlow(MeshBuilder& builder, float radius, const glm::vec4& center, float z = 0.0f)
    {
        builder.AddPolygon({
            { radius, 0.0f }, { radius * 0.7071f, radius * 0.7071f },
            { 0.0f, radius }, { -radius * 0.7071f, radius * 0.7071f },
            { -radius, 0.0f }, { -radius * 0.7071f, -radius * 0.7071f },
            { 0.0f, -radius }, { radius * 0.7071f, -radius * 0.7071f },
        }, center, z);
    }

    // 爆炸圆环：线宽由绘制时的 SetLineWidth 控制
    void AddRingShape(MeshBuilder& builder, float radius, const glm::vec4& color)
    {
        constexpr uint32_t SEGMENTS = 32;
        const uint32_t base = static_cast<uint32_t>(builder.vertices.size() / 7);
        for (uint32_t segment = 0; segment <= SEGMENTS; ++segment) {
            const float angle = 2.0f * PI * static_cast<float>(segment) / SEGMENTS;
            const float strength = 0.35f + 0.65f * std::abs(std::sin(angle * 2.0f));
            builder.AddVertex(glm::vec2(std::cos(angle) * radius, std::sin(angle) * radius), 0.0f,
                              glm::vec4(color.x * strength, color.y * strength, color.z * strength, color.w));
        }
        for (uint32_t segment = 0; segment < SEGMENTS; ++segment) {
            builder.indices.push_back(base + segment);
            builder.indices.push_back(base + segment + 1);
        }
    }

    Ref<VertexArray> BuildMesh(MeshBuilder& builder)
    {
        Ref<VertexArray> vertexArray = VertexArray::Create();

        std::vector<float> vertices = std::move(builder.vertices);
        builder.vertices.clear();

        Ref<VertexBuffer> vertexBuffer = VertexBuffer::Create(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(float)));
        vertexBuffer->SetLayout({
            { ShaderDataType::Float3, "a_Position" },
            { ShaderDataType::Float4, "a_Color" },
        });
        vertexArray->AddVertexBuffer(vertexBuffer);

        Ref<IndexBuffer> indexBuffer = IndexBuffer::Create(builder.indices.data(), static_cast<uint32_t>(builder.indices.size()));
        vertexArray->SetIndexBuffer(indexBuffer);
        return vertexArray;
    }

    // ---------- 机体轮廓 ----------

    Ref<VertexArray> CreatePlayerMesh()
    {
        MeshBuilder builder;
        // 机身（窄长六边形）
        builder.AddPolygon({
            { 0.0f, 0.50f }, { 0.08f, 0.30f }, { 0.08f, -0.30f },
            { 0.0f, -0.46f }, { -0.08f, -0.30f }, { -0.08f, 0.30f },
        }, PLAYER_FUSELAGE);
        // 左右机翼（后掠三角）
        builder.AddPolygon({ { 0.05f, 0.20f }, { 0.48f, -0.34f }, { 0.09f, -0.46f } }, PLAYER_WING);
        builder.AddPolygon({ { -0.05f, 0.20f }, { -0.09f, -0.46f }, { -0.48f, -0.34f } }, PLAYER_WING);
        // 座舱
        builder.AddPolygon({ { 0.0f, 0.32f }, { 0.05f, 0.14f }, { 0.0f, -0.08f }, { -0.05f, 0.14f } }, PLAYER_CANOPY);
        // 尾翼
        builder.AddTriangle({ 0.0f, -0.42f }, { 0.07f, -0.50f }, { -0.07f, -0.50f }, PLAYER_TAIL);
        return BuildMesh(builder);
    }

    Ref<VertexArray> CreateEnemyAMesh()
    {
        MeshBuilder builder;
        builder.AddPolygon({ { 0.0f, 0.5f }, { 0.1f, 0.1f }, { 0.0f, -0.5f }, { -0.1f, 0.1f } }, ENEMY_A_BODY);
        builder.AddPolygon({ { 0.03f, 0.12f }, { 0.44f, 0.36f }, { 0.16f, -0.2f } }, ENEMY_A_TRIM);
        builder.AddPolygon({ { -0.03f, 0.12f }, { -0.16f, -0.2f }, { -0.44f, 0.36f } }, ENEMY_A_TRIM);
        builder.AddPolygon({ { 0.0f, 0.26f }, { 0.05f, 0.02f }, { 0.0f, -0.16f }, { -0.05f, 0.02f } }, ENEMY_A_CORE);
        return BuildMesh(builder);
    }

    Ref<VertexArray> CreateEnemyBMesh()
    {
        MeshBuilder builder;
        builder.AddPolygon({ { 0.0f, 0.52f }, { 0.09f, 0.18f }, { 0.0f, -0.52f }, { -0.09f, 0.18f } }, ENEMY_B_BODY);
        builder.AddPolygon({ { 0.04f, -0.02f }, { 0.42f, -0.3f }, { 0.1f, -0.42f } }, ENEMY_B_TRIM);
        builder.AddPolygon({ { -0.04f, -0.02f }, { -0.1f, -0.42f }, { -0.42f, -0.3f } }, ENEMY_B_TRIM);
        builder.AddPolygon({ { 0.0f, 0.3f }, { 0.04f, 0.1f }, { 0.0f, -0.04f }, { -0.04f, 0.1f } }, ENEMY_B_CORE);
        return BuildMesh(builder);
    }

    Ref<VertexArray> CreateEnemyCMesh()
    {
        MeshBuilder builder;
        builder.AddPolygon({ { 0.0f, 0.55f }, { 0.19f, 0.17f }, { 0.29f, -0.28f }, { 0.0f, -0.5f },
                             { -0.29f, -0.28f }, { -0.19f, 0.17f } }, ENEMY_C_BODY);
        builder.AddPolygon({ { 0.1f, 0.0f }, { 0.52f, -0.24f }, { 0.16f, -0.4f } }, ENEMY_C_TRIM);
        builder.AddPolygon({ { -0.1f, 0.0f }, { -0.16f, -0.4f }, { -0.52f, -0.24f } }, ENEMY_C_TRIM);
        builder.AddTriangle({ 0.0f, 0.42f }, { 0.09f, 0.1f }, { -0.09f, 0.1f }, ENEMY_C_CORE);
        return BuildMesh(builder);
    }

    Ref<VertexArray> CreatePlayerBulletMesh()
    {
        MeshBuilder builder;
        AddBulletShape(builder, 0.045f, 0.14f, glm::vec4(1.0f, 0.85f, 0.30f, 1.0f), BULLET_CORE);
        return BuildMesh(builder);
    }

    // 玩家追踪导弹：细长弹体 + 弹头 + 尾翼，加一条亮芯便于在暗背景下识别
    Ref<VertexArray> CreatePlayerMissileMesh()
    {
        MeshBuilder builder;
        builder.AddPolygon({
            { 0.0f, 0.34f }, { 0.05f, 0.20f }, { 0.05f, -0.26f },
            { 0.0f, -0.33f }, { -0.05f, -0.26f }, { -0.05f, 0.20f },
        }, MISSILE_BODY);
        builder.AddPolygon({ { 0.0f, 0.47f }, { 0.05f, 0.18f }, { -0.05f, 0.18f } }, MISSILE_NOSE);
        builder.AddTriangle({ 0.04f, -0.14f }, { 0.16f, -0.34f }, { 0.05f, -0.34f }, MISSILE_FIN);
        builder.AddTriangle({ -0.04f, -0.14f }, { -0.05f, -0.34f }, { -0.16f, -0.34f }, MISSILE_FIN);
        AddVerticalRect(builder, 0.016f, 0.18f, MISSILE_CORE, MISSILE_CORE, 0.01f);  // 抬高 z 避免被弹体遮挡
        return BuildMesh(builder);
    }

    // 导弹补给：青色八角底盘 + 内部导弹剪影（图标保持正立，靠光晕脉冲提示可拾取）
    // 图标相对底盘抬高一点 z：同 z 的重叠几何会被深度测试剔除
    Ref<VertexArray> CreateMissilePickupMesh()
    {
        constexpr float DIAGONAL = 0.3535f;  // 45° 方向的半径（0.5 * sqrt(2)/2 取整）
        MeshBuilder builder;
        builder.AddPolygon({
            { 0.0f, 0.5f }, { DIAGONAL, DIAGONAL }, { 0.5f, 0.0f }, { DIAGONAL, -DIAGONAL },
            { 0.0f, -0.5f }, { -DIAGONAL, -DIAGONAL }, { -0.5f, 0.0f }, { -DIAGONAL, DIAGONAL },
        }, PICKUP_BASE);
        builder.AddPolygon({
            { 0.0f, 0.24f }, { 0.055f, 0.12f }, { 0.055f, -0.14f },
            { 0.0f, -0.20f }, { -0.055f, -0.14f }, { -0.055f, 0.12f },
        }, MISSILE_BODY, 0.02f);
        builder.AddPolygon({ { 0.0f, 0.32f }, { 0.055f, 0.08f }, { -0.055f, 0.08f } }, MISSILE_NOSE, 0.02f);
        builder.AddTriangle({ 0.045f, -0.06f }, { 0.15f, -0.24f }, { 0.055f, -0.24f }, MISSILE_FIN, 0.02f);
        builder.AddTriangle({ -0.045f, -0.06f }, { -0.055f, -0.24f }, { -0.15f, -0.24f }, MISSILE_FIN, 0.02f);
        return BuildMesh(builder);
    }

    Ref<VertexArray> CreateEnemyBulletSprite(const glm::vec4& color)
    {
        MeshBuilder builder;
        AddBulletShape(builder, 0.042f, 0.13f, color, glm::vec4(1.0f, 0.95f, 0.85f, 1.0f));
        return BuildMesh(builder);
    }

    Ref<VertexArray> CreateRingMesh()
    {
        MeshBuilder builder;
        AddRingShape(builder, 1.0f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        return BuildMesh(builder);
    }

    Ref<VertexArray> CreateBurstMesh()
    {
        MeshBuilder builder;
        AddStarBurst(builder, 1.0f, 0.36f, glm::vec4(1.0f, 0.95f, 0.75f, 1.0f));
        return BuildMesh(builder);
    }

    Ref<VertexArray> CreateGlowMesh()
    {
        MeshBuilder builder;
        AddRadialGlow(builder, 1.0f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        return BuildMesh(builder);
    }

    // 屏幕顶部/底部的渐隐条：顶点色为白色，实际颜色与透明度由 u_Tint 控制
    Ref<VertexArray> CreateVignetteMesh()
    {
        MeshBuilder builder;
        const glm::vec4 solid(1.0f, 1.0f, 1.0f, 1.0f);
        const glm::vec4 clear(1.0f, 1.0f, 1.0f, 0.0f);
        AddVerticalRect(builder, 1.0f, 1.0f, solid, clear);
        return BuildMesh(builder);
    }

    // 全屏纯色四边形（受击红晕等整屏叠加效果）：顶点色一致，避免光晕网格的八边形边缘
    Ref<VertexArray> CreateFullscreenMesh()
    {
        MeshBuilder builder;
        const glm::vec4 white(1.0f, 1.0f, 1.0f, 1.0f);
        AddVerticalRect(builder, 1.0f, 1.0f, white, white);
        return BuildMesh(builder);
    }
}

// ---------- PlaneGameCamera ----------

glm::vec3 PlaneGameCamera::GetPosition() const
{
    return glm::vec3(0.0f, 0.0f, 10.0f);
}

glm::mat4 PlaneGameCamera::GetViewMatrix() const
{
    // 震屏偏移叠加在视图矩阵上（默认单位阵）
    return Translate(m_shakeOffset);
}

glm::mat4 PlaneGameCamera::GetProjectionMatrix() const
{
    // 世界单位与窗口像素 1:1（窗口高 900px 时半高恰为 450）
    const uint32_t width = Application::Get().GetWindow().GetWidth();
    const uint32_t height = Application::Get().GetWindow().GetHeight();
    const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 16.0f / 9.0f;
    constexpr float HALF_HEIGHT = 450.0f;
    const float halfWidth = HALF_HEIGHT * aspect;
    return Orthographic(-halfWidth, halfWidth, -HALF_HEIGHT, HALF_HEIGHT, -10.0f, 10.0f);
}

// ---------- PlaneGameLayer ----------

void PlaneGameLayer::OnAttach()
{
    CreateShaders();
    CreateMeshes();
    CreateStars();

    RenderCommand::SetClearColor(glm::vec4(0.02f, 0.03f, 0.08f, 1.0f));
    ResetGame();

    OI_INFO("PlaneGame OnAttach");
}

void PlaneGameLayer::OnDetach() {}

void PlaneGameLayer::CreateShaders()
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

    m_shader = Shader::Create("PlaneGame", VERTEX_SOURCE, FRAGMENT_SOURCE);
}

void PlaneGameLayer::CreateMeshes()
{
    m_playerMesh = CreatePlayerMesh();
    m_enemyAMesh = CreateEnemyAMesh();
    m_enemyBMesh = CreateEnemyBMesh();
    m_enemyCMesh = CreateEnemyCMesh();
    m_playerBulletMesh = CreatePlayerBulletMesh();
    m_playerMissileMesh = CreatePlayerMissileMesh();
    m_pickupMesh = CreateMissilePickupMesh();
    m_enemyBulletMesh = CreateEnemyBulletSprite(ENEMY_BULLET_A);
    m_enemyBulletHeavyMesh = CreateEnemyBulletSprite(ENEMY_BULLET_C);
    m_ringMesh = CreateRingMesh();
    m_burstMesh = CreateBurstMesh();
    m_glowMesh = CreateGlowMesh();
    m_vignetteMesh = CreateVignetteMesh();
    m_fullscreenMesh = CreateFullscreenMesh();
}

Ref<VertexArray> PlaneGameLayer::CreateDynamicVertexArray(uint32_t maxVertices, uint32_t indexCount)
{
    Ref<VertexArray> vertexArray = VertexArray::Create();

    m_starVertexBuffer = VertexBuffer::Create(maxVertices * 7 * sizeof(float));
    m_starVertexBuffer->SetLayout({
        { ShaderDataType::Float3, "a_Position" },
        { ShaderDataType::Float4, "a_Color" },
    });
    vertexArray->AddVertexBuffer(m_starVertexBuffer);

    // 引擎的 RenderCommand::DrawIndexed 要求顶点数组带索引缓冲；
    // 星空用固定四边形成对索引（每颗星 6 个索引）
    std::vector<uint32_t> indices(indexCount * 6);
    for (uint32_t i = 0; i < indexCount; ++i) {
        const uint32_t base = i * 4;
        indices[i * 6 + 0] = base + 0;
        indices[i * 6 + 1] = base + 2;
        indices[i * 6 + 2] = base + 1;
        indices[i * 6 + 3] = base + 1;
        indices[i * 6 + 4] = base + 2;
        indices[i * 6 + 5] = base + 3;
    }
    vertexArray->SetIndexBuffer(IndexBuffer::Create(indices.data(), static_cast<uint32_t>(indices.size())));
    return vertexArray;
}

void PlaneGameLayer::CreateStars()
{
    constexpr uint32_t STAR_COUNT = 180;
    m_stars.reserve(STAR_COUNT);

    // 星空覆盖整个窗口（世界单位在 -900..900 x -450..450 之间，留出余量）
    for (uint32_t i = 0; i < STAR_COUNT; ++i) {
        Star star;
        star.position = glm::vec2(RandomFloat(-1100.0f, 1100.0f), RandomFloat(-450.0f, 450.0f));
        star.speed = RandomFloat(30.0f, 170.0f);
        star.size = RandomFloat(1.5f, 3.5f);
        const float brightness = RandomFloat(0.60f, 1.00f);
        star.color = glm::vec4(brightness, brightness, 0.95f + 0.05f * brightness, 1.0f);
        m_stars.push_back(star);
    }

    for (int i = 0; i < 6; ++i) {
        Nebula nebula;
        nebula.position = glm::vec2(RandomFloat(-900.0f, 900.0f), RandomFloat(-450.0f, 450.0f));
        nebula.speed = RandomFloat(6.0f, 14.0f);
        nebula.scale = RandomFloat(140.0f, 320.0f);
        const glm::vec4 palette[4] = {
            glm::vec4(0.22f, 0.30f, 0.62f, 0.10f),
            glm::vec4(0.42f, 0.20f, 0.50f, 0.10f),
            glm::vec4(0.15f, 0.45f, 0.55f, 0.09f),
            glm::vec4(0.30f, 0.25f, 0.70f, 0.09f),
        };
        nebula.color = palette[i % 4];
        m_nebulae.push_back(nebula);
    }

    m_starMesh = CreateDynamicVertexArray(STAR_COUNT * 4 + 4, STAR_COUNT);
    RebuildStarVertices();
}

void PlaneGameLayer::ResetGame()
{
    m_state = GameState::Playing;
    m_time = 0.0f;
    m_score = 0;
    m_combo = 1;
    m_comboTimer = 0.0f;
    m_spawnTimer = 1.2f;
    m_spawnInterval = 1.2f;
    m_difficulty = 1.0f;
    m_lives = 3;
    m_invulnerable = 2.0f;
    m_fireCooldown = 0.0f;
    m_shake = 0.0f;
    m_hitFlash = 0.0f;
    m_playerPosition = glm::vec2(0.0f, -m_halfHeight + 110.0f);
    m_playerVelocity = glm::vec2(0.0f, 0.0f);
    m_playerRotation = 0.0f;
    m_missileTime = 0.0f;
    m_nextEnemyId = 1;
    m_entities.clear();
    m_effects.clear();
}

void PlaneGameLayer::UpdateBackground(float deltaTime)
{
    for (auto& star : m_stars) {
        star.position.y -= star.speed * deltaTime;
        if (star.position.y < -m_halfHeight - 12.0f) {
            star.position.y = m_halfHeight + RandomFloat(0.0f, 40.0f);
            star.position.x = RandomFloat(-m_halfWidth - 40.0f, m_halfWidth + 40.0f);
        }
    }

    for (auto& nebula : m_nebulae) {
        nebula.position.y -= nebula.speed * deltaTime;
        if (nebula.position.y < -m_halfHeight - nebula.scale) {
            nebula.position.y = m_halfHeight + nebula.scale;
            nebula.position.x = RandomFloat(-m_halfWidth - 200.0f, m_halfWidth + 200.0f);
        }
    }

    RebuildStarVertices();
}

void PlaneGameLayer::RebuildStarVertices()
{
    m_starVertices.clear();
    m_starVertices.reserve(m_stars.size() * 4 * 7);

    for (const auto& star : m_stars) {
        const float h = star.size;
        const float w = star.size * 0.9f;
        const float x = star.position.x;
        const float y = star.position.y;
        const float data[28] = {
            x - w, y + h, -1.0f, star.color.x, star.color.y, star.color.z, star.color.w,
            x + w, y + h, -1.0f, star.color.x, star.color.y, star.color.z, star.color.w,
            x - w, y - h, -1.0f, star.color.x, star.color.y, star.color.z, star.color.w * 0.35f,
            x + w, y - h, -1.0f, star.color.x, star.color.y, star.color.z, star.color.w * 0.35f,
        };
        m_starVertices.insert(m_starVertices.end(), data, data + 28);
    }

    if (!m_starVertices.empty()) {
        m_starVertexBuffer->SetData(m_starVertices.data(), static_cast<uint32_t>(m_starVertices.size() * sizeof(float)));
    }
}

void PlaneGameLayer::SpawnEnemy(EntityKind kind, const glm::vec2& position)
{
    Entity enemy;
    enemy.kind = kind;
    enemy.id = m_nextEnemyId++;
    enemy.position = position;

    switch (kind) {
        case EntityKind::EnemyA:
            enemy.mesh = &m_enemyAMesh;
            enemy.radius = 42.0f;
            enemy.velocity = glm::vec2(0.0f, 160.0f * m_difficulty);
            break;
        case EntityKind::EnemyB:
            enemy.mesh = &m_enemyBMesh;
            enemy.radius = 36.0f;
            enemy.wave = RandomFloat(0.0f, 2.0f * PI);
            enemy.velocity = glm::vec2(0.0f, 205.0f * m_difficulty);
            break;
        case EntityKind::EnemyC:
            enemy.mesh = &m_enemyCMesh;
            enemy.radius = 62.0f;
            enemy.velocity = glm::vec2(0.0f, 100.0f * m_difficulty);
            break;
        default:
            break;
    }

    enemy.fireCooldown = RandomFloat(0.6f, 1.6f);
    m_entities.push_back(enemy);
}

void PlaneGameLayer::SpawnFighterShot()
{
    // 双发普通子弹：出膛位置随玩家朝向旋转，弹道基本笔直（带少量外扩）
    const float cosR = std::cos(m_playerRotation);
    const float sinR = std::sin(m_playerRotation);
    const float wingX = 0.30f * 70.0f;
    const float muzzleY = 0.30f * 70.0f;

    for (const float side : { -1.0f, 1.0f }) {
        Entity bullet;
        bullet.kind = EntityKind::PlayerBullet;
        bullet.mesh = &m_playerBulletMesh;
        bullet.radius = 10.0f;
        bullet.position = glm::vec2(
            m_playerPosition.x + side * wingX * cosR - muzzleY * sinR,
            m_playerPosition.y + side * wingX * sinR + muzzleY * cosR);
        bullet.velocity = glm::vec2(-side * wingX * 1.6f, 640.0f);
        m_entities.push_back(bullet);
    }
}

void PlaneGameLayer::SpawnFighterMissiles()
{
    // 双发从左右机翼位置射出，初始朝向与玩家机头一致（之后由导弹自行追踪）
    const float cosR = std::cos(m_playerRotation);
    const float sinR = std::sin(m_playerRotation);
    const float wingX = 0.30f * 70.0f;
    const float muzzleY = 0.30f * 70.0f;

    for (const float side : { -1.0f, 1.0f }) {
        Entity missile;
        missile.kind = EntityKind::PlayerMissile;
        missile.mesh = &m_playerMissileMesh;
        missile.radius = 12.0f;
        missile.position = glm::vec2(
            m_playerPosition.x + side * wingX * cosR - muzzleY * sinR,
            m_playerPosition.y + side * wingX * sinR + muzzleY * cosR);
        missile.rotation = m_playerRotation;
        missile.velocity = HeadingVector(m_playerRotation) * MISSILE_SPEED;
        missile.lifetime = MISSILE_LIFETIME;
        m_entities.push_back(missile);
    }
}

void PlaneGameLayer::SpawnPickup(const glm::vec2& position)
{
    // 导弹补给：在掉落点缓慢下落，未被拾取时自行消失
    Entity pickup;
    pickup.kind = EntityKind::Pickup;
    pickup.mesh = &m_pickupMesh;
    pickup.radius = 34.0f;
    pickup.position = position;
    pickup.wave = RandomFloat(0.0f, 2.0f * PI);
    pickup.lifetime = PICKUP_LIFETIME;
    m_entities.push_back(pickup);
}

void PlaneGameLayer::SpawnPickupFlash(const glm::vec2& position)
{
    Effect flash;
    flash.kind = EffectKind::Flash;
    flash.position = position;
    flash.radius = 90.0f;
    flash.lifetime = 0.25f;
    flash.color = glm::vec4(0.55f, 0.90f, 1.0f, 0.9f);
    m_effects.push_back(flash);

    Effect ring;
    ring.kind = EffectKind::Ring;
    ring.position = position;
    ring.radius = 140.0f;
    ring.lifetime = 0.4f;
    ring.color = glm::vec4(0.70f, 0.95f, 1.0f, 1.0f);
    ring.thickness = 3.0f;
    m_effects.push_back(ring);
}

void PlaneGameLayer::SpawnEnemyShot(const Entity& enemy)
{
    const glm::vec2 aim = m_playerPosition - enemy.position;
    const float distance = std::sqrt(aim.x * aim.x + aim.y * aim.y);
    const glm::vec2 direction = distance > 0.001f ? aim / distance : glm::vec2(0.0f, -1.0f);

    auto spawnBullet = [&](const Ref<VertexArray>* mesh, const glm::vec2& velocity, float radius) {
        Entity bullet;
        bullet.kind = EntityKind::EnemyBullet;
        bullet.mesh = mesh;
        bullet.radius = radius;
        bullet.position = enemy.position + direction * 30.0f;
        bullet.velocity = velocity;
        m_entities.push_back(bullet);
    };

    switch (enemy.kind) {
        case EntityKind::EnemyA:
            spawnBullet(&m_enemyBulletMesh, glm::vec2(0.0f, -250.0f), 12.0f);
            break;
        case EntityKind::EnemyB:
            spawnBullet(&m_enemyBulletMesh, direction * 290.0f, 12.0f);
            break;
        case EntityKind::EnemyC: {
            const float base = RotationTo(direction);
            for (int i = -1; i <= 1; ++i) {
                const float angle = base + static_cast<float>(i) * 0.24f;
                spawnBullet(&m_enemyBulletHeavyMesh,
                            glm::vec2(std::sin(angle), std::cos(angle)) * 240.0f, 14.0f);
            }
            break;
        }
        default:
            break;
    }
}

void PlaneGameLayer::SpawnExplosion(const glm::vec2& position, const glm::vec4& color,
                                    float radius, bool large)
{
    Effect flash;
    flash.kind = EffectKind::Flash;
    flash.position = position;
    flash.radius = large ? 150.0f : radius * 2.2f;
    flash.lifetime = large ? 0.4f : 0.2f;
    flash.color = glm::vec4(1.0f, 0.92f, 0.65f, 0.9f);
    m_effects.push_back(flash);

    Effect burst;
    burst.kind = EffectKind::Burst;
    burst.position = position;
    burst.radius = large ? 180.0f : radius * 2.6f;
    burst.lifetime = large ? 0.45f : 0.25f;
    burst.color = glm::vec4(1.0f, 0.85f, 0.45f, 1.0f);
    m_effects.push_back(burst);

    Effect ring;
    ring.kind = EffectKind::Ring;
    ring.position = position;
    ring.radius = large ? 190.0f : radius * 3.4f;
    ring.lifetime = large ? 0.6f : 0.4f;
    ring.color = color;
    ring.thickness = large ? 5.0f : 3.0f;
    m_effects.push_back(ring);

    if (large) {
        Effect secondRing;
        secondRing.kind = EffectKind::Ring;
        secondRing.position = position;
        secondRing.radius = 260.0f;
        secondRing.lifetime = 0.75f;
        secondRing.color = glm::vec4(1.0f, 0.65f, 0.35f, 1.0f);
        secondRing.thickness = 3.0f;
        m_effects.push_back(secondRing);
    }

    const int particleCount = large ? 34 : RandomInt(10, 16);
    for (int i = 0; i < particleCount; ++i) {
        Entity particle;
        particle.kind = EntityKind::Particle;
        particle.mesh = &m_glowMesh;
        particle.position = position;
        const float angle = RandomFloat(0.0f, 2.0f * PI);
        const float speed = RandomFloat(60.0f, large ? 520.0f : 320.0f);
        particle.velocity = glm::vec2(std::cos(angle), std::sin(angle)) * speed;
        particle.rotation = RandomFloat(0.0f, 2.0f * PI);
        particle.spin = RandomFloat(-6.0f, 6.0f);
        particle.scale = RandomFloat(10.0f, large ? 40.0f : 26.0f);
        particle.lifetime = RandomFloat(0.35f, large ? 1.1f : 0.7f);
        const float mix = RandomFloat(0.0f, 1.0f);
        particle.color = glm::vec4(color.x * (1.0f - mix) + 1.0f * mix,
                                       color.y * (1.0f - mix) + 0.85f * mix,
                                       color.z * (1.0f - mix) + 0.45f * mix, 1.0f);
        m_entities.push_back(particle);
    }
}

void PlaneGameLayer::SpawnPlayerDeath(const glm::vec2& position)
{
    SpawnExplosion(position, glm::vec4(0.40f, 0.85f, 1.00f, 1.0f), 80.0f, true);

    Effect ring;
    ring.kind = EffectKind::Ring;
    ring.position = position;
    ring.radius = 340.0f;
    ring.lifetime = 0.9f;
    ring.color = glm::vec4(0.85f, 0.95f, 1.0f, 1.0f);
    ring.thickness = 4.0f;
    m_effects.push_back(ring);
}

void PlaneGameLayer::KillEnemy(uint32_t index)
{
    Entity& enemy = m_entities[index];

    const glm::vec4 color = [&]() {
        switch (enemy.kind) {
            case EntityKind::EnemyA: return ENEMY_A_BODY;
            case EntityKind::EnemyB: return ENEMY_B_BODY;
            case EntityKind::EnemyC: return ENEMY_C_BODY;
            default: return glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        }
    }();

    const int points = [&]() {
        switch (enemy.kind) {
            case EntityKind::EnemyA: return 100;
            case EntityKind::EnemyB: return 200;
            case EntityKind::EnemyC: return 300;
            default: return 0;
        }
    }();

    AddScore(points * m_combo);
    m_combo = std::min(m_combo + 1, 8);
    m_comboTimer = 2.2f;

    const bool large = enemy.kind == EntityKind::EnemyC;
    SpawnExplosion(enemy.position, color, enemy.radius, large);

    // 大型敌机被击毁后掉落导弹补给
    if (large) {
        SpawnPickup(enemy.position);
    }

    m_entities[index] = m_entities.back();
    m_entities.pop_back();
}

void PlaneGameLayer::AddScore(int points)
{
    m_score += points;
    if (m_score > m_highScore) {
        m_highScore = m_score;
    }
}

bool PlaneGameLayer::IsEnemy(EntityKind kind)
{
    return kind == EntityKind::EnemyA || kind == EntityKind::EnemyB || kind == EntityKind::EnemyC;
}

void PlaneGameLayer::HitPlayer()
{
    if (m_invulnerable > 0.0f || m_state != GameState::Playing) {
        return;
    }

    --m_lives;
    m_shake = 26.0f;
    m_hitFlash = 1.0f;
    m_combo = 1;
    m_comboTimer = 0.0f;
    SpawnExplosion(m_playerPosition, PLAYER_FLAME, 50.0f, false);

    // 清除附近的敌方子弹，给玩家一点喘息空间
    for (auto it = m_entities.begin(); it != m_entities.end();) {
        if (it->kind == EntityKind::EnemyBullet && CircleHit(it->position, 90.0f, m_playerPosition, 0.0f)) {
            it = m_entities.erase(it);
        } else {
            ++it;
        }
    }

    if (m_lives <= 0) {
        SpawnPlayerDeath(m_playerPosition);
        m_shake = 60.0f;
        m_state = GameState::GameOver;
        return;
    }

    m_playerPosition = glm::vec2(0.0f, -m_halfHeight + 110.0f);
    m_playerVelocity = glm::vec2(0.0f, 0.0f);
    m_invulnerable = 2.0f;
}

void PlaneGameLayer::UpdatePlayer(float deltaTime)
{
    if (m_state != GameState::Playing) {
        return;
    }

    // WASD / 方向键移动
    const glm::vec2 facing[4] = {
        glm::vec2(0.0f, 1.0f), glm::vec2(0.0f, -1.0f),
        glm::vec2(-1.0f, 0.0f), glm::vec2(1.0f, 0.0f),
    };
    const bool keys[4] = {
        Input::IsKeyPressed(Key::W) || Input::IsKeyPressed(Key::Up),
        Input::IsKeyPressed(Key::S) || Input::IsKeyPressed(Key::Down),
        Input::IsKeyPressed(Key::A) || Input::IsKeyPressed(Key::Left),
        Input::IsKeyPressed(Key::D) || Input::IsKeyPressed(Key::Right),
    };

    glm::vec2 target(0.0f, 0.0f);
    for (int i = 0; i < 4; ++i) {
        if (keys[i]) {
            target += facing[i];
        }
    }
    const float targetLength = target.x * target.x + target.y * target.y;
    if (targetLength > 0.0001f) {
        const float length = std::sqrt(targetLength);
        constexpr float PLAYER_SPEED = 460.0f;
        target = target / length * PLAYER_SPEED;
    }

    m_playerVelocity += (target - m_playerVelocity) * std::min(deltaTime * 12.0f, 1.0f);
    m_playerPosition += m_playerVelocity * deltaTime;

    const float margin = 45.0f;
    m_playerPosition.x = std::clamp(m_playerPosition.x, -m_halfWidth + margin, m_halfWidth - margin);
    m_playerPosition.y = std::clamp(m_playerPosition.y, -m_halfHeight + margin, m_halfHeight * 0.55f);

    // 横向移动时机体倾斜
    const float targetRotation = -m_playerVelocity.x / 460.0f * 0.30f;
    m_playerRotation += (targetRotation - m_playerRotation) * std::min(deltaTime * 10.0f, 1.0f);

    // 自动连射（按住 Space 时持续开火）
    m_fireCooldown -= deltaTime;
    const bool autoFire = Input::IsKeyPressed(Key::Space) && m_time > 0.15f;
    if (autoFire && m_fireCooldown <= 0.0f) {
        if (m_missileTime > 0.0f) {
            SpawnFighterMissiles();
        } else {
            SpawnFighterShot();
        }
        m_fireCooldown = 0.12f;
    }
}

void PlaneGameLayer::UpdateEntities(float deltaTime)
{
    for (uint32_t i = 0; i < m_entities.size();) {
        Entity& entity = m_entities[i];
        entity.age += deltaTime;
        entity.rotation += entity.spin * deltaTime;

        switch (entity.kind) {
            case EntityKind::Particle: {
                entity.position += entity.velocity * deltaTime;
                entity.velocity = entity.velocity * (1.0f - std::min(deltaTime * 2.8f, 1.0f));
                if (entity.age >= entity.lifetime) {
                    m_entities[i] = m_entities.back();
                    m_entities.pop_back();
                    continue;
                }
                ++i;
                break;
            }
            case EntityKind::PlayerBullet: {
                entity.position += entity.velocity * deltaTime;
                if (entity.position.y > m_halfHeight + 40.0f) {
                    m_entities[i] = m_entities.back();
                    m_entities.pop_back();
                    continue;
                }
                ++i;
                break;
            }
            case EntityKind::PlayerMissile: {
                // 索敌：锁定失效（目标被摧毁或超出脱锁距离）后重新选择目标
                const Entity* target = nullptr;
                if (entity.targetId != 0) {
                    for (const Entity& candidate : m_entities) {
                        if (candidate.id == entity.targetId) {
                            target = &candidate;
                            break;
                        }
                    }
                    if (target != nullptr) {
                        const glm::vec2 toTarget = target->position - entity.position;
                        if (toTarget.x * toTarget.x + toTarget.y * toTarget.y >
                            MISSILE_LOSE_RANGE * MISSILE_LOSE_RANGE) {
                            target = nullptr;
                        }
                    }
                }

                const glm::vec2 heading = HeadingVector(entity.rotation);
                if (target == nullptr) {
                    // 取机头前方索敌锥角内最近的敌机
                    float bestDistanceSq = MISSILE_LOCK_RANGE * MISSILE_LOCK_RANGE;
                    for (const Entity& candidate : m_entities) {
                        if (!IsEnemy(candidate.kind)) {
                            continue;
                        }
                        const glm::vec2 toTarget = candidate.position - entity.position;
                        const float distanceSq = toTarget.x * toTarget.x + toTarget.y * toTarget.y;
                        if (distanceSq > bestDistanceSq || distanceSq < 0.0001f) {
                            continue;
                        }
                        // 只接收机头前方的目标，避免导弹出膛后立即掉头
                        const float cosAngle = (toTarget.x * heading.x + toTarget.y * heading.y) /
                                               std::sqrt(distanceSq);
                        if (cosAngle < MISSILE_LOCK_CONE) {
                            continue;
                        }
                        bestDistanceSq = distanceSq;
                        target = &candidate;
                    }
                    entity.targetId = target != nullptr ? target->id : 0;
                }

                // 以最大转向角速度朝目标偏转，再沿当前朝向推进（未锁定时保持直线飞行）
                if (target != nullptr) {
                    const glm::vec2 toTarget = target->position - entity.position;
                    const float targetAngle = HeadingAngle(toTarget);
                    const float delta = std::remainder(targetAngle - entity.rotation, 2.0f * PI);
                    const float maxStep = MISSILE_TURN_RATE * deltaTime;
                    entity.rotation += std::clamp(delta, -maxStep, maxStep);
                }
                entity.velocity = HeadingVector(entity.rotation) * MISSILE_SPEED;
                entity.position += entity.velocity * deltaTime;

                if (entity.age >= entity.lifetime ||
                    std::abs(entity.position.x) > m_halfWidth + 90.0f ||
                    std::abs(entity.position.y) > m_halfHeight + 90.0f) {
                    m_entities[i] = m_entities.back();
                    m_entities.pop_back();
                    continue;
                }
                ++i;
                break;
            }
            case EntityKind::EnemyBullet: {
                entity.position += entity.velocity * deltaTime;
                if (entity.position.y < -m_halfHeight - 40.0f) {
                    m_entities[i] = m_entities.back();
                    m_entities.pop_back();
                    continue;
                }
                ++i;
                break;
            }
            case EntityKind::Pickup: {
                // 缓慢下落并左右摆动，未被拾取时随时间消失
                entity.position.y -= PICKUP_FALL_SPEED * deltaTime;
                entity.position.x += std::sin(entity.age * 2.2f + entity.wave) * PICKUP_WOBBLE_SPEED * deltaTime;
                if (entity.age >= entity.lifetime || entity.position.y < -m_halfHeight - 60.0f) {
                    m_entities[i] = m_entities.back();
                    m_entities.pop_back();
                    continue;
                }
                ++i;
                break;
            }
            case EntityKind::EnemyA: {
                entity.position.y -= 160.0f * m_difficulty * deltaTime;
                entity.rotation = 0.0f;
                if (entity.position.y < -m_halfHeight - 70.0f) {
                    entity.position.y = m_halfHeight + 70.0f;
                    entity.position.x = RandomFloat(-m_halfWidth + 80.0f, m_halfWidth - 80.0f);
                }
                ++i;
                break;
            }
            case EntityKind::EnemyB: {
                entity.position.y -= 205.0f * m_difficulty * deltaTime;
                entity.position.x += std::sin(entity.age * 3.4f + entity.wave) * 150.0f * deltaTime;
                entity.position.x = std::clamp(entity.position.x, -m_halfWidth + 60.0f, m_halfWidth - 60.0f);
                entity.rotation = 0.35f * std::cos(entity.age * 3.4f + entity.wave);
                if (entity.position.y < -m_halfHeight - 70.0f) {
                    m_entities[i] = m_entities.back();
                    m_entities.pop_back();
                    continue;
                }
                ++i;
                break;
            }
            case EntityKind::EnemyC: {
                entity.position.y -= 100.0f * m_difficulty * deltaTime;
                // 缓慢追踪玩家横向位置
                const float dx = m_playerPosition.x - entity.position.x;
                entity.position.x += std::clamp(dx, -1.0f, 1.0f) * 60.0f * deltaTime;
                entity.rotation = std::clamp(dx / 300.0f, -1.0f, 1.0f) * 0.22f;
                if (entity.position.y < -m_halfHeight - 80.0f) {
                    m_entities[i] = m_entities.back();
                    m_entities.pop_back();
                    continue;
                }
                ++i;
                break;
            }
        }
    }
}

void PlaneGameLayer::UpdateEffects(float deltaTime)
{
    for (uint32_t i = 0; i < m_effects.size();) {
        Effect& effect = m_effects[i];
        effect.age += deltaTime;
        if (effect.age >= effect.lifetime) {
            m_effects[i] = m_effects.back();
            m_effects.pop_back();
            continue;
        }
        ++i;
    }
}

void PlaneGameLayer::UpdateSpawning(float deltaTime)
{
    if (m_state != GameState::Playing) {
        return;
    }

    m_time += deltaTime;
    m_difficulty = std::min(1.0f + m_time / 45.0f, 2.2f);

    m_spawnTimer -= deltaTime;
    if (m_spawnTimer <= 0.0f) {
        m_spawnInterval = std::max(0.55f, 1.2f - m_time * 0.01f);
        m_spawnTimer = m_spawnInterval * RandomFloat(0.75f, 1.2f);

        const float roll = RandomFloat(0.0f, 1.0f);
        float weights[3] = { std::max(0.25f, 0.75f - m_time / 120.0f), 0.30f, std::min(0.3f, m_time / 150.0f) };
        const float total = weights[0] + weights[1] + weights[2];

        // 在可玩区域内随机选择出生点
        float x = RandomFloat(-m_halfWidth + 120.0f, m_halfWidth - 120.0f);
        float y = m_halfHeight + 80.0f;
        float cursor = roll * total;
        EntityKind kind = EntityKind::EnemyA;
        for (int type = 0; type < 3; ++type) {
            cursor -= weights[type];
            if (cursor <= 0.0f) {
                kind = type == 0 ? EntityKind::EnemyA : (type == 1 ? EntityKind::EnemyB : EntityKind::EnemyC);
                break;
            }
        }

        SpawnEnemy(kind, glm::vec2(x, y));
    }
}

void PlaneGameLayer::OnUpdate()
{
    if (!m_active) {
        return;
    }

    // 窗口尺寸变化时更新可玩区域
    const uint32_t width = Application::Get().GetWindow().GetWidth();
    const uint32_t height = Application::Get().GetWindow().GetHeight();
    m_halfHeight = 450.0f;
    m_halfWidth = m_halfHeight * (height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 16.0f / 9.0f);

    const float deltaTime = std::min(Timer::GetDeltaTime(), 0.05f);
    RenderCommand::Clear();

    if (m_state != GameState::Paused) {
        UpdateBackground(deltaTime);
        UpdatePlayer(deltaTime);
        UpdateEntities(deltaTime);
        UpdateEffects(deltaTime);
        UpdateSpawning(deltaTime);

        // 敌机开火
        for (auto& entity : m_entities) {
            if (entity.kind == EntityKind::EnemyA || entity.kind == EntityKind::EnemyB || entity.kind == EntityKind::EnemyC) {
                entity.fireCooldown -= deltaTime;
                if (entity.fireCooldown <= 0.0f && entity.position.y > -m_halfHeight * 0.5f) {
                    SpawnEnemyShot(entity);
                    entity.fireCooldown = entity.kind == EntityKind::EnemyC ? 2.6f : RandomFloat(1.5f, 2.4f);
                }
            }
        }

        // 连击计时
        if (m_comboTimer > 0.0f) {
            m_comboTimer -= deltaTime;
            if (m_comboTimer <= 0.0f) {
                m_combo = 1;
            }
        }

        // 碰撞：玩家投射物（普通子弹/跟踪导弹）vs 敌机。命中后先移除投射物，避免 swap-and-pop 后索引失效
        for (uint32_t i = 0; i < m_entities.size();) {
            const EntityKind shotKind = m_entities[i].kind;
            if (shotKind != EntityKind::PlayerBullet && shotKind != EntityKind::PlayerMissile) {
                ++i;
                continue;
            }

            const glm::vec2 shotPosition = m_entities[i].position;
            const float shotRadius = m_entities[i].radius;

            uint32_t hitIndex = 0;
            bool hit = false;
            for (uint32_t j = 0; j < m_entities.size() && !hit; ++j) {
                const Entity& target = m_entities[j];
                if (IsEnemy(target.kind) && CircleHit(shotPosition, shotRadius, target.position, target.radius)) {
                    hitIndex = j;
                    hit = true;
                }
            }

            if (!hit) {
                ++i;
                continue;
            }

            // 先移除投射物（其索引先于敌机移除保持有效）
            m_entities[i] = m_entities.back();
            m_entities.pop_back();
            if (hitIndex < m_entities.size()) {
                KillEnemy(hitIndex);
            }
            // i 位置现在是换入的后备元素，下一轮循环继续处理
        }

        // 拾取导弹补给（无敌期间同样可以拾取）
        constexpr float PLAYER_RADIUS = 26.0f;
        if (m_state == GameState::Playing) {
            for (uint32_t i = 0; i < m_entities.size();) {
                const Entity& pickup = m_entities[i];
                if (pickup.kind != EntityKind::Pickup ||
                    !CircleHit(pickup.position, pickup.radius, m_playerPosition, PLAYER_RADIUS)) {
                    ++i;
                    continue;
                }

                const glm::vec2 pickupPosition = pickup.position;
                m_missileTime = MISSILE_DURATION;  // 重复拾取直接刷新时长
                SpawnPickupFlash(pickupPosition);

                m_entities[i] = m_entities.back();
                m_entities.pop_back();
            }
        }

        // 碰撞：玩家 vs 敌机 / 敌方子弹
        if (m_state == GameState::Playing && m_invulnerable <= 0.0f) {
            for (uint32_t i = 0; i < m_entities.size();) {
                const Entity& entity = m_entities[i];
                const bool enemy = IsEnemy(entity.kind);
                const bool isBullet = entity.kind == EntityKind::EnemyBullet;
                const bool collides = (enemy || isBullet) &&
                    CircleHit(entity.position, entity.radius, m_playerPosition, PLAYER_RADIUS);
                if (!collides) {
                    ++i;
                    continue;
                }

                const bool wasBullet = isBullet;
                m_entities[i] = m_entities.back();
                m_entities.pop_back();
                if (!wasBullet) {
                    // 撞上敌机：敌机也被摧毁
                    SpawnExplosion(entity.position, glm::vec4(0.9f, 0.4f, 0.3f, 1.0f), entity.radius, false);
                }
                HitPlayer();
                if (m_invulnerable > 0.0f || m_state == GameState::GameOver) {
                    break;
                }
            }
        }

        m_invulnerable = std::max(0.0f, m_invulnerable - deltaTime);
        m_missileTime = std::max(0.0f, m_missileTime - deltaTime);
        m_hitFlash = std::max(0.0f, m_hitFlash - deltaTime * 2.2f);
        m_shake = std::max(0.0f, m_shake - deltaTime * 90.0f);
    }

    // 视图偏移（受击震屏）→ 交给相机，随相机 UBO 上传
    glm::vec3 shakeOffset(0.0f, 0.0f, 0.0f);
    if (m_shake > 0.01f) {
        shakeOffset = glm::vec3(RandomFloat(-m_shake, m_shake) * 0.5f,
                                RandomFloat(-m_shake, m_shake) * 0.5f, 0.0f);
    }
    m_camera.SetShakeOffset(shakeOffset);

    Renderer::BeginScene(m_camera);
    RenderWorld();
    Renderer::EndScene();
}

void PlaneGameLayer::DrawMesh(const Ref<VertexArray>& mesh, const glm::mat4& transform,
                              const glm::vec4& tint, float lineWidth)
{
    m_shader->SetMat4("u_Transform", transform);
    m_shader->SetFloat4("u_Tint", tint);
    if (lineWidth > 0.0f) {
        RenderCommand::SetLineWidth(lineWidth);
    }
    RenderCommand::DrawIndexed(mesh);
}

void PlaneGameLayer::RenderWorld()
{
    // 手动渲染路径：先绑定着色器再设置 uniform（glUniform* 只作用于当前绑定的 program）；
    // 相机矩阵由 BeginScene 上传的 UBO 提供
    m_shader->Bind();

    // ---- 背景：星云 + 星空 ----
    m_shader->SetFloat("u_Falloff", 1.0f);
    for (const auto& nebula : m_nebulae) {
        const glm::mat4 transform = Translate(glm::vec3(nebula.position.x, nebula.position.y, -2.0f)) *
                                        Scale(glm::vec3(nebula.scale, nebula.scale, 1.0f));
        DrawMesh(m_glowMesh, transform, nebula.color);
    }

    m_shader->SetFloat("u_Falloff", 0.0f);
    DrawMesh(m_starMesh, glm::mat4(1.0f), glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    // ---- 敌机 ----
    for (const auto& entity : m_entities) {
        switch (entity.kind) {
            case EntityKind::EnemyA:
                DrawMesh(*entity.mesh, Translate(glm::vec3(entity.position.x, entity.position.y, -0.05f)) *
                                          RotateZ(entity.rotation) * Scale(glm::vec3(70.0f, 70.0f, 1.0f)));
                break;
            case EntityKind::EnemyB:
                DrawMesh(*entity.mesh, Translate(glm::vec3(entity.position.x, entity.position.y, -0.05f)) *
                                          RotateZ(entity.rotation) * Scale(glm::vec3(56.0f, 56.0f, 1.0f)));
                break;
            case EntityKind::EnemyC:
                DrawMesh(*entity.mesh, Translate(glm::vec3(entity.position.x, entity.position.y, -0.05f)) *
                                          RotateZ(entity.rotation) * Scale(glm::vec3(86.0f, 86.0f, 1.0f)));
                break;
            default:
                break;
        }
    }

    // ---- 敌方子弹：子弹网格为局部小数坐标，需要按 70 倍缩放（与机体一致）
    for (const auto& entity : m_entities) {
        if (entity.kind == EntityKind::EnemyBullet) {
            DrawMesh(*entity.mesh, Translate(glm::vec3(entity.position.x, entity.position.y, 0.02f)) *
                                   RotateZ(HeadingAngle(entity.velocity)) *
                                   Scale(glm::vec3(70.0f, 70.0f, 1.0f)));
        }
    }

    // ---- 爆炸特效（闪光 / 十字 / 圆环）----
    for (const auto& effect : m_effects) {
        if (effect.kind == EffectKind::Flash) {
            const float t = std::min(effect.age / effect.lifetime, 1.0f);
            const float alpha = (1.0f - t) * effect.color.w;
            const float expansion = 0.55f + 0.45f * t;
            const float radius = effect.radius * expansion;
            m_shader->SetFloat("u_Falloff", 1.0f);
            DrawMesh(m_glowMesh, Translate(glm::vec3(effect.position.x, effect.position.y, 0.2f)) *
                                 Scale(glm::vec3(radius, radius, 1.0f)),
                     glm::vec4(effect.color.x, effect.color.y, effect.color.z, alpha));
        } else if (effect.kind == EffectKind::Burst) {
            const float t = std::min(effect.age / effect.lifetime, 1.0f);
            const float alpha = (1.0f - t) * (1.0f - t);
            m_shader->SetFloat("u_Falloff", 0.0f);
            DrawMesh(m_burstMesh, Translate(glm::vec3(effect.position.x, effect.position.y, 0.15f)) *
                                  RotateZ(t * 1.2f) *
                                  Scale(glm::vec3(effect.radius, effect.radius, 1.0f)),
                     glm::vec4(effect.color.x, effect.color.y, effect.color.z, alpha));
        } else {
            const float t = std::min(effect.age / effect.lifetime, 1.0f);
            const float expansion = t * t * (3.0f - 2.0f * t);
            const float radius = effect.radius * (0.25f + 0.75f * expansion);
            const float alpha = (1.0f - t) * effect.color.w;
            DrawMesh(m_ringMesh, Translate(glm::vec3(effect.position.x, effect.position.y, 0.1f)) *
                                 Scale(glm::vec3(radius, radius, 1.0f)),
                     glm::vec4(effect.color.x, effect.color.y, effect.color.z, alpha),
                     effect.thickness * (1.0f - t * 0.6f));
        }
    }

    // ---- 导弹补给道具 ----
    for (const auto& entity : m_entities) {
        if (entity.kind != EntityKind::Pickup) {
            continue;
        }
        const float pulse = 0.5f + 0.5f * std::sin(entity.age * 5.0f);
        const float size = 58.0f + 5.0f * pulse;
        const float fade = std::min((entity.lifetime - entity.age) * 0.5f, 1.0f);  // 消失前 2 秒淡出
        const glm::mat4 base = Translate(glm::vec3(entity.position.x, entity.position.y, 0.04f)) *
                               Scale(glm::vec3(size, size, 1.0f));
        // 光晕放在徽章后方（z 更小为更远），否则同深度会被深度测试剔除
        m_shader->SetFloat("u_Falloff", 1.0f);
        DrawMesh(m_glowMesh, base * Translate(glm::vec3(0.0f, 0.0f, -0.01f)) *
                             Scale(glm::vec3(1.25f, 1.25f, 1.0f)),
                 glm::vec4(0.40f, 0.85f, 1.0f, (0.18f + 0.16f * pulse) * fade));
        m_shader->SetFloat("u_Falloff", 0.0f);
        DrawMesh(*entity.mesh, base, glm::vec4(1.0f, 1.0f, 1.0f, fade));
    }

    // ---- 粒子 ----
    m_shader->SetFloat("u_Falloff", 1.0f);
    for (const auto& entity : m_entities) {
        if (entity.kind != EntityKind::Particle) {
            continue;
        }
        const float t = std::min(entity.age / entity.lifetime, 1.0f);
        const float alpha = (1.0f - t) * (1.0f - t);
        const float size = entity.scale * (1.0f - t * 0.6f);
        DrawMesh(*entity.mesh, Translate(glm::vec3(entity.position.x, entity.position.y, 0.15f)) *
                               RotateZ(entity.rotation) * Scale(glm::vec3(size, size, 1.0f)),
                 glm::vec4(entity.color.x, entity.color.y, entity.color.z, alpha));
    }

    // ---- 玩家 ----
    const bool blinkHidden = m_invulnerable > 0.0f &&
                             std::fmod(m_invulnerable, 0.24f) < 0.12f && m_state != GameState::GameOver;
    if (m_state != GameState::GameOver && m_playerPosition.y < m_halfHeight && !blinkHidden) {
        // 尾焰
        const float thrust = 0.6f + 0.4f * std::sin(m_time * 30.0f);
        const float flameHeight = (44.0f + 26.0f * thrust) * (1.0f - std::min(std::sqrt(m_playerVelocity.x * m_playerVelocity.x +
                                                                                    m_playerVelocity.y * m_playerVelocity.y) / 1000.0f, 0.35f));
        m_shader->SetFloat("u_Falloff", 1.0f);
        DrawMesh(m_glowMesh, Translate(glm::vec3(m_playerPosition.x, m_playerPosition.y, -0.02f)) *
                             RotateZ(m_playerRotation) *
                             Translate(glm::vec3(0.0f, -52.0f, 0.0f)) *
                             Scale(glm::vec3(20.0f, flameHeight, 1.0f)),
                 glm::vec4(PLAYER_FLAME.x, PLAYER_FLAME.y, PLAYER_FLAME.z,
                               PLAYER_FLAME.w * (0.55f + 0.45f * thrust)));

        m_shader->SetFloat("u_Falloff", 0.0f);
        DrawMesh(m_playerMesh, Translate(glm::vec3(m_playerPosition.x, m_playerPosition.y, 0.0f)) *
                               RotateZ(m_playerRotation) * Scale(glm::vec3(70.0f, 70.0f, 1.0f)));
    }

    // ---- 玩家投射物（最上层）----
    for (const auto& entity : m_entities) {
        if (entity.kind == EntityKind::PlayerBullet) {
            DrawMesh(*entity.mesh, Translate(glm::vec3(entity.position.x, entity.position.y, 0.08f)) *
                                   RotateZ(HeadingAngle(entity.velocity)) *
                                   Scale(glm::vec3(70.0f, 70.0f, 1.0f)));
        }
    }
    for (const auto& entity : m_entities) {
        if (entity.kind != EntityKind::PlayerMissile) {
            continue;
        }
        const glm::mat4 base = Translate(glm::vec3(entity.position.x, entity.position.y, 0.08f)) *
                               RotateZ(entity.rotation);
        // 尾焰随飞行时间闪烁
        const float flicker = 0.7f + 0.3f * std::sin(entity.age * 42.0f);
        m_shader->SetFloat("u_Falloff", 1.0f);
        DrawMesh(m_glowMesh, base * Translate(glm::vec3(0.0f, -35.0f, -0.01f)) *
                             Scale(glm::vec3(7.0f, 17.0f * flicker, 1.0f)),
                 glm::vec4(1.0f, 0.62f, 0.28f, 0.7f * flicker));
        m_shader->SetFloat("u_Falloff", 0.0f);
        DrawMesh(*entity.mesh, base * Scale(glm::vec3(70.0f, 70.0f, 1.0f)));
    }

    // ---- 受击红晕 / 暗角 ----
    m_shader->SetFloat("u_Falloff", 0.0f);
    if (m_hitFlash > 0.01f) {
        DrawMesh(m_fullscreenMesh, Translate(glm::vec3(0.0f, 0.0f, 1.0f)) *
                                   Scale(glm::vec3(m_halfWidth, m_halfHeight, 1.0f)),
                 glm::vec4(1.0f, 0.25f, 0.20f, 0.30f * m_hitFlash));
    }
    DrawMesh(m_vignetteMesh, Translate(glm::vec3(0.0f, m_halfHeight - 90.0f, 1.0f)) *
                             Scale(glm::vec3(m_halfWidth, 90.0f, 1.0f)),
             glm::vec4(0.0f, 0.0f, 0.0f, 0.35f));
    DrawMesh(m_vignetteMesh, Translate(glm::vec3(0.0f, -m_halfHeight + 90.0f, 1.0f)) *
                             Scale(glm::vec3(m_halfWidth, -90.0f, 1.0f)),
             glm::vec4(0.0f, 0.0f, 0.0f, 0.35f));
}

void PlaneGameLayer::OnImGuiRender()
{
    RenderHud();
}

void PlaneGameLayer::RenderHud()
{
    if (!m_active) {
        return;
    }

    const bool overlay = m_state != GameState::Playing;
    if (overlay) {
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    } else {
        ImGui::SetNextWindowPos(ImVec2(18.0f, 14.0f));
    }
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                   (overlay ? 0 : ImGuiWindowFlags_NoInputs);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.95f, 1.0f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.0f);

    if (ImGui::Begin("HUD", nullptr, flags)) {
        ImGui::SetWindowFontScale(1.15f);
        ImGui::Text("得分 %d", m_score);
        ImGui::Text("最高 %d", m_highScore);
        if (m_combo > 1 && m_comboTimer > 0.0f) {
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.35f, 1.0f), "连击 x%d", m_combo);
        }
        ImGui::Text("生命 %d", m_lives);
        if (m_missileTime > 0.0f) {
            ImGui::TextColored(ImVec4(0.45f, 0.85f, 1.0f, 1.0f), "导弹 %.1f s", m_missileTime);
        } else {
            ImGui::Text("武器 普通子弹");
        }

        if (m_state == GameState::Paused) {
            const ImVec2 center = ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
            ImGui::SetCursorPos(ImVec2(center.x - 60.0f, center.y - 60.0f));
            ImGui::Text("游戏暂停");
            if (ImGui::Button("继续 (P)")) {
                m_state = GameState::Playing;
            }
        }

        if (m_state == GameState::GameOver) {
            const ImVec2 center = ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
            ImGui::SetCursorPos(ImVec2(center.x - 110.0f, center.y - 70.0f));
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "已被击落");
            ImGui::SetCursorPos(ImVec2(center.x - 110.0f, center.y - 40.0f));
            ImGui::Text("最终得分 %d", m_score);
            ImGui::SetCursorPos(ImVec2(center.x - 110.0f, center.y - 10.0f));
            if (ImGui::Button("重新开始 (R)", ImVec2(220.0f, 0.0f))) {
                ResetGame();
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(2);

    // 底部操作提示
    if (m_state == GameState::Playing) {
        ImGui::SetNextWindowPos(ImVec2(18.0f, ImGui::GetIO().DisplaySize.y - 46.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.82f, 0.9f, 0.75f));
        if (ImGui::Begin("Hint", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                           ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                           ImGuiWindowFlags_NoInputs)) {
            ImGui::Text("WASD / 方向键移动 · 按住空格自动开火 · P 暂停 · F1/F2/F3 切换场景 · F4 调试");
        }
        ImGui::End();
        ImGui::PopStyleColor(2);
    }

    if (m_showDebug) {
        ImGui::SetNextWindowPos(ImVec2(18.0f, 120.0f));
        if (ImGui::Begin("调试##Planegame", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
            const ImGuiIO& io = ImGui::GetIO();
            ImGui::Text("FPS %.1f (%.2f ms)", io.Framerate, io.Framerate > 0.0f ? 1000.0f / io.Framerate : 0.0f);
            ImGui::Text("状态 %s", m_state == GameState::Playing ? "游戏中" : m_state == GameState::Paused ? "暂停" : "结束");
            ImGui::Text("时间 %.1f s", m_time);
            ImGui::Text("难度 %.2f", m_difficulty);
            ImGui::Text("实体 %d", static_cast<int>(m_entities.size()));
            ImGui::Text("特效 %d", static_cast<int>(m_effects.size()));
            ImGui::Text("玩家 (%.0f, %.0f)  无敌 %.2f", m_playerPosition.x, m_playerPosition.y, m_invulnerable);
            ImGui::Separator();
            if (ImGui::Button("重新开始")) {
                ResetGame();
            }
            ImGui::SameLine();
            if (ImGui::Button("关闭")) {
                Application::Get().Close();
            }
        }
        ImGui::End();
    }
}

void PlaneGameLayer::OnEvent(Event& event)
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
        case Key::P:
            if (m_state == GameState::Playing) {
                m_state = GameState::Paused;
            } else if (m_state == GameState::Paused) {
                m_state = GameState::Playing;
            }
            break;
        case Key::R:
            ResetGame();
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
