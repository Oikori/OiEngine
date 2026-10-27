#include "Platformer.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <random>
#include <string>
#include <vector>

#include <imgui.h>

#include "RenderHelpers.h"
#include "Engine/Core/Input.h"
#include "Engine/Events/KeyEvent.h"

using namespace OiEngine;
using namespace OiEngine::Platformer;

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
    constexpr float GROUND_TOP = -400.0f;          // 起始地面顶面
    constexpr float WORLD_MIN_X = -490.0f;
    constexpr float WORLD_MAX_X = 5040.0f;
    constexpr float WORLD_KILL_Y = -620.0f;        // 摔到该高度以下即死亡
    constexpr float VIEW_HALF_HEIGHT = 450.0f;     // 正交半高（世界单位与窗口像素 1:1）
    constexpr float CAMERA_LOOKAHEAD = 70.0f;      // 相机朝移动方向的前瞻量
    constexpr float CLOUD_LOOP = 2600.0f;          // 云层视差图案的循环宽度
    constexpr float HILL_LOOP = 2200.0f;           // 远山视差图案的循环宽度

    // ---- 配色 ----
    const glm::vec4 SKY_TOP(0.30f, 0.56f, 0.86f, 1.0f);
    const glm::vec4 SKY_BOTTOM(0.80f, 0.91f, 0.97f, 1.0f);
    const glm::vec4 GRASS_BODY(0.45f, 0.32f, 0.24f, 1.0f);
    const glm::vec4 GRASS_TOP(0.46f, 0.79f, 0.39f, 1.0f);
    const glm::vec4 STONE_BODY(0.36f, 0.40f, 0.47f, 1.0f);
    const glm::vec4 STONE_TOP(0.62f, 0.68f, 0.76f, 1.0f);
    const glm::vec4 MOVER_BODY(0.46f, 0.35f, 0.25f, 1.0f);     // 移动平台：木料质感，和静态平台区分
    const glm::vec4 MOVER_TOP(0.83f, 0.68f, 0.41f, 1.0f);
    const glm::vec4 MOVER_RIVET(0.28f, 0.23f, 0.18f, 1.0f);
    const glm::vec4 SPIKE_BODY(0.56f, 0.60f, 0.69f, 1.0f);
    const glm::vec4 SPIKE_TIP(0.88f, 0.92f, 0.97f, 1.0f);
    const glm::vec4 SPIKE_BASE(0.30f, 0.32f, 0.39f, 1.0f);
    const glm::vec4 SUPPORT_COLOR(0.31f, 0.34f, 0.41f, 1.0f); // 闸门支柱等背景构件
    const glm::vec4 CHECKPOINT_OFF(0.60f, 0.64f, 0.72f, 1.0f);
    const glm::vec4 CHECKPOINT_ON(0.34f, 0.88f, 0.52f, 1.0f);
    const glm::vec4 HILL_FAR(0.62f, 0.75f, 0.83f, 1.0f);
    const glm::vec4 HILL_NEAR(0.47f, 0.68f, 0.64f, 1.0f);
    const glm::vec4 PLAYER_BODY(0.95f, 0.60f, 0.29f, 1.0f);
    const glm::vec4 PLAYER_BELLY(1.00f, 0.80f, 0.48f, 1.0f);
    const glm::vec4 EYE_WHITE(1.00f, 1.00f, 1.00f, 1.0f);
    const glm::vec4 EYE_PUPIL(0.22f, 0.16f, 0.14f, 1.0f);
    const glm::vec4 COIN_GOLD(1.00f, 0.82f, 0.30f, 1.0f);
    const glm::vec4 COIN_CORE(1.00f, 0.95f, 0.68f, 1.0f);
    const glm::vec4 FLAG_RED(0.92f, 0.30f, 0.36f, 1.0f);
    const glm::vec4 FLAG_POLE(0.58f, 0.62f, 0.70f, 1.0f);
    const glm::vec4 DUST(0.96f, 0.97f, 1.00f, 0.80f);
    const glm::vec4 DEATH_SPARK(1.00f, 0.38f, 0.40f, 1.00f);

    std::mt19937 g_rng(0x5EED1234u);

    float RandomFloat(float min, float max)
    {
        return std::uniform_real_distribution<float>(min, max)(g_rng);
    }

    constexpr float BGM_VOLUME = 0.42f;
    constexpr float BGM_VICTORY_VOLUME = 0.12f;

    // 运行时工作目录下的资源路径（assets 由 CMake POST_BUILD 复制到输出目录）
    std::string AudioAssetPath(const char* fileName)
    {
        return std::string("assets/audio/") + fileName;
    }

    // 2D 一次性音效：Play 返回的声部由 AudioEngine 自动持有到播放结束
    void PlayOneShot(const Ref<AudioClip>& clip, float volume, float pitch = 1.0f)
    {
        if (!clip) {
            return;
        }

        AudioPlaybackSettings settings;
        settings.volume = volume;
        settings.pitch = pitch;
        clip->Play(settings);
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

    // ---------- 尖刺时间轴 ----------
    // 伸缩尖刺在周期内的伸出比例（cycle 为 [0,1) 相位）：
    //   0.00–0.50 完全缩回（安全，只留基座）
    //   0.50–0.60 预警：尖头微微露出（不致命，提示"要弹出来了"）
    //   0.60–0.67 快速弹出（越过致命阈值后开始危险）
    //   0.67–0.90 完全伸出（致命）
    //   0.90–1.00 收回
    float SpikeExtendAt(float cycle)
    {
        const float t = cycle - std::floor(cycle);
        if (t < 0.50f) {
            return 0.0f;
        }
        if (t < 0.60f) {
            const float u = (t - 0.50f) / 0.10f;
            return 0.14f + 0.08f * (0.5f - 0.5f * std::cos(u * PI));   // 0.14 → 0.22
        }
        if (t < 0.67f) {
            const float u = (t - 0.60f) / 0.07f;
            return 0.22f + 0.78f * u * u;
        }
        if (t < 0.90f) {
            return 1.0f;
        }
        return 1.0f - (t - 0.90f) / 0.10f;
    }

    // ---------- 网格资源 ----------

    Ref<VertexArray> CreateSolidMesh()
    {
        // 单位矩形：宽 1 高 1、中心在原点，便于用 Scale 复用（平台 / 旗杆 / 基座等）
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
        // 终点旗面（顶点色即红色）：附着点在左侧中点，向右展开
        MeshBuilder builder;
        builder.AddPolygon({ { 0.0f, 0.5f }, { 0.9f, 0.25f }, { 0.0f, 0.0f } }, FLAG_RED, 0.0f);
        return BuildMesh(builder);
    }

    Ref<VertexArray> CreatePennantMesh()
    {
        // 存档点小旗（白色顶点色，绘制时用 tint 上色：未激活灰 / 激活绿）
        MeshBuilder builder;
        builder.AddPolygon({ { 0.0f, 0.5f }, { 0.9f, 0.25f }, { 0.0f, 0.0f } },
                           glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 0.0f);
        return BuildMesh(builder);
    }

    Ref<VertexArray> CreateSpikeToothMesh()
    {
        // 单根尖齿：底边 y = 0、齿尖 y = 1、水平居中（宽 1），
        // 绘制时按「齿宽 x 伸出高度」缩放，右侧留一条亮面表达光照
        MeshBuilder builder;
        builder.AddPolygon({ { -0.5f, 0.0f }, { 0.5f, 0.0f }, { 0.0f, 1.0f } }, SPIKE_BODY, 0.0f);
        builder.AddPolygon({ { -0.46f, 0.0f }, { -0.16f, 0.0f }, { 0.0f, 1.0f } }, SPIKE_TIP, 0.02f);
        return BuildMesh(builder);
    }

    Ref<VertexArray> CreateHillMesh()
    {
        // 远山：单位半圆（宽 1、高 1），底边贴在 y = 0 上
        constexpr int SEGMENTS = 20;
        MeshBuilder builder;
        std::vector<glm::vec2> points;
        points.reserve(SEGMENTS + 1);
        for (int i = 0; i <= SEGMENTS; ++i) {
            const float angle = PI * static_cast<float>(i) / static_cast<float>(SEGMENTS);
            points.push_back(glm::vec2(std::cos(angle) * 0.5f, std::sin(angle)));
        }
        builder.AddPolygon(points, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 0.0f);
        return BuildMesh(builder);
    }

    // ---------- 背景布局 ----------
    struct HillSeed {
        float offset;
        float width;
        float height;
        float scrollFactor;   // 相机位移参与比例：越小越远（视觉移动越慢）
        float z;
        glm::vec4 color;
    };

    // 远山分两层：远层滚动最慢、颜色最淡；近层更快、更实（都排在云层之后）
    const HillSeed HILL_SEEDS[] = {
        { 0.0f, 620.0f, 210.0f, 0.90f, -3.95f, HILL_FAR },
        { 520.0f, 470.0f, 150.0f, 0.90f, -3.95f, HILL_FAR },
        { 1080.0f, 700.0f, 255.0f, 0.90f, -3.95f, HILL_FAR },
        { 1620.0f, 460.0f, 140.0f, 0.90f, -3.95f, HILL_FAR },
        { 220.0f, 460.0f, 170.0f, 0.78f, -3.60f, HILL_NEAR },
        { 780.0f, 380.0f, 215.0f, 0.78f, -3.60f, HILL_NEAR },
        { 1360.0f, 520.0f, 190.0f, 0.78f, -3.60f, HILL_NEAR },
        { 1900.0f, 400.0f, 150.0f, 0.78f, -3.60f, HILL_NEAR },
    };
}

// ---------- PlatformerCamera ----------

glm::vec3 PlatformerCamera::GetPosition() const
{
    return glm::vec3(m_center.x, m_center.y, 10.0f);
}

glm::mat4 PlatformerCamera::GetViewMatrix() const
{
    // 相机中心平移至原点（相机效果应体现在这里返回的视图矩阵中，随 UBO 上传）
    return Translate(m_shakeOffset) * Translate(glm::vec3(-m_center.x, -m_center.y, 0.0f));
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
    BuildBackdrop();
    LoadSounds();
    ResetLevel();
    StartMusic();

    RenderCommand::SetClearColor(glm::vec4(0.62f, 0.78f, 0.92f, 1.0f));

    OI_INFO("Platformer OnAttach");
}

void PlatformerLayer::OnDetach()
{
    if (m_bgm) {
        m_bgm->Stop();
        m_bgm.reset();
    }
}

void PlatformerLayer::LoadSounds()
{
    if (!AudioEngine::IsInitialized()) {
        OI_WARN("Platformer: audio device unavailable, sound effects disabled");
        return;
    }

    m_bgmClip = AudioEngine::LoadClip(AudioAssetPath("bgm.wav"), /*streaming=*/true);
    m_jumpSound = AudioEngine::LoadClip(AudioAssetPath("jump.wav"));
    m_landSound = AudioEngine::LoadClip(AudioAssetPath("land.wav"));
    m_coinSound = AudioEngine::LoadClip(AudioAssetPath("coin.wav"));
    m_checkpointSound = AudioEngine::LoadClip(AudioAssetPath("checkpoint.wav"));
    m_deathSound = AudioEngine::LoadClip(AudioAssetPath("death.wav"));
    m_victorySound = AudioEngine::LoadClip(AudioAssetPath("victory.wav"));

    AudioEngine::SetMasterVolume(m_audioVolume);
    AudioEngine::SetMuted(m_audioMuted);
}

void PlatformerLayer::StartMusic()
{
    if (!m_bgmClip) {
        return;
    }
    if (m_bgm && !m_bgm->IsFinished()) {
        return;
    }

    AudioPlaybackSettings music;
    music.volume = BGM_VOLUME;
    music.looping = true;
    m_bgm = m_bgmClip->Play(music);
    if (!m_bgm) {
        OI_WARN("Platformer: failed to start background music");
        return;
    }
    OI_INFO("Platformer: background music started (loop)");
}

void PlatformerLayer::CreateShaders()
{
    m_shader = Shader::Create("assets/shader/scenes/platformer.glsl");
}

void PlatformerLayer::CreateMeshes()
{
    m_solidMesh = CreateSolidMesh();
    m_skyMesh = CreateSkyMesh();
    m_glowMesh = CreateGlowMesh();
    m_coinMesh = CreateCoinMesh();
    m_playerMesh = CreatePlayerMesh();
    m_flagMesh = CreateFlagMesh();
    m_pennantMesh = CreatePennantMesh();
    m_spikeMesh = CreateSpikeToothMesh();
    m_hillMesh = CreateHillMesh();
}

void PlatformerLayer::BuildBackdrop()
{
    m_clouds.clear();
    m_hills.clear();

    // 云层：视差背景，随机分布并错开 z
    for (int i = 0; i < 14; ++i) {
        Cloud cloud;
        cloud.x = RandomFloat(0.0f, CLOUD_LOOP);
        cloud.y = RandomFloat(170.0f, 420.0f);
        cloud.scale = RandomFloat(110.0f, 240.0f);
        cloud.z = -2.75f + 0.01f * static_cast<float>(i);
        cloud.color = glm::vec4(1.0f, 1.0f, 1.0f, RandomFloat(0.22f, 0.45f));
        m_clouds.push_back(cloud);
    }

    // 远山：两层视差，循环铺开（底边沉在视野下方，像远处的谷地）
    for (const HillSeed& seed : HILL_SEEDS) {
        Hill hill;
        hill.x = seed.offset;
        hill.y = -540.0f;
        hill.width = seed.width;
        hill.height = seed.height;
        hill.scrollFactor = seed.scrollFactor;
        hill.z = seed.z;
        hill.color = seed.color;
        m_hills.push_back(hill);
    }
}

void PlatformerLayer::BuildLevel()
{
    m_platforms.clear();
    m_spikes.clear();
    m_decors.clear();
    m_checkpoints.clear();
    m_coins.clear();

    // ================== §A 起点营地（教学：跳上矮台阶） ==================
    AddBlock(-470.0f, -100.0f, 20.0f, 150.0f, false);   // 左端挡墙（防止跑出关卡）
    AddGround(-130.0f, 370.0f, GROUND_TOP);             // x ∈ [-500, 240]
    AddBlock(170.0f, -340.0f, 55.0f, 45.0f, true);      // 矮台阶（抬高 60）
    AddCoin(-260.0f, -330.0f);
    AddCoin(170.0f, -270.0f);
    m_spawnPoint = glm::vec2(-40.0f, GROUND_TOP + PLAYER_HALF_HEIGHT);

    // ================== §B 尖刺与缺口（教学：尖刺会扎人；掉进缺口算坠落） ==================
    // 缺口 [240, 430] 特意不设坑底：摔下去就是"掉下去了"，避免看不见的坑底机关骗人
    AddGround(690.0f, 260.0f, GROUND_TOP);              // 地面 [430, 950]
    AddSpikes(glm::vec2(575.0f, GROUND_TOP), 70.0f, Spike::Direction::Up);
    AddSpikes(glm::vec2(785.0f, GROUND_TOP), 70.0f, Spike::Direction::Up);
    AddBlock(890.0f, -320.0f, 45.0f, 50.0f, true);      // 台阶（继续向右）
    AddCheckpoint(470.0f, GROUND_TOP);
    AddCoin(470.0f, -330.0f);
    AddCoin(680.0f, -320.0f);
    AddCoin(890.0f, -250.0f);

    // ================== §C 阶梯上行 + 伸缩尖刺平台 ==================
    AddBlock(1030.0f, -310.0f, 60.0f, 60.0f, true);
    AddBlock(1230.0f, -210.0f, 60.0f, 60.0f, true);
    AddBlock(1470.0f, -140.0f, 120.0f, 60.0f, true);    // 平台正中埋着伸缩尖刺：看准缩回再跳过去
    AddCyclicSpikes(glm::vec2(1470.0f, -140.0f), 90.0f, Spike::Direction::Up, 46.0f, 2.6f, 0.0f);
    AddBlock(1740.0f, -230.0f, 70.0f, 60.0f, true);
    AddCheckpoint(1700.0f, -230.0f);
    AddCoin(1030.0f, -240.0f);
    AddCoin(1230.0f, -140.0f);
    AddCoin(1770.0f, -160.0f);

    // ================== §D 深渊 + 水平移动平台 ==================
    // 深渊 [1810, 2380] 不设坑底：错过移动平台就是坠落（和地面尖刺区分开）
    AddMovingPlatform(glm::vec2(1880.0f, -268.0f), glm::vec2(2300.0f, -268.0f),
                      glm::vec2(70.0f, 18.0f), 140.0f, 0.45f);   // 顶面 -250
    AddBlock(2560.0f, -280.0f, 90.0f, 60.0f, true);     // 对岸落脚平台
    AddCheckpoint(2510.0f, -280.0f);
    AddCoin(2090.0f, -170.0f);      // 深渊正上方：得在移动平台上起跳才拿得到
    AddCoin(2560.0f, -200.0f);

    // ================== §E 垂直升降 + 壁刺走廊 ==================
    // 升降台顶面在 -340（低）与 -150（高）之间往返；右侧立柱贴着朝左的尖刺。
    // 尖刺判定带（玩家中心 y ∈ [-315, -205]）只覆盖升降台中段：
    // 上车、到顶、跳到立柱顶都安全，只有"升到一半还站在升降台右侧"才会撞刺
    AddMovingPlatform(glm::vec2(2740.0f, -356.0f), glm::vec2(2740.0f, -166.0f),
                      glm::vec2(80.0f, 16.0f), 95.0f, 0.45f);
    AddBlock(2880.0f, -150.0f, 40.0f, 85.0f, false);    // 立柱（顶面与升降台高位齐平，可直接跨过去）
    AddSpikes(glm::vec2(2840.0f, -260.0f), 70.0f, Spike::Direction::Left, 100.0f);
    AddBlock(3050.0f, -150.0f, 80.0f, 60.0f, true);
    AddCheckpoint(3000.0f, -150.0f);
    AddCoin(2650.0f, -122.0f);      // 升降台左侧：正好提示"往这边躲"
    AddCoin(3080.0f, -80.0f);

    // ================== §F 天花板闸门 + 终局跳台 ==================
    AddBlock(3480.0f, -150.0f, 210.0f, 60.0f, true);    // 闸门走廊 [3270, 3690]
    AddDecor(3370.0f, -150.0f, 24.0f, 100.0f, SUPPORT_COLOR);
    AddDecor(3650.0f, -150.0f, 24.0f, 100.0f, SUPPORT_COLOR);
    AddBlock(3510.0f, 10.0f, 150.0f, 30.0f, false);     // 闸门顶盖 y ∈ [-50, 10]
    AddCyclicSpikes(glm::vec2(3510.0f, -50.0f), 250.0f, Spike::Direction::Down, 90.0f, 3.0f, 0.0f);
    AddSpikes(glm::vec2(3510.0f, 10.0f), 290.0f, Spike::Direction::Up, 40.0f);  // 顶盖上也有刺：堵住翻越近路
    // 走廊尽头的缺口 [3690, 3840] 同样不设坑底：摔下去算坠落
    AddBlock(3930.0f, -190.0f, 90.0f, 60.0f, true);
    AddCheckpoint(3900.0f, -190.0f);
    AddBlock(4180.0f, -150.0f, 45.0f, 40.0f, true);     // 窄台两连跳
    AddBlock(4390.0f, -100.0f, 40.0f, 40.0f, true);
    AddBlock(4720.0f, -100.0f, 200.0f, 60.0f, true);    // 终点大平台
    AddBlock(4980.0f, -100.0f, 20.0f, 150.0f, false);   // 右端挡墙
    m_flagPosition = glm::vec2(4780.0f, -100.0f);
    AddCoin(3510.0f, -120.0f);      // 闸门正下方：贪这一枚得卡准时机
    AddCoin(4180.0f, -80.0f);
    AddCoin(4390.0f, -30.0f);
    AddCoin(4700.0f, -20.0f);

    m_respawnPoint = m_spawnPoint;
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

void PlatformerLayer::AddMovingPlatform(const glm::vec2& from, const glm::vec2& to, const glm::vec2& halfSize,
                                        float speed, float pauseTime, float startT)
{
    Platform platform;
    platform.halfSize = halfSize;
    platform.bodyColor = MOVER_BODY;
    platform.topColor = MOVER_TOP;
    platform.pathStart = from;
    platform.pathEnd = to;
    platform.speed = speed;
    platform.pauseTime = pauseTime;
    platform.t = glm::clamp(startT, 0.0f, 1.0f);
    platform.position = from + (to - from) * platform.t;
    m_platforms.push_back(platform);
}

void PlatformerLayer::AddDecor(float centerX, float centerY, float halfWidth, float halfHeight,
                               const glm::vec4& color)
{
    Decor decor;
    decor.position = glm::vec2(centerX, centerY);
    decor.halfSize = glm::vec2(halfWidth, halfHeight);
    decor.color = color;
    m_decors.push_back(decor);
}

void PlatformerLayer::AddSpikes(const glm::vec2& basePosition, float width, Spike::Direction direction,
                                float height)
{
    Spike spike;
    spike.position = basePosition;
    spike.width = width;
    spike.height = height;
    spike.direction = direction;
    spike.extend = 1.0f;
    m_spikes.push_back(spike);
}

void PlatformerLayer::AddCyclicSpikes(const glm::vec2& basePosition, float width, Spike::Direction direction,
                                      float height, float period, float startPhase)
{
    Spike spike;
    spike.position = basePosition;
    spike.width = width;
    spike.height = height;
    spike.direction = direction;
    spike.cyclic = true;
    spike.period = period;
    spike.phase = startPhase - std::floor(startPhase);
    spike.extend = SpikeExtendAt(spike.phase);
    m_spikes.push_back(spike);
}

void PlatformerLayer::AddCheckpoint(float x, float surfaceY)
{
    Checkpoint checkpoint;
    checkpoint.position = glm::vec2(x, surfaceY);
    m_checkpoints.push_back(checkpoint);
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
    // 整关重建：金币、存档点、移动平台相位、伸缩尖刺相位全部回到初始状态
    BuildLevel();

    m_playerPosition = m_spawnPoint;
    m_velocity = glm::vec2(0.0f, 0.0f);
    m_grounded = false;
    m_wasGrounded = false;
    m_coyoteTimer = 0.0f;
    m_jumpBufferTimer = 0.0f;
    m_jumpDownPrev = false;
    m_jumpHeld = false;
    m_jumpHoldTime = 0.0f;
    m_roseInAir = false;
    m_apexActive = false;
    m_fastFalling = false;
    m_facing = 1;
    m_squash = 1.0f;
    m_landSpeed = 0.0f;
    m_groundPlatformIndex = -1;
    m_inputLockTimer = 0.0f;

    m_coinsCollected = 0;
    m_time = 0.0f;
    m_deaths = 0;
    m_shake = 0.0f;
    m_deathFlash = 0.0f;
    m_deathCause = nullptr;
    m_deathCauseTimer = 0.0f;
    m_state = GameState::Playing;
    m_puffs.clear();

    // 相机直接就位，避免从上一个位置平滑拉过来
    m_camera.SetCenter(glm::vec2(CameraTargetX(m_spawnPoint.x), 0.0f));
    m_camera.SetShakeOffset(glm::vec3(0.0f, 0.0f, 0.0f));

    // 从胜利 / 低音量状态重开时恢复 BGM
    if (m_bgm) {
        m_bgm->SetVolume(BGM_VOLUME);
        if (!m_bgm->IsPlaying()) {
            m_bgm->Resume();
        }
    }
}

void PlatformerLayer::RespawnAtCheckpoint()
{
    m_playerPosition = m_respawnPoint;
    m_velocity = glm::vec2(0.0f, 0.0f);
    m_grounded = false;
    m_groundPlatformIndex = -1;
    m_wasGrounded = false;
    m_coyoteTimer = 0.0f;
    m_jumpBufferTimer = 0.0f;
    m_jumpHeld = false;
    m_jumpHoldTime = 0.0f;
    m_roseInAir = false;
    m_apexActive = false;
    m_fastFalling = false;
    m_squash = 1.0f;
    m_inputLockTimer = 0.22f;   // 复活瞬间锁一下输入，避免惯性误操作

    // 相机直接就位（长关卡里不能从死亡点慢慢拉过来）
    m_camera.SetCenter(glm::vec2(CameraTargetX(m_playerPosition.x), 0.0f));
}

void PlatformerLayer::KillPlayer(const char* cause, const glm::vec2& effectPosition)
{
    if (m_state != GameState::Playing) {
        return;
    }

    ++m_deaths;
    m_shake = 26.0f;
    m_deathFlash = 1.0f;
    m_deathCause = cause;
    m_deathCauseTimer = 1.8f;

    SpawnPuff(effectPosition, 12, DEATH_SPARK, 250.0f, 15.0f, 300.0f);
    SpawnPuff(effectPosition, 6, DUST, 130.0f, 12.0f, 420.0f);
    PlayOneShot(m_deathSound, 0.52f, RandomFloat(0.96f, 1.04f));

    RespawnAtCheckpoint();
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

void PlatformerLayer::UpdatePlatforms(float deltaTime)
{
    for (auto& platform : m_platforms) {
        if (!platform.IsMoving() || deltaTime <= 0.0f) {
            continue;
        }

        const glm::vec2 previous = platform.position;
        if (platform.pauseTimer > 0.0f) {
            platform.pauseTimer = std::max(0.0f, platform.pauseTimer - deltaTime);
        } else {
            const float pathLength = glm::length(platform.pathEnd - platform.pathStart);
            if (pathLength > 0.0001f) {
                platform.t += platform.speed / pathLength * deltaTime * static_cast<float>(platform.pathDir);
                if (platform.t >= 1.0f) {
                    platform.t = 1.0f;
                    platform.pathDir = -1;
                    platform.pauseTimer = platform.pauseTime;
                } else if (platform.t <= 0.0f) {
                    platform.t = 0.0f;
                    platform.pathDir = 1;
                    platform.pauseTimer = platform.pauseTime;
                }
                platform.position = platform.pathStart + (platform.pathEnd - platform.pathStart) * platform.t;
            }
        }

        // 记录本帧实际位移速度：载人、轨道提示都靠它
        platform.velocity = (platform.position - previous) / deltaTime;
    }
}

void PlatformerLayer::UpdateSpikes(float deltaTime)
{
    for (auto& spike : m_spikes) {
        if (!spike.cyclic) {
            spike.extend = 1.0f;
            continue;
        }
        spike.phase = spike.phase + deltaTime / std::max(spike.period, 0.05f);
        spike.phase -= std::floor(spike.phase);
        spike.extend = SpikeExtendAt(spike.phase);
    }
}

void PlatformerLayer::UpdatePlayer(float deltaTime)
{
    const ImGuiIO& io = ImGui::GetIO();
    // 只有"文本输入激活"时才屏蔽游戏键盘输入；仅窗口聚焦/键盘导航不算，
    // 否则打开 F4 面板后角色会不听使唤（WantCaptureKeyboard 可能为 true）
    if (m_inputLockTimer > 0.0f) {
        m_inputLockTimer = std::max(0.0f, m_inputLockTimer - deltaTime);
    }
    const bool acceptInput = !io.WantTextInput && m_state == GameState::Playing && m_inputLockTimer <= 0.0f;

    const bool moveLeft = acceptInput && (Input::IsKeyPressed(Key::A) || Input::IsKeyPressed(Key::Left));
    const bool moveRight = acceptInput && (Input::IsKeyPressed(Key::D) || Input::IsKeyPressed(Key::Right));
    const bool jumpDownRaw = Input::IsKeyPressed(Key::Space) || Input::IsKeyPressed(Key::W) ||
                             Input::IsKeyPressed(Key::Up);
    const bool jumpDown = acceptInput && jumpDownRaw;
    const bool fastFallDown = acceptInput && (Input::IsKeyPressed(Key::S) || Input::IsKeyPressed(Key::Down));

    // ---- 跳跃缓冲：新按下时刷新，落地前提前按下也算数 ----
    // 记录原始按键状态（而非过滤后的），这样复活时如果还按着跳跃键不会立刻又起跳
    if (jumpDown && !m_jumpDownPrev) {
        m_jumpBufferTimer = m_tuning.jumpBufferTime;
    } else {
        m_jumpBufferTimer = std::max(0.0f, m_jumpBufferTimer - deltaTime);
    }
    m_jumpDownPrev = jumpDownRaw;

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
        PlayOneShot(m_jumpSound, 0.32f, RandomFloat(0.97f, 1.03f));
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

    // ---- 载人：站在移动平台上时，把平台本帧位移叠加进自己的移动 ----
    glm::vec2 carry(0.0f, 0.0f);
    if (m_grounded && m_groundPlatformIndex >= 0 &&
        m_groundPlatformIndex < static_cast<int>(m_platforms.size())) {
        carry = m_platforms[static_cast<size_t>(m_groundPlatformIndex)].velocity * deltaTime;
    }

    // ---- 移动与碰撞（m_grounded / m_groundPlatformIndex 由碰撞解算重置）----
    m_grounded = false;
    m_groundPlatformIndex = -1;
    MoveAndCollide(glm::vec2(m_velocity.x * deltaTime, m_velocity.y * deltaTime) + carry);

    // ---- 落地 / 离地处理 ----
    if (m_grounded) {
        if (!m_wasGrounded && m_landSpeed > 180.0f) {
            const float impact = glm::clamp(m_landSpeed / 2400.0f, 0.0f, 1.0f);
            m_squash = 1.0f - 0.18f * impact;
            SpawnPuff(m_playerPosition + glm::vec2(0.0f, -PLAYER_HALF_HEIGHT),
                      4 + static_cast<int>(impact * 5.0f), DUST, 80.0f + 120.0f * impact, 13.0f, 500.0f);
            PlayOneShot(m_landSound, 0.14f + 0.18f * impact, 0.95f + 0.10f * impact);
        }
        m_coyoteTimer = m_tuning.coyoteTime;
        m_apexActive = false;
        m_roseInAir = false;
    } else {
        m_coyoteTimer = std::max(0.0f, m_coyoteTimer - deltaTime);
    }
    m_wasGrounded = m_grounded;
    m_landSpeed = 0.0f;

    // ---- 世界边界与坠落 ----
    m_playerPosition.x = glm::clamp(m_playerPosition.x,
                                    WORLD_MIN_X + PLAYER_HALF_WIDTH + 4.0f,
                                    WORLD_MAX_X - PLAYER_HALF_WIDTH - 4.0f);
    if (m_playerPosition.y < WORLD_KILL_Y) {
        KillPlayer("掉下去了", m_playerPosition);
        return;
    }

    if (m_state != GameState::Playing) {
        return;
    }

    // ---- 尖刺判定 ----
    if (CheckHazards()) {
        return;
    }

    // ---- 存档点 ----
    for (auto& checkpoint : m_checkpoints) {
        if (checkpoint.activated) {
            continue;
        }
        if (std::abs(checkpoint.position.x - m_playerPosition.x) < PLAYER_HALF_WIDTH + 26.0f &&
            std::abs(checkpoint.position.y - m_playerPosition.y) < PLAYER_HALF_HEIGHT + 34.0f) {
            checkpoint.activated = true;
            m_respawnPoint = glm::vec2(checkpoint.position.x, checkpoint.position.y + PLAYER_HALF_HEIGHT + 2.0f);
            SpawnPuff(checkpoint.position + glm::vec2(0.0f, 44.0f), 12,
                      glm::vec4(0.45f, 1.0f, 0.62f, 1.0f), 170.0f, 13.0f, -40.0f);
            PlayOneShot(m_checkpointSound, 0.42f);
        }
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
            PlayOneShot(m_coinSound, 0.32f, RandomFloat(0.98f, 1.04f));
        }
    }

    // ---- 终点旗 ----
    const glm::vec2 flagCenter = m_flagPosition + glm::vec2(0.0f, FLAG_HEIGHT * 0.5f);
    if (std::abs(flagCenter.x - m_playerPosition.x) < PLAYER_HALF_WIDTH + 26.0f &&
        std::abs(flagCenter.y - m_playerPosition.y) < PLAYER_HALF_HEIGHT + FLAG_HEIGHT * 0.5f) {
        m_state = GameState::Victory;
        if (m_bgm) {
            m_bgm->SetVolume(BGM_VICTORY_VOLUME);
        }
        PlayOneShot(m_victorySound, 0.55f);
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

        // Y 轴：按"最小推出距离"决定落在顶面还是撞到底面。
        // 不能只看位移方向：上行升降台载人时玩家的净位移是向上的，
        // 会被误判成头撞底面而塞到平台下方（表现为"平台一向上移动人就掉下去"）
        if (stepDelta.y != 0.0f) {
            m_playerPosition.y += stepDelta.y;
            for (size_t i = 0; i < m_platforms.size(); ++i) {
                const Platform& platform = m_platforms[i];
                if (!OverlapsPlatform(platform)) {
                    continue;
                }

                const float platformTop = platform.position.y + platform.halfSize.y;
                const float platformBottom = platform.position.y - platform.halfSize.y;
                const float pushUp = platformTop - (m_playerPosition.y - PLAYER_HALF_HEIGHT);
                const float pushDown = (m_playerPosition.y + PLAYER_HALF_HEIGHT) - platformBottom;

                if (pushUp <= pushDown) {
                    // 从上方进入：站到顶面（移动平台向上顶人时也走这里）
                    m_playerPosition.y = platformTop + PLAYER_HALF_HEIGHT;
                    m_landSpeed = std::max(m_landSpeed, -m_velocity.y);
                    m_grounded = true;
                    m_groundPlatformIndex = static_cast<int>(i);
                } else {
                    // 从下方进入：撞头
                    m_playerPosition.y = platformBottom - PLAYER_HALF_HEIGHT;
                }
                m_velocity.y = 0.0f;
            }
        }
    }
}

bool PlatformerLayer::SpikeHitbox(const Spike& spike, glm::vec2& center, glm::vec2& halfSize) const
{
    if (spike.extend < m_tuning.spikeDeadlyExtend) {
        return false;   // 缩回 / 预警期不参与判定（预警露尖是纯粹的安全提示）
    }

    const float reach = spike.height * spike.extend;
    const float alongHalfWidth = spike.width * 0.5f * m_tuning.spikeHitboxShrink;   // 齿尖细，横向放宽
    const float alongCenter = reach * 0.58f;   // 沿突出方向的判定中心（跳过靠近基座的一段）
    const float alongHalf = reach * 0.37f;

    switch (spike.direction) {
        case Spike::Direction::Up:
            center = spike.position + glm::vec2(0.0f, alongCenter);
            halfSize = glm::vec2(alongHalfWidth, alongHalf);
            break;
        case Spike::Direction::Down:
            center = spike.position + glm::vec2(0.0f, -alongCenter);
            halfSize = glm::vec2(alongHalfWidth, alongHalf);
            break;
        case Spike::Direction::Left:
            center = spike.position + glm::vec2(-alongCenter, 0.0f);
            halfSize = glm::vec2(alongHalf, alongHalfWidth);
            break;
        case Spike::Direction::Right:
            center = spike.position + glm::vec2(alongCenter, 0.0f);
            halfSize = glm::vec2(alongHalf, alongHalfWidth);
            break;
    }
    return true;
}

bool PlatformerLayer::CheckHazards()
{
    for (const auto& spike : m_spikes) {
        glm::vec2 center(0.0f, 0.0f);
        glm::vec2 halfSize(0.0f, 0.0f);
        if (!SpikeHitbox(spike, center, halfSize)) {
            continue;
        }
        if (std::abs(m_playerPosition.x - center.x) < PLAYER_HALF_WIDTH + halfSize.x &&
            std::abs(m_playerPosition.y - center.y) < PLAYER_HALF_HEIGHT + halfSize.y) {
            KillPlayer("被尖刺扎到", (m_playerPosition + center) * 0.5f);
            return true;
        }
    }
    return false;
}

float PlatformerLayer::SpikeRotation(Spike::Direction direction)
{
    switch (direction) {
        case Spike::Direction::Up:
            return 0.0f;
        case Spike::Direction::Down:
            return PI;                 // 单位齿默认指向 +Y，旋转 π 后朝下
        case Spike::Direction::Left:
            return PI * 0.5f;          // 朝 -X
        case Spike::Direction::Right:
            return -PI * 0.5f;         // 朝 +X
    }
    return 0.0f;
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
    // 独立项目：显式关闭 HDR 与光源剔除，保持 2D 非 HDR 渲染路径
    Renderer::SetHDREnabled(false);
    Renderer::SetLightCullingEnabled(false);

    const float deltaTime = std::min(Timer::GetDeltaTime(), 0.05f);

    RenderCommand::Clear();

    if (m_state == GameState::Playing) {
        m_time += deltaTime;
    }

    UpdatePlatforms(deltaTime);
    UpdateSpikes(deltaTime);
    UpdatePlayer(deltaTime);

    // 挤压拉伸回弹（落地压扁 / 起跳拉伸后恢复）
    m_squash += (1.0f - m_squash) * std::min(1.0f, deltaTime * 11.0f);

    // 震屏与死亡闪红衰减
    m_shake = std::max(0.0f, m_shake - deltaTime * 90.0f);
    m_deathFlash = std::max(0.0f, m_deathFlash - deltaTime * 2.6f);
    m_deathCauseTimer = std::max(0.0f, m_deathCauseTimer - deltaTime);

    glm::vec3 shakeOffset(0.0f, 0.0f, 0.0f);
    if (m_shake > 0.01f) {
        shakeOffset = glm::vec3(RandomFloat(-m_shake, m_shake) * 0.5f,
                                RandomFloat(-m_shake, m_shake) * 0.5f, 0.0f);
    }
    m_camera.SetShakeOffset(shakeOffset);

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
    const auto visible = [&](float x, float margin) {
        return std::abs(x - cameraCenter.x) <= halfWidth + margin;
    };

    // ---- 天空：全屏渐变，跟随相机 ----
    m_shader->SetFloat("u_Falloff", 0.0f);
    DrawMesh(m_skyMesh, Translate(glm::vec3(cameraCenter.x, 0.0f, -9.0f)) *
                            Scale(glm::vec3(halfWidth * 2.0f, VIEW_HALF_HEIGHT * 2.0f, 1.0f)), white);

    // ---- 远山：两层视差，图案循环避免出界 ----
    for (const auto& hill : m_hills) {
        float offsetX = std::fmod(hill.x - cameraCenter.x * hill.scrollFactor, HILL_LOOP);
        if (offsetX < -HILL_LOOP * 0.5f) {
            offsetX += HILL_LOOP;
        }
        if (offsetX > HILL_LOOP * 0.5f) {
            offsetX -= HILL_LOOP;
        }
        const float x = cameraCenter.x + offsetX;
        if (!visible(x, hill.width)) {
            continue;
        }
        DrawMesh(m_hillMesh, Translate(glm::vec3(x, hill.y, hill.z)) *
                                 Scale(glm::vec3(hill.width, hill.height, 1.0f)), hill.color);
    }

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
        if (!visible(x, cloud.scale * 1.5f)) {
            continue;
        }

        // 一朵云由三个重叠光晕（大-中-小、错位）拼出蓬松轮廓
        DrawMesh(m_glowMesh, Translate(glm::vec3(x - cloud.scale * 0.42f, cloud.y + 6.0f, cloud.z)) *
                                 Scale(glm::vec3(cloud.scale * 0.72f, cloud.scale * 0.44f, 1.0f)), cloud.color);
        DrawMesh(m_glowMesh, Translate(glm::vec3(x + cloud.scale * 0.18f, cloud.y + 16.0f, cloud.z)) *
                                 Scale(glm::vec3(cloud.scale, cloud.scale * 0.56f, 1.0f)), cloud.color);
        DrawMesh(m_glowMesh, Translate(glm::vec3(x + cloud.scale * 0.62f, cloud.y, cloud.z)) *
                                 Scale(glm::vec3(cloud.scale * 0.6f, cloud.scale * 0.38f, 1.0f)), cloud.color);
    }
    m_shader->SetFloat("u_Falloff", 0.0f);

    // ---- 背景装饰（闸门支柱等，无碰撞，画在平台之后）----
    for (const auto& decor : m_decors) {
        if (!visible(decor.position.x, decor.halfSize.x)) {
            continue;
        }
        DrawMesh(m_solidMesh, Translate(glm::vec3(decor.position, -0.55f)) *
                                  Scale(glm::vec3(decor.halfSize * 2.0f, 1.0f)), decor.color);
    }

    // ---- 平台：主体 + 顶面高光条（z 错开，避免同 z 重叠被深度测试剔除）----
    for (const auto& platform : m_platforms) {
        if (!visible(platform.position.x, platform.halfSize.x + 96.0f)) {
            continue;
        }

        // 移动平台先画一条半透明的往返轨道，方便预判落点
        if (platform.IsMoving()) {
            const glm::vec2 trackCenter = (platform.pathStart + platform.pathEnd) * 0.5f;
            const glm::vec2 trackSize = glm::abs(platform.pathEnd - platform.pathStart);
            DrawMesh(m_solidMesh, Translate(glm::vec3(trackCenter, -0.66f)) *
                                      Scale(glm::vec3(trackSize.x + 8.0f, trackSize.y + 8.0f, 1.0f)),
                     glm::vec4(1.0f, 1.0f, 1.0f, 0.14f));
        }

        DrawMesh(m_solidMesh, Translate(glm::vec3(platform.position, -0.4f)) *
                              Scale(glm::vec3(platform.halfSize * 2.0f, 1.0f)), platform.bodyColor);
        DrawMesh(m_solidMesh,
                 Translate(glm::vec3(platform.position.x, platform.position.y + platform.halfSize.y - 5.0f, -0.38f)) *
                     Scale(glm::vec3(platform.halfSize.x * 2.0f, 10.0f, 1.0f)), platform.topColor);

        // 移动平台补三颗铆钉：一眼区分"会动的"
        if (platform.IsMoving()) {
            for (int i = -1; i <= 1; ++i) {
                DrawMesh(m_solidMesh,
                         Translate(glm::vec3(platform.position.x + platform.halfSize.x * 0.55f * static_cast<float>(i),
                                             platform.position.y - platform.halfSize.y * 0.3f, -0.35f)) *
                             Scale(glm::vec3(9.0f, 9.0f, 1.0f)),
                         MOVER_RIVET);
            }
        }
    }

    // ---- 尖刺：基座 + 逐齿绘制（齿宽恒定，方向用旋转表达；缩回时只留基座）----
    for (const auto& spike : m_spikes) {
        if (!visible(spike.position.x, spike.width + 80.0f)) {
            continue;
        }

        const float rotation = SpikeRotation(spike.direction);
        const glm::mat4 baseTransform =
            Translate(glm::vec3(spike.position, -0.36f)) * RotateZ(rotation);

        // 基座：即使完全缩回也画一条暗色板，提示"这里埋着机关"（公平起见必须可见）
        DrawMesh(m_solidMesh, baseTransform * Translate(glm::vec3(0.0f, 3.0f, 0.0f)) *
                                  Scale(glm::vec3(spike.width, 6.0f, 1.0f)), SPIKE_BASE);

        const float teethHeight = spike.height * spike.extend;
        if (teethHeight < 1.0f) {
            continue;
        }

        // 预警阶段（尚未致命）：加一圈琥珀色脉冲光晕，给玩家反应时间
        if (spike.cyclic && spike.extend < m_tuning.spikeDeadlyExtend) {
            const float pulse = 0.30f + 0.22f * std::sin(m_time * 16.0f);
            m_shader->SetFloat("u_Falloff", 1.0f);
            DrawMesh(m_glowMesh, baseTransform * Translate(glm::vec3(0.0f, spike.height * 0.55f, 0.04f)) *
                                     Scale(glm::vec3(spike.width * 1.15f, spike.height * 1.5f, 1.0f)),
                     glm::vec4(1.0f, 0.72f, 0.28f, pulse));
            m_shader->SetFloat("u_Falloff", 0.0f);
        }

        const int teeth = std::max(1, static_cast<int>(std::floor(spike.width / 34.0f)));
        const float toothWidth = spike.width / static_cast<float>(teeth);
        for (int i = 0; i < teeth; ++i) {
            const float localX = -spike.width * 0.5f + toothWidth * (static_cast<float>(i) + 0.5f);
            DrawMesh(m_spikeMesh, baseTransform * Translate(glm::vec3(localX, 0.0f, 0.0f)) *
                                      Scale(glm::vec3(toothWidth, teethHeight, 1.0f)),
                     white);
        }
    }

    // ---- 存档点：未激活为灰、激活后变绿并带光晕 ----
    for (const auto& checkpoint : m_checkpoints) {
        if (!visible(checkpoint.position.x, 80.0f)) {
            continue;
        }
        const glm::vec4 poleColor = checkpoint.activated ? CHECKPOINT_ON : CHECKPOINT_OFF;

        DrawMesh(m_solidMesh, Translate(glm::vec3(checkpoint.position.x, checkpoint.position.y + 3.0f, -0.32f)) *
                                  Scale(glm::vec3(30.0f, 6.0f, 1.0f)), glm::vec4(0.32f, 0.35f, 0.42f, 1.0f));
        DrawMesh(m_solidMesh, Translate(glm::vec3(checkpoint.position.x, checkpoint.position.y + 25.0f, -0.32f)) *
                                  Scale(glm::vec3(6.0f, 50.0f, 1.0f)), poleColor);
        DrawMesh(m_pennantMesh, Translate(glm::vec3(checkpoint.position.x + 2.0f, checkpoint.position.y + 34.0f, -0.31f)) *
                                 Scale(glm::vec3(20.0f, 16.0f, 1.0f)), poleColor);

        if (checkpoint.activated) {
            m_shader->SetFloat("u_Falloff", 1.0f);
            DrawMesh(m_glowMesh, Translate(glm::vec3(checkpoint.position.x, checkpoint.position.y + 40.0f, -0.30f)) *
                                     Scale(glm::vec3(46.0f, 52.0f, 1.0f)),
                     glm::vec4(0.45f, 1.0f, 0.62f, 0.30f));
            m_shader->SetFloat("u_Falloff", 0.0f);
        }
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
        if (coin.collected || !visible(coin.position.x, 80.0f)) {
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

    // ---- 死亡闪红（整屏覆盖，随时间淡出）----
    if (m_deathFlash > 0.001f) {
        DrawMesh(m_solidMesh, Translate(glm::vec3(cameraCenter.x, 0.0f, 0.5f)) *
                                  Scale(glm::vec3(halfWidth * 2.0f, VIEW_HALF_HEIGHT * 2.0f, 1.0f)),
                 glm::vec4(0.92f, 0.16f, 0.22f, m_deathFlash * 0.32f));
    }

    // ---- 调试：判定盒可视化 ----
    if (m_showDebug && m_showHitboxes) {
        RenderHitboxes();
    }
}

void PlatformerLayer::RenderHitboxes()
{
    // 平台判定框
    for (const auto& platform : m_platforms) {
        DrawMesh(m_solidMesh, Translate(glm::vec3(platform.position, 0.30f)) *
                                  Scale(glm::vec3(platform.halfSize * 2.0f, 1.0f)),
                 glm::vec4(0.30f, 1.0f, 0.55f, 0.12f));
    }

    // 尖刺判定盒（黄 = 预警期不致命，红 = 当前致命）
    for (const auto& spike : m_spikes) {
        glm::vec2 center(0.0f, 0.0f);
        glm::vec2 halfSize(0.0f, 0.0f);
        if (!SpikeHitbox(spike, center, halfSize)) {
            continue;
        }
        DrawMesh(m_solidMesh, Translate(glm::vec3(center, 0.32f)) * Scale(glm::vec3(halfSize * 2.0f, 1.0f)),
                 glm::vec4(1.0f, 0.28f, 0.30f, 0.24f));
    }

    // 玩家判定框
    DrawMesh(m_solidMesh, Translate(glm::vec3(m_playerPosition, 0.34f)) *
                              Scale(glm::vec3(PLAYER_WIDTH, PLAYER_HEIGHT, 1.0f)),
             glm::vec4(1.0f, 1.0f, 1.0f, 0.18f));
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
        int activatedCheckpoints = 0;
        for (const auto& checkpoint : m_checkpoints) {
            activatedCheckpoints += checkpoint.activated ? 1 : 0;
        }

        ImGui::Text("平台跳跃");
        ImGui::Text("金币 %d / %d    用时 %.1f s    死亡 %d    存档点 %d/%d", m_coinsCollected,
                    static_cast<int>(m_coins.size()), m_time, m_deaths,
                    activatedCheckpoints, static_cast<int>(m_checkpoints.size()));
        if (m_deathCauseTimer > 0.0f && m_deathCause != nullptr) {
            ImGui::TextColored(ImVec4(1.0f, 0.48f, 0.48f, 1.0f), "%s！已回到最近的存档点", m_deathCause);
        }
        ImGui::TextDisabled("A/D 或 ←/→ 移动    空格/W/↑ 跳跃（按住跳更高，轻点跳得低）");
        ImGui::TextDisabled("↓/S 快速下坠    R 重开    F4 手感调参    Esc 退出");
        ImGui::TextDisabled("小心尖刺：灰色基座下可能藏着伸缩尖刺（露尖时安全，完全弹出才致命）");
        if (m_state == GameState::Victory) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.35f, 1.0f), "通关！用时 %.2f 秒，金币 %d/%d，死亡 %d 次",
                               m_time, m_coinsCollected, static_cast<int>(m_coins.size()), m_deaths);
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
    ImGui::SetNextWindowSize(ImVec2(360.0f, 580.0f), ImGuiCond_FirstUseEver);
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
        const bool onMover = m_grounded && m_groundPlatformIndex >= 0 &&
                             m_groundPlatformIndex < static_cast<int>(m_platforms.size()) &&
                             m_platforms[static_cast<size_t>(m_groundPlatformIndex)].IsMoving();
        ImGui::Text("vy %.0f px/s    跳跃按住 %.2f s（%s）", m_velocity.y, m_jumpHoldTime,
                    m_jumpHeld ? "按住中" : "已松开");
        ImGui::Text("所站平台：%s", onMover ? "移动平台（随平台位移）" : "地面 / 静态平台");
        ImGui::TextDisabled("ImGui 键盘捕获 %s / 文本输入 %s", io.WantCaptureKeyboard ? "开" : "关",
                            io.WantTextInput ? "开" : "关");
        ImGui::Separator();

        ImGui::Text("音频");
        if (!AudioEngine::IsInitialized()) {
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "音频设备未初始化");
        } else {
            if (ImGui::Checkbox("静音", &m_audioMuted)) {
                AudioEngine::SetMuted(m_audioMuted);
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120.0f);
            if (ImGui::SliderFloat("主音量", &m_audioVolume, 0.0f, 1.0f, "%.2f")) {
                AudioEngine::SetMasterVolume(m_audioVolume);
            }
            if (ImGui::Button("测试音效")) {
                PlayOneShot(m_coinSound, 0.35f);
                PlayOneShot(m_checkpointSound, 0.35f);
            }
        }
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
        if (ImGui::CollapsingHeader("4) 陷阱宽容度", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat("致命伸出阈值", &m_tuning.spikeDeadlyExtend, 0.01f, 0.0f, 1.0f, "%.2f");
            ImGui::DragFloat("判定宽度系数", &m_tuning.spikeHitboxShrink, 0.01f, 0.30f, 1.20f, "%.2f");
            ImGui::Checkbox("显示判定盒", &m_showHitboxes);
            ImGui::TextDisabled("伸缩尖刺：露尖阶段不致命，完全弹出才判定");
            if (ImGui::Button("重开关卡 (R)")) {
                ResetLevel();
            }
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
    RenderHud();
}

void PlatformerLayer::OnEvent(Event& event)
{
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
