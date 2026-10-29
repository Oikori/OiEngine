#include "SurvivorsArt.h"

#include <cmath>
#include <cstdint>
#include <vector>

using namespace OiEngine;

namespace {
    constexpr float PI = 3.14159265358979323846f;
    constexpr float TAU = 2.0f * PI;

    // ---------------- 调色板（暗夜 + 月色 + 宝石） ----------------
    const glm::vec4 WHITE(1.0f, 1.0f, 1.0f, 1.0f);

    const glm::vec4 PLAYER_CLOAK(0.29f, 0.33f, 0.62f, 1.0f);
    const glm::vec4 PLAYER_INNER(0.16f, 0.18f, 0.40f, 1.0f);
    const glm::vec4 PLAYER_TRIM(0.88f, 0.76f, 0.42f, 1.0f);
    const glm::vec4 PLAYER_VOID(0.05f, 0.04f, 0.09f, 1.0f);
    const glm::vec4 CRYSTAL(0.58f, 0.95f, 1.00f, 1.0f);

    // ---------------- 四个角色各自的配色（和 ItemColor 里的角色色大致对应） ----------------
    const glm::vec4 WATCH_STEEL(0.42f, 0.50f, 0.66f, 1.0f);   // 守夜人：钢蓝重甲 + 金边
    const glm::vec4 WATCH_DARK(0.22f, 0.26f, 0.38f, 1.0f);
    const glm::vec4 WATCH_GOLD(0.94f, 0.80f, 0.44f, 1.0f);

    const glm::vec4 HUNTER_CLOAK(0.26f, 0.44f, 0.34f, 1.0f);  // 猎手：墨绿斗篷 + 皮革
    const glm::vec4 HUNTER_DARK(0.14f, 0.24f, 0.20f, 1.0f);
    const glm::vec4 HUNTER_LEATHER(0.54f, 0.40f, 0.26f, 1.0f);
    const glm::vec4 HUNTER_GLOW(0.70f, 1.00f, 0.72f, 1.0f);

    const glm::vec4 SISTER_ROBE(0.88f, 0.88f, 0.94f, 1.0f);   // 修女：米白长袍 + 金十字
    const glm::vec4 SISTER_SHADE(0.62f, 0.64f, 0.76f, 1.0f);
    const glm::vec4 SISTER_GOLD(0.96f, 0.84f, 0.54f, 1.0f);

    const glm::vec4 WARLOCK_ROBE(0.32f, 0.20f, 0.48f, 1.0f);  // 术士：紫黑长袍 + 紫色法球
    const glm::vec4 WARLOCK_DARK(0.16f, 0.10f, 0.26f, 1.0f);
    const glm::vec4 WARLOCK_GLOW(0.74f, 0.54f, 1.00f, 1.0f);

    const glm::vec4 BAT_WING(0.42f, 0.22f, 0.52f, 1.0f);
    const glm::vec4 BAT_BODY(0.60f, 0.34f, 0.72f, 1.0f);
    const glm::vec4 BAT_EYE(1.00f, 0.72f, 0.35f, 1.0f);

    const glm::vec4 GHOUL_BODY(0.38f, 0.58f, 0.38f, 1.0f);
    const glm::vec4 GHOUL_DARK(0.22f, 0.38f, 0.25f, 1.0f);
    const glm::vec4 GHOUL_EYE(0.80f, 1.00f, 0.55f, 1.0f);

    const glm::vec4 WRAITH_SHELL(0.46f, 0.84f, 0.92f, 0.85f);
    const glm::vec4 WRAITH_TAIL(0.32f, 0.60f, 0.80f, 0.70f);
    const glm::vec4 WRAITH_CORE(0.75f, 1.00f, 1.00f, 0.95f);

    const glm::vec4 BRUTE_BODY(0.62f, 0.34f, 0.26f, 1.0f);
    const glm::vec4 BRUTE_METAL(0.42f, 0.40f, 0.44f, 1.0f);
    const glm::vec4 BRUTE_CORE(1.00f, 0.62f, 0.22f, 1.0f);

    const glm::vec4 BOSS_CLOAK(0.30f, 0.06f, 0.10f, 1.0f);
    const glm::vec4 BOSS_BONE(0.86f, 0.82f, 0.74f, 1.0f);
    const glm::vec4 BOSS_HORN(0.55f, 0.20f, 0.24f, 1.0f);
    const glm::vec4 BOSS_EYE(1.00f, 0.16f, 0.20f, 1.0f);

    const glm::vec4 BLADE_OUTER(0.62f, 0.92f, 1.00f, 1.0f);
    const glm::vec4 BLADE_INNER(0.92f, 1.00f, 1.00f, 1.0f);
    const glm::vec4 SCYTHE_OUTER(0.72f, 0.36f, 0.98f, 1.0f);
    const glm::vec4 SCYTHE_INNER(0.98f, 0.82f, 1.00f, 1.0f);

    const glm::vec4 COIN_RIM(0.84f, 0.60f, 0.18f, 1.0f);
    const glm::vec4 COIN_FACE(1.00f, 0.82f, 0.36f, 1.0f);
    const glm::vec4 COIN_MARK(0.70f, 0.46f, 0.13f, 1.0f);
    const glm::vec4 COIN_SHINE(1.00f, 0.98f, 0.86f, 0.95f);

    const glm::vec4 TOMB_STONE(0.26f, 0.25f, 0.34f, 1.0f);
    const glm::vec4 TOMB_EDGE(0.38f, 0.37f, 0.48f, 1.0f);
    const glm::vec4 TOMB_MARK(0.52f, 0.50f, 0.60f, 1.0f);
    const glm::vec4 GRASS_COLOR(0.18f, 0.30f, 0.26f, 1.0f);
    const glm::vec4 SIGIL_COLOR(0.92f, 0.78f, 0.42f, 0.75f);

    // ---------------- 网格构建 ----------------
    struct MeshBuilder {
        std::vector<float> vertices;  // 交错：位置 xyz + 颜色 rgba
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

        void AddTriangle3(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c,
                          const glm::vec4& ca, const glm::vec4& cb, const glm::vec4& cc, float z)
        {
            const uint32_t base = AddVertex(a, z, ca);
            AddVertex(b, z, cb);
            AddVertex(c, z, cc);
            indices.push_back(base);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
        }

        void AddPolygon(const std::vector<glm::vec2>& points, const glm::vec4& color, float z = 0.0f)
        {
            if (points.size() < 3) {
                return;
            }
            const uint32_t base = AddVertex(points[0], z, color);
            for (size_t i = 1; i < points.size(); ++i) {
                AddVertex(points[i], z, color);
            }
            for (uint32_t i = 1; i + 1 < static_cast<uint32_t>(points.size()); ++i) {
                indices.push_back(base);
                indices.push_back(base + i);
                indices.push_back(base + i + 1);
            }
        }

        void AddPolygon(std::initializer_list<glm::vec2> points, const glm::vec4& color, float z = 0.0f)
        {
            AddPolygon(std::vector<glm::vec2>(points), color, z);
        }

        void AddQuad(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c, const glm::vec2& d,
                     const glm::vec4& color, float z = 0.0f)
        {
            const uint32_t base = AddVertex(a, z, color);
            AddVertex(b, z, color);
            AddVertex(c, z, color);
            AddVertex(d, z, color);
            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 0);
            indices.push_back(base + 2);
            indices.push_back(base + 3);
        }

        // 四角不同颜色的方片（渐变）
        void AddGradientQuad(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c, const glm::vec2& d,
                             const glm::vec4& ca, const glm::vec4& cb, const glm::vec4& cc, const glm::vec4& cd,
                             float z = 0.0f)
        {
            const uint32_t base = AddVertex(a, z, ca);
            AddVertex(b, z, cb);
            AddVertex(c, z, cc);
            AddVertex(d, z, cd);
            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 0);
            indices.push_back(base + 2);
            indices.push_back(base + 3);
        }

        void AddRect(const glm::vec2& center, const glm::vec2& half, const glm::vec4& color, float z = 0.0f)
        {
            AddQuad(center + glm::vec2(-half.x, half.y), center + half,
                    center + glm::vec2(half.x, -half.y), center - half, color, z);
        }

        void AddDiamond(const glm::vec2& center, float halfWidth, float halfHeight,
                        const glm::vec4& color, float z = 0.0f)
        {
            AddQuad(center + glm::vec2(0.0f, halfHeight), center + glm::vec2(halfWidth, 0.0f),
                    center + glm::vec2(0.0f, -halfHeight), center + glm::vec2(-halfWidth, 0.0f), color, z);
        }

        void AddCircle(const glm::vec2& center, float radius, int segments, const glm::vec4& color, float z = 0.0f)
        {
            const uint32_t base = AddVertex(center, z, color);
            for (int i = 0; i <= segments; ++i) {
                const float angle = TAU * static_cast<float>(i) / static_cast<float>(segments);
                AddVertex(center + glm::vec2(std::cos(angle), std::sin(angle)) * radius, z, color);
            }
            for (int i = 0; i < segments; ++i) {
                indices.push_back(base);
                indices.push_back(base + 1 + i);
                indices.push_back(base + 2 + i);
            }
        }

        // 环带：外圈 / 内圈同角度成对生成三角形
        void AddRing(const glm::vec2& center, float innerRadius, float outerRadius, int segments,
                     const glm::vec4& color, float z = 0.0f)
        {
            const uint32_t base = static_cast<uint32_t>(vertices.size() / 7);
            for (int i = 0; i <= segments; ++i) {
                const float angle = TAU * static_cast<float>(i) / static_cast<float>(segments);
                const glm::vec2 dir(std::cos(angle), std::sin(angle));
                AddVertex(center + dir * outerRadius, z, color);
                AddVertex(center + dir * innerRadius, z, color);
            }
            for (int i = 0; i < segments; ++i) {
                const uint32_t o0 = base + static_cast<uint32_t>(i) * 2;
                const uint32_t i0 = o0 + 1;
                const uint32_t o1 = o0 + 2;
                const uint32_t i1 = o0 + 3;
                indices.push_back(o0);
                indices.push_back(i0);
                indices.push_back(o1);
                indices.push_back(o1);
                indices.push_back(i0);
                indices.push_back(i1);
            }
        }

        // 弯月形刀刃：外弧与内弧之间的带子，两端收成尖角
        void AddCrescent(float outerRadius, float innerRadius, float fromDeg, float toDeg, int segments,
                         const glm::vec4& color, float z = 0.0f)
        {
            const uint32_t base = static_cast<uint32_t>(vertices.size() / 7);
            const float from = fromDeg * PI / 180.0f;
            const float to = toDeg * PI / 180.0f;
            for (int i = 0; i <= segments; ++i) {
                const float t = static_cast<float>(i) / static_cast<float>(segments);
                const float angle = from + (to - from) * t;
                const float pinch = std::sin(t * PI);  // 两端厚度归零
                const glm::vec2 dir(std::sin(angle), std::cos(angle));
                AddVertex(dir * outerRadius, z, color);
                AddVertex(dir * (outerRadius - (outerRadius - innerRadius) * pinch), z, color);
            }
            for (int i = 0; i < segments; ++i) {
                const uint32_t o0 = base + static_cast<uint32_t>(i) * 2;
                const uint32_t i0 = o0 + 1;
                const uint32_t o1 = o0 + 2;
                const uint32_t i1 = o0 + 3;
                indices.push_back(o0);
                indices.push_back(i0);
                indices.push_back(o1);
                indices.push_back(o1);
                indices.push_back(i0);
                indices.push_back(i1);
            }
        }

        void AddLine(const glm::vec2& a, const glm::vec2& b, const glm::vec4& color, float z = 0.0f)
        {
            AddVertex(a, z, color);
            AddVertex(b, z, color);
        }
    };

    Ref<VertexArray> BuildMesh(MeshBuilder& builder)
    {
        Ref<VertexArray> vertexArray = VertexArray::Create();

        std::vector<float> vertices = std::move(builder.vertices);
        builder.vertices.clear();

        Ref<VertexBuffer> vertexBuffer = VertexBuffer::Create(
            vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(float)));
        vertexBuffer->SetLayout({
            { ShaderDataType::Float3, "a_Position" },
            { ShaderDataType::Float4, "a_Color" },
        });
        vertexArray->AddVertexBuffer(vertexBuffer);
        vertexArray->SetIndexBuffer(IndexBuffer::Create(
            builder.indices.data(), static_cast<uint32_t>(builder.indices.size())));
        return vertexArray;
    }

    // 只含顶点的线网格（RenderCommand::DrawLines 走 glDrawArrays，不需要索引缓冲）
    Ref<VertexArray> BuildLineMesh(MeshBuilder& builder, uint32_t& vertexCount)
    {
        Ref<VertexArray> vertexArray = VertexArray::Create();
        std::vector<float> vertices = std::move(builder.vertices);
        builder.vertices.clear();
        vertexCount = static_cast<uint32_t>(vertices.size() / 7);

        Ref<VertexBuffer> vertexBuffer = VertexBuffer::Create(
            vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(float)));
        vertexBuffer->SetLayout({
            { ShaderDataType::Float3, "a_Position" },
            { ShaderDataType::Float4, "a_Color" },
        });
        vertexArray->AddVertexBuffer(vertexBuffer);
        return vertexArray;
    }

    // ---------------- 角色 ----------------
    // ---------------- 角色：四个角色各一套轮廓，靠剪影就能分辨 ----------------
    // 约定：局部坐标约 ±0.6，绘制时统一 Scale(64)；原点在躯干中心。

    // 守夜人：重甲 + 圆盾 + 短锤（最宽最厚重）
    Ref<VertexArray> CreatePlayerWatchman()
    {
        MeshBuilder builder;
        // 长袍下摆（宽）
        builder.AddPolygon({
            { 0.00f, 0.12f }, { 0.30f, 0.00f }, { 0.36f, -0.46f }, { 0.18f, -0.60f },
            { -0.18f, -0.60f }, { -0.36f, -0.46f }, { -0.30f, 0.00f },
        }, WATCH_DARK);
        // 胸甲
        builder.AddPolygon({
            { 0.00f, 0.22f }, { 0.26f, 0.08f }, { 0.22f, -0.32f }, { 0.00f, -0.42f },
            { -0.22f, -0.32f }, { -0.26f, 0.08f },
        }, WATCH_STEEL, 0.02f);
        // 腰带
        builder.AddRect({ 0.00f, -0.20f }, { 0.24f, 0.035f }, WATCH_GOLD, 0.03f);
        // 肩甲（左右：金边 + 钢面）
        for (int side = -1; side <= 1; side += 2) {
            const float x = 0.265f * static_cast<float>(side);
            builder.AddCircle({ x, 0.10f }, 0.115f, 14, WATCH_GOLD, 0.03f);
            builder.AddCircle({ x, 0.10f }, 0.088f, 12, WATCH_STEEL, 0.04f);
        }
        // 头盔 + 面甲缝 + 盔顶羽饰
        builder.AddCircle({ 0.00f, 0.34f }, 0.185f, 20, WATCH_STEEL, 0.05f);
        builder.AddRect({ 0.00f, 0.30f }, { 0.125f, 0.052f }, PLAYER_VOID, 0.06f);
        builder.AddRect({ 0.00f, 0.315f }, { 0.100f, 0.014f }, WATCH_GOLD, 0.07f);
        builder.AddTriangle({ 0.00f, 0.52f }, { 0.055f, 0.62f }, { -0.055f, 0.62f }, WATCH_GOLD, 0.05f);
        // 圆盾（左手）：钢面 + 金环 + 中心宝石
        builder.AddCircle({ -0.34f, -0.02f }, 0.205f, 22, WATCH_STEEL, 0.08f);
        builder.AddRing({ -0.34f, -0.02f }, 0.150f, 0.190f, 22, WATCH_GOLD, 0.09f);
        builder.AddCircle({ -0.34f, -0.02f }, 0.055f, 12, CRYSTAL, 0.10f);
        // 短锤（右手）
        builder.AddPolygon({
            { 0.30f, -0.30f }, { 0.345f, -0.28f }, { 0.400f, 0.10f }, { 0.355f, 0.12f },
        }, HUNTER_LEATHER, 0.04f);
        builder.AddRect({ 0.400f, 0.16f }, { 0.085f, 0.065f }, WATCH_STEEL, 0.05f);
        return BuildMesh(builder);
    }

    // 猎手：尖兜帽 + 短斗篷 + 长弓（细长、墨绿）
    Ref<VertexArray> CreatePlayerHunter()
    {
        MeshBuilder builder;
        // 斗篷（下摆略外扩，别收成尖）
        builder.AddPolygon({
            { 0.00f, 0.16f }, { 0.22f, 0.04f }, { 0.20f, -0.32f }, { 0.13f, -0.60f },
            { -0.13f, -0.60f }, { -0.20f, -0.32f }, { -0.22f, 0.04f },
        }, HUNTER_CLOAK);
        // 内衬
        builder.AddPolygon({
            { 0.00f, 0.04f }, { 0.11f, -0.02f }, { 0.09f, -0.46f },
            { -0.09f, -0.46f }, { -0.11f, -0.02f },
        }, HUNTER_DARK, 0.01f);
        // 斜挎皮带
        builder.AddPolygon({
            { -0.15f, 0.10f }, { 0.15f, -0.14f }, { 0.15f, -0.22f }, { -0.15f, 0.02f },
        }, HUNTER_LEATHER, 0.02f);
        // 箭袋放在**左侧背后**，给右边的弓让位
        builder.AddRect({ -0.22f, -0.02f }, { 0.050f, 0.170f }, HUNTER_LEATHER, 0.02f);
        builder.AddTriangle({ -0.22f, 0.19f }, { -0.18f, 0.30f }, { -0.26f, 0.30f }, HUNTER_GLOW, 0.02f);
        // 尖兜帽 + 脸影 + 两点眼光
        builder.AddPolygon({
            { 0.00f, 0.58f }, { 0.17f, 0.30f }, { 0.13f, 0.10f }, { -0.13f, 0.10f }, { -0.17f, 0.30f },
        }, HUNTER_DARK, 0.04f);
        builder.AddCircle({ 0.00f, 0.27f }, 0.125f, 16, PLAYER_VOID, 0.05f);
        builder.AddCircle({ -0.050f, 0.27f }, 0.022f, 8, HUNTER_GLOW, 0.06f);
        builder.AddCircle({ 0.050f, 0.27f }, 0.022f, 8, HUNTER_GLOW, 0.06f);
        // 长弓（右手侧）：向右弓起的弧形弓臂 + 弦
        const glm::vec2 bowHand(0.30f, -0.04f);
        constexpr int BOW_SEGMENTS = 8;
        glm::vec2 outer[BOW_SEGMENTS + 1];
        glm::vec2 inner[BOW_SEGMENTS + 1];
        for (int i = 0; i <= BOW_SEGMENTS; ++i) {
            const float angle = -1.15f + 2.30f * static_cast<float>(i) / static_cast<float>(BOW_SEGMENTS);
            const glm::vec2 dir(std::cos(angle), std::sin(angle));
            outer[i] = bowHand + glm::vec2(0.100f * dir.x, 0.350f * dir.y);
            inner[i] = bowHand + glm::vec2(0.042f * dir.x, 0.330f * dir.y);
        }
        for (int i = 0; i < BOW_SEGMENTS; ++i) {
            builder.AddQuad(outer[i], outer[i + 1], inner[i + 1], inner[i], HUNTER_LEATHER, 0.03f);
        }
        // 弓弦：两端尖之间的弦线（细、暗）
        builder.AddRect(bowHand + glm::vec2(0.040f, 0.0f), { 0.007f, 0.315f }, HUNTER_DARK, 0.03f);
        return BuildMesh(builder);
    }

    // 修女：头巾 + 米白长袍 + 十字杖（圆润、亮）
    Ref<VertexArray> CreatePlayerSister()
    {
        MeshBuilder builder;
        // 长袍
        builder.AddPolygon({
            { 0.00f, 0.16f }, { 0.26f, 0.02f }, { 0.34f, -0.50f }, { 0.16f, -0.60f },
            { -0.16f, -0.60f }, { -0.34f, -0.50f }, { -0.26f, 0.02f },
        }, SISTER_ROBE);
        // 披肩（暗一档）+ 腰带
        builder.AddPolygon({
            { 0.00f, 0.14f }, { 0.20f, 0.06f }, { 0.16f, -0.22f }, { -0.16f, -0.22f }, { -0.20f, 0.06f },
        }, SISTER_SHADE, 0.02f);
        builder.AddRect({ 0.00f, -0.26f }, { 0.22f, 0.030f }, SISTER_GOLD, 0.03f);
        // 胸前十字
        builder.AddRect({ 0.00f, 0.02f }, { 0.020f, 0.075f }, SISTER_GOLD, 0.05f);
        builder.AddRect({ 0.00f, 0.06f }, { 0.060f, 0.020f }, SISTER_GOLD, 0.05f);
        // 头巾：外圈 + 两侧垂布 + 脸影 + 眼睛
        builder.AddCircle({ 0.00f, 0.30f }, 0.205f, 20, SISTER_ROBE, 0.04f);
        builder.AddPolygon({
            { -0.20f, 0.32f }, { -0.14f, 0.00f }, { -0.03f, -0.03f }, { -0.09f, 0.32f },
        }, SISTER_ROBE, 0.03f);
        builder.AddPolygon({
            { 0.20f, 0.32f }, { 0.14f, 0.00f }, { 0.03f, -0.03f }, { 0.09f, 0.32f },
        }, SISTER_ROBE, 0.03f);
        builder.AddCircle({ 0.00f, 0.28f }, 0.120f, 16, PLAYER_VOID, 0.05f);
        builder.AddCircle({ -0.045f, 0.29f }, 0.020f, 8, SISTER_GOLD, 0.06f);
        builder.AddCircle({ 0.045f, 0.29f }, 0.020f, 8, SISTER_GOLD, 0.06f);
        // 十字杖（右手）
        builder.AddRect({ 0.34f, -0.08f }, { 0.022f, 0.420f }, SISTER_GOLD, 0.03f);
        builder.AddRect({ 0.34f, 0.30f }, { 0.090f, 0.022f }, SISTER_GOLD, 0.04f);
        builder.AddCircle({ 0.34f, 0.30f }, 0.050f, 12, CRYSTAL, 0.05f);
        return BuildMesh(builder);
    }

    // 术士：尖帽 + 紫黑长袍 + 悬浮法球（最瘦、下摆收尖）
    Ref<VertexArray> CreatePlayerWarlock()
    {
        MeshBuilder builder;
        // 长袍（下摆尖角）
        builder.AddPolygon({
            { 0.00f, 0.18f }, { 0.27f, 0.02f }, { 0.30f, -0.44f }, { 0.00f, -0.64f },
            { -0.30f, -0.44f }, { -0.27f, 0.02f },
        }, WARLOCK_ROBE);
        // 前襟暗缝（细长一条，别做成整块菱形，否则下摆看着像个洞）
        builder.AddPolygon({
            { 0.00f, -0.14f }, { 0.075f, -0.30f }, { 0.00f, -0.58f }, { -0.075f, -0.30f },
        }, WARLOCK_DARK, 0.01f);
        builder.AddRect({ 0.00f, -0.16f }, { 0.22f, 0.028f }, WARLOCK_GLOW, 0.02f);
        // 帽檐（扁菱形，亮一档）+ 帽尖 + 帽带
        builder.AddDiamond({ 0.00f, 0.36f }, 0.340f, 0.085f, WARLOCK_ROBE, 0.04f);
        builder.AddTriangle({ 0.00f, 0.68f }, { 0.150f, 0.34f }, { -0.150f, 0.34f }, WARLOCK_DARK, 0.05f);
        builder.AddRect({ 0.00f, 0.38f }, { 0.130f, 0.018f }, WARLOCK_GLOW, 0.06f);
        // 脸影 + 紫光眼
        builder.AddCircle({ 0.00f, 0.25f }, 0.125f, 16, PLAYER_VOID, 0.06f);
        builder.AddCircle({ -0.048f, 0.25f }, 0.024f, 8, WARLOCK_GLOW, 0.07f);
        builder.AddCircle({ 0.048f, 0.25f }, 0.024f, 8, WARLOCK_GLOW, 0.07f);
        // 悬浮法球（右手侧）+ 短杖
        builder.AddRect({ 0.30f, -0.06f }, { 0.020f, 0.240f }, WARLOCK_DARK, 0.03f);
        builder.AddRing({ 0.34f, 0.22f }, 0.135f, 0.170f, 22, WARLOCK_GLOW, 0.08f);
        builder.AddCircle({ 0.34f, 0.22f }, 0.110f, 18, WARLOCK_DARK, 0.08f);
        builder.AddCircle({ 0.34f, 0.22f }, 0.070f, 16, CRYSTAL, 0.09f);
        return BuildMesh(builder);
    }

    Ref<VertexArray> CreateBat()
    {
        MeshBuilder builder;
        builder.AddPolygon({ { -0.07f, 0.12f }, { -0.52f, 0.36f }, { -0.44f, -0.04f }, { -0.16f, -0.16f } },
                           BAT_WING);
        builder.AddPolygon({ { 0.07f, 0.12f }, { 0.16f, -0.16f }, { 0.44f, -0.04f }, { 0.52f, 0.36f } },
                           BAT_WING);
        builder.AddTriangle({ 0.0f, 0.24f }, { 0.065f, 0.42f }, { 0.11f, 0.20f }, BAT_BODY, 0.01f);
        builder.AddTriangle({ 0.0f, 0.24f }, { -0.11f, 0.20f }, { -0.065f, 0.42f }, BAT_BODY, 0.01f);
        builder.AddPolygon({ { 0.0f, 0.26f }, { 0.11f, 0.04f }, { 0.0f, -0.28f }, { -0.11f, 0.04f } },
                           BAT_BODY, 0.02f);
        builder.AddCircle({ -0.05f, 0.10f }, 0.030f, 8, BAT_EYE, 0.03f);
        builder.AddCircle({ 0.05f, 0.10f }, 0.030f, 8, BAT_EYE, 0.03f);
        return BuildMesh(builder);
    }

    Ref<VertexArray> CreateGhoul()
    {
        MeshBuilder builder;
        builder.AddPolygon({
            { 0.00f, -0.44f }, { 0.30f, -0.26f }, { 0.36f, 0.16f }, { 0.16f, 0.42f },
            { -0.16f, 0.42f }, { -0.36f, 0.16f }, { -0.30f, -0.26f },
        }, GHOUL_BODY);
        // 前伸的双臂
        builder.AddPolygon({ { 0.28f, 0.20f }, { 0.54f, -0.04f }, { 0.46f, -0.18f }, { 0.24f, -0.02f } },
                           GHOUL_DARK, 0.01f);
        builder.AddPolygon({ { -0.28f, 0.20f }, { -0.24f, -0.02f }, { -0.46f, -0.18f }, { -0.54f, -0.04f } },
                           GHOUL_DARK, 0.01f);
        // 破烂下摆
        builder.AddTriangle({ 0.0f, -0.40f }, { 0.16f, -0.40f }, { 0.08f, -0.60f }, GHOUL_DARK, 0.02f);
        builder.AddTriangle({ 0.0f, -0.40f }, { -0.08f, -0.60f }, { -0.16f, -0.40f }, GHOUL_DARK, 0.02f);
        // 眼与口
        builder.AddCircle({ -0.115f, 0.18f }, 0.052f, 10, GHOUL_EYE, 0.03f);
        builder.AddCircle({ 0.115f, 0.18f }, 0.052f, 10, GHOUL_EYE, 0.03f);
        builder.AddRect({ 0.0f, -0.04f }, { 0.10f, 0.030f }, PLAYER_VOID, 0.03f);
        builder.AddTriangle({ 0.0f, 0.02f }, { 0.03f, -0.10f }, { -0.03f, -0.10f }, GHOUL_EYE, 0.02f);
        return BuildMesh(builder);
    }

    Ref<VertexArray> CreateWraith()
    {
        MeshBuilder builder;
        // 破碎的尾摆
        builder.AddPolygon({ { 0.0f, -0.20f }, { 0.26f, -0.44f }, { 0.14f, -0.62f }, { 0.0f, -0.50f },
                             { -0.14f, -0.62f }, { -0.26f, -0.44f } }, WRAITH_TAIL, 0.0f);
        // 半透明外壳
        builder.AddCircle({ 0.0f, 0.04f }, 0.40f, 26, WRAITH_SHELL, 0.01f);
        builder.AddCircle({ 0.0f, 0.04f }, 0.28f, 22, PLAYER_VOID, 0.02f);
        builder.AddCircle({ 0.0f, 0.04f }, 0.22f, 20, WRAITH_CORE, 0.03f);
        builder.AddCircle({ -0.085f, 0.08f }, 0.048f, 10, PLAYER_VOID, 0.04f);
        builder.AddCircle({ 0.085f, 0.08f }, 0.048f, 10, PLAYER_VOID, 0.04f);
        builder.AddRing({ 0.0f, 0.04f }, 0.44f, 0.48f, 26, WRAITH_SHELL, 0.0f);
        return BuildMesh(builder);
    }

    Ref<VertexArray> CreateBrute()
    {
        MeshBuilder builder;
        builder.AddPolygon({
            { 0.00f, -0.48f }, { 0.36f, -0.30f }, { 0.42f, 0.14f }, { 0.22f, 0.46f },
            { -0.22f, 0.46f }, { -0.42f, 0.14f }, { -0.36f, -0.30f },
        }, BRUTE_BODY);
        // 肩甲
        builder.AddPolygon({ { 0.28f, 0.30f }, { 0.60f, 0.12f }, { 0.52f, -0.10f }, { 0.26f, 0.04f } },
                           BRUTE_METAL, 0.01f);
        builder.AddPolygon({ { -0.28f, 0.30f }, { -0.26f, 0.04f }, { -0.52f, -0.10f }, { -0.60f, 0.12f } },
                           BRUTE_METAL, 0.01f);
        // 胸口熔核
        builder.AddCircle({ 0.0f, 0.10f }, 0.13f, 16, BRUTE_CORE, 0.03f);
        builder.AddCircle({ 0.0f, 0.10f }, 0.06f, 12, WHITE, 0.04f);
        // 头与眼
        builder.AddPolygon({ { 0.0f, 0.52f }, { 0.16f, 0.36f }, { 0.0f, 0.22f }, { -0.16f, 0.36f } },
                           BRUTE_METAL, 0.02f);
        builder.AddCircle({ -0.06f, 0.38f }, 0.035f, 8, BRUTE_CORE, 0.03f);
        builder.AddCircle({ 0.06f, 0.38f }, 0.035f, 8, BRUTE_CORE, 0.03f);
        return BuildMesh(builder);
    }

    Ref<VertexArray> CreateBoss()
    {
        MeshBuilder builder;
        // 破损斗篷
        builder.AddPolygon({
            { 0.00f, -0.60f }, { 0.48f, -0.42f }, { 0.58f, 0.12f }, { 0.30f, 0.46f },
            { -0.30f, 0.46f }, { -0.58f, 0.12f }, { -0.48f, -0.42f },
        }, BOSS_CLOAK);
        // 骷髅面甲
        builder.AddPolygon({
            { 0.00f, -0.32f }, { 0.30f, -0.12f }, { 0.34f, 0.26f }, { 0.16f, 0.46f },
            { -0.16f, 0.46f }, { -0.34f, 0.26f }, { -0.30f, -0.12f },
        }, BOSS_BONE, 0.01f);
        // 双角
        builder.AddTriangle({ 0.27f, 0.36f }, { 0.60f, 0.64f }, { 0.35f, 0.16f }, BOSS_HORN, 0.02f);
        builder.AddTriangle({ -0.27f, 0.36f }, { -0.35f, 0.16f }, { -0.60f, 0.64f }, BOSS_HORN, 0.02f);
        // 王冠尖刺
        for (int i = -1; i <= 1; ++i) {
            const float x = static_cast<float>(i) * 0.13f;
            builder.AddTriangle({ x - 0.06f, 0.44f }, { x, 0.60f }, { x + 0.06f, 0.44f }, BOSS_HORN, 0.03f);
        }
        // 三只眼
        builder.AddCircle({ -0.14f, 0.16f }, 0.075f, 12, BOSS_EYE, 0.04f);
        builder.AddCircle({ 0.14f, 0.16f }, 0.075f, 12, BOSS_EYE, 0.04f);
        builder.AddCircle({ 0.0f, 0.34f }, 0.050f, 10, BOSS_EYE, 0.04f);
        // 獠牙
        builder.AddTriangle({ -0.10f, -0.08f }, { -0.04f, -0.08f }, { -0.07f, -0.20f }, BOSS_BONE, 0.03f);
        builder.AddTriangle({ 0.04f, -0.08f }, { 0.10f, -0.08f }, { 0.07f, -0.20f }, BOSS_BONE, 0.03f);
        return BuildMesh(builder);
    }

    // ---------------- 投射物 / 掉落物 ----------------
    Ref<VertexArray> CreateBlade()
    {
        MeshBuilder builder;
        builder.AddCrescent(0.50f, 0.26f, -78.0f, 78.0f, 18, BLADE_OUTER, 0.0f);
        builder.AddCrescent(0.50f, 0.40f, -66.0f, 66.0f, 16, BLADE_INNER, 0.01f);
        return BuildMesh(builder);
    }

    Ref<VertexArray> CreateScythe()
    {
        MeshBuilder builder;
        builder.AddCrescent(0.52f, 0.20f, -108.0f, 108.0f, 22, SCYTHE_OUTER, 0.0f);
        builder.AddCrescent(0.52f, 0.40f, -96.0f, 96.0f, 18, SCYTHE_INNER, 0.01f);
        // 短柄
        builder.AddPolygon({ { -0.04f, -0.06f }, { 0.04f, -0.06f }, { 0.03f, -0.46f }, { -0.03f, -0.46f } },
                           SCYTHE_OUTER, 0.02f);
        return BuildMesh(builder);
    }

    // 金币掉落物：暗金边 + 亮金盘面 + 中心凹刻 + 左上高光。
    // 整枚按 age 缓慢旋转，高光跟着打圈，远看就是一枚在地上反光的钱币
    Ref<VertexArray> CreateCoin()
    {
        MeshBuilder builder;
        builder.AddCircle({ 0.0f, 0.0f }, 0.46f, 28, COIN_RIM, 0.0f);
        builder.AddCircle({ 0.0f, 0.0f }, 0.36f, 28, COIN_FACE, 0.01f);
        builder.AddDiamond({ 0.0f, 0.0f }, 0.10f, 0.18f, COIN_MARK, 0.02f);
        builder.AddDiamond({ -0.17f, 0.16f }, 0.045f, 0.085f, COIN_SHINE, 0.03f);
        return BuildMesh(builder);
    }

    // ---------------- 通用特效 ----------------
    Ref<VertexArray> CreateDisc()
    {
        MeshBuilder builder;
        builder.AddCircle({ 0.0f, 0.0f }, 1.0f, 64, WHITE);
        return BuildMesh(builder);
    }

    Ref<VertexArray> CreateRing()
    {
        MeshBuilder builder;
        builder.AddRing({ 0.0f, 0.0f }, 0.90f, 1.0f, 72, WHITE);
        return BuildMesh(builder);
    }

    Ref<VertexArray> CreateQuad()
    {
        MeshBuilder builder;
        builder.AddRect({ 0.0f, 0.0f }, { 0.5f, 0.5f }, WHITE);
        return BuildMesh(builder);
    }

    // 单边渐隐条：局部 +Y 边不透明、-Y 边全透明（用于暗角 / 光柱）
    Ref<VertexArray> CreateVignette()
    {
        MeshBuilder builder;
        const glm::vec4 solid(1.0f, 1.0f, 1.0f, 1.0f);
        const glm::vec4 clear(1.0f, 1.0f, 1.0f, 0.0f);
        builder.AddGradientQuad({ -0.5f, 0.5f }, { 0.5f, 0.5f }, { 0.5f, -0.5f }, { -0.5f, -0.5f },
                                solid, solid, clear, clear, 0.0f);
        return BuildMesh(builder);
    }

    // 径向暗角：中心全透明、边缘不透明（配黑色 tint 即得到无接缝的压暗）
    Ref<VertexArray> CreateVignetteDisc()
    {
        MeshBuilder builder;
        constexpr int SEGMENTS = 72;
        // 多段缓入的 alpha 曲线：避免线性渐变在起点/终点形成肉眼可见的「椭圆边界」（马赫带）
        constexpr float RADII[5] = { 0.35f, 0.55f, 0.75f, 0.90f, 1.00f };
        constexpr float ALPHAS[5] = { 0.00f, 0.10f, 0.30f, 0.62f, 1.00f };
        constexpr int RINGS = 5;

        const uint32_t base = static_cast<uint32_t>(builder.vertices.size() / 7);
        for (int ring = 0; ring < RINGS; ++ring) {
            const glm::vec4 color(1.0f, 1.0f, 1.0f, ALPHAS[ring]);
            for (int i = 0; i <= SEGMENTS; ++i) {
                const float angle = TAU * static_cast<float>(i) / static_cast<float>(SEGMENTS);
                builder.AddVertex(glm::vec2(std::cos(angle), std::sin(angle)) * RADII[ring], 0.0f, color);
            }
        }
        for (int ring = 0; ring < RINGS - 1; ++ring) {
            const uint32_t inner = base + static_cast<uint32_t>(ring) * (SEGMENTS + 1);
            const uint32_t outer = inner + SEGMENTS + 1;
            for (int i = 0; i < SEGMENTS; ++i) {
                const uint32_t a = inner + static_cast<uint32_t>(i);
                const uint32_t b = outer + static_cast<uint32_t>(i);
                builder.indices.push_back(a);
                builder.indices.push_back(b);
                builder.indices.push_back(a + 1);
                builder.indices.push_back(a + 1);
                builder.indices.push_back(b);
                builder.indices.push_back(b + 1);
            }
        }
        return BuildMesh(builder);
    }

    // 闪电柱：向上的锯齿光带，底端最亮、顶端淡出
    Ref<VertexArray> CreateBolt()
    {
        MeshBuilder builder;
        constexpr int SEGMENTS = 9;
        float prevCenter = 0.0f;
        float prevHalf = 0.085f;
        for (int i = 0; i <= SEGMENTS; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(SEGMENTS);
            const float y = -0.5f + t;
            const float jitter = 0.055f * std::sin(t * 17.0f) + 0.035f * std::sin(t * 41.0f + 1.7f);
            const float center = jitter;
            const float half = 0.085f * (1.0f - t) + 0.012f;
            if (i == 0) {
                prevCenter = center;
                prevHalf = half;
                continue;
            }
            const glm::vec4 topColor(0.70f, 0.85f, 1.0f, 0.0f);
            const glm::vec4 bottomColor(0.85f, 0.95f, 1.0f, 0.95f);
            const float tPrev = static_cast<float>(i - 1) / static_cast<float>(SEGMENTS);
            const float yPrev = -0.5f + tPrev;
            const glm::vec4 cPrev = glm::mix(bottomColor, topColor, tPrev);
            const glm::vec4 cNow = glm::mix(bottomColor, topColor, t);
            // 外发光带
            builder.AddGradientQuad(
                { prevCenter - prevHalf * 2.4f, yPrev }, { prevCenter + prevHalf * 2.4f, yPrev },
                { center + half * 2.4f, y }, { center - half * 2.4f, y },
                cPrev, cPrev, cNow, cNow, 0.0f);
            // 亮芯
            builder.AddGradientQuad(
                { prevCenter - prevHalf * 0.55f, yPrev }, { prevCenter + prevHalf * 0.55f, yPrev },
                { center + half * 0.55f, y }, { center - half * 0.55f, y },
                glm::vec4(1.0f, 1.0f, 1.0f, cPrev.a), glm::vec4(1.0f, 1.0f, 1.0f, cPrev.a),
                glm::vec4(1.0f, 1.0f, 1.0f, cNow.a), glm::vec4(1.0f, 1.0f, 1.0f, cNow.a), 0.01f);
            prevCenter = center;
            prevHalf = half;
        }
        return BuildMesh(builder);
    }

    // ---------------- 地面装饰 ----------------
    Ref<VertexArray> CreateTombstone()
    {
        MeshBuilder builder;
        // 底座阴影
        builder.AddRect({ 0.0f, -0.50f }, { 0.30f, 0.08f }, glm::vec4(0.0f, 0.0f, 0.0f, 0.35f), 0.0f);
        // 石板
        builder.AddPolygon({
            { -0.24f, -0.46f }, { 0.24f, -0.46f }, { 0.24f, 0.12f }, { 0.15f, 0.34f },
            { 0.0f, 0.42f }, { -0.15f, 0.34f }, { -0.24f, 0.12f },
        }, TOMB_STONE, 0.01f);
        // 受光边
        builder.AddPolygon({ { -0.19f, -0.40f }, { -0.11f, -0.40f }, { -0.11f, 0.18f }, { -0.17f, 0.26f } },
                           TOMB_EDGE, 0.02f);
        // 十字刻痕
        builder.AddRect({ 0.0f, 0.10f }, { 0.026f, 0.17f }, TOMB_MARK, 0.03f);
        builder.AddRect({ 0.0f, 0.14f }, { 0.11f, 0.026f }, TOMB_MARK, 0.03f);
        return BuildMesh(builder);
    }

    Ref<VertexArray> CreateGrass()
    {
        MeshBuilder builder;
        // 低矮的草丛：一撮土 + 五片外扩的草叶
        builder.AddCircle({ 0.0f, -0.30f }, 0.30f, 14, glm::vec4(0.12f, 0.15f, 0.14f, 0.85f), 0.0f);
        builder.AddTriangle({ -0.26f, -0.34f }, { -0.10f, -0.36f }, { -0.34f, 0.26f }, GRASS_COLOR, 0.01f);
        builder.AddTriangle({ -0.14f, -0.34f }, { 0.02f, -0.34f }, { -0.04f, 0.44f }, GRASS_COLOR, 0.01f);
        builder.AddTriangle({ 0.02f, -0.34f }, { 0.16f, -0.34f }, { 0.24f, 0.34f }, GRASS_COLOR, 0.01f);
        builder.AddTriangle({ 0.12f, -0.34f }, { 0.28f, -0.32f }, { 0.38f, 0.18f }, GRASS_COLOR, 0.01f);
        builder.AddTriangle({ -0.22f, -0.34f }, { -0.04f, -0.34f }, { -0.16f, 0.50f }, GRASS_COLOR, 0.02f);
        return BuildMesh(builder);
    }

    // 玩家脚下的法阵（圣光环武器拥有时显示）
    Ref<VertexArray> CreateSigil()
    {
        MeshBuilder builder;
        builder.AddRing({ 0.0f, 0.0f }, 0.82f, 0.86f, 48, SIGIL_COLOR, 0.0f);
        builder.AddRing({ 0.0f, 0.0f }, 0.94f, 0.97f, 48, SIGIL_COLOR, 0.0f);
        for (int i = 0; i < 4; ++i) {
            const float angle = static_cast<float>(i) * PI * 0.5f;
            const glm::vec2 dir(std::cos(angle), std::sin(angle));
            const glm::vec2 tangent(-dir.y, dir.x);
            builder.AddDiamond(dir * 0.72f, 0.045f, 0.075f, SIGIL_COLOR, 0.0f);
            builder.AddQuad(tangent * 0.02f + dir * 0.45f, tangent * 0.09f + dir * 0.45f,
                            tangent * 0.09f + dir * 0.55f, tangent * 0.02f + dir * 0.55f,
                            SIGIL_COLOR, 0.0f);
        }
        return BuildMesh(builder);
    }

    // 世界网格线（静态，绘制时平移对齐到相机）
    Ref<VertexArray> CreateGrid(uint32_t& vertexCount)
    {
        MeshBuilder builder;
        constexpr float SPAN = 3200.0f;
        constexpr float STEP = 120.0f;
        const int count = static_cast<int>(SPAN / STEP);
        for (int i = -count; i <= count; ++i) {
            const float coord = static_cast<float>(i) * STEP;
            const bool major = (i % 5) == 0;
            const glm::vec4 color = major ? glm::vec4(0.34f, 0.28f, 0.52f, 0.40f)
                                          : glm::vec4(0.22f, 0.19f, 0.34f, 0.19f);
            builder.AddLine({ coord, -SPAN }, { coord, SPAN }, color, 0.0f);
            builder.AddLine({ -SPAN, coord }, { SPAN, coord }, color, 0.0f);
        }
        return BuildLineMesh(builder, vertexCount);
    }
}

namespace OiEngine::Survivors {

    void SurvivorsArt::Load()
    {
        playerWatchman = CreatePlayerWatchman();
        playerHunter = CreatePlayerHunter();
        playerSister = CreatePlayerSister();
        playerWarlock = CreatePlayerWarlock();
        bat = CreateBat();
        ghoul = CreateGhoul();
        wraith = CreateWraith();
        brute = CreateBrute();
        boss = CreateBoss();

        blade = CreateBlade();
        scythe = CreateScythe();
        coin = CreateCoin();

        disc = CreateDisc();
        particle = disc;
        ring = CreateRing();
        quad = CreateQuad();
        vignette = CreateVignette();
        vignetteDisc = CreateVignetteDisc();
        bolt = CreateBolt();
        tombstone = CreateTombstone();
        grass = CreateGrass();
        sigil = CreateSigil();

        lineGrid = CreateGrid(lineGridVertexCount);
    }

    void SurvivorsArt::Unload()
    {
        playerWatchman.reset();
        playerHunter.reset();
        playerSister.reset();
        playerWarlock.reset();
        bat.reset();
        ghoul.reset();
        wraith.reset();
        brute.reset();
        boss.reset();
        blade.reset();
        scythe.reset();
        coin.reset();
        particle.reset();
        disc.reset();
        ring.reset();
        quad.reset();
        vignette.reset();
        vignetteDisc.reset();
        bolt.reset();
        tombstone.reset();
        grass.reset();
        sigil.reset();
        lineGrid.reset();
        lineGridVertexCount = 0;
    }
}
