#include "SurvivorsGame.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <format>
#include <string>

#include <glm/gtc/matrix_transform.hpp>

#include "Engine/Core/Input.h"
#include "Engine/Events/KeyEvent.h"

using namespace OiEngine;
using namespace OiEngine::Survivors;

namespace {
    constexpr float PI = 3.14159265358979323846f;

    // 与 SandBox/RenderHelpers.h 相同的矩阵助手，Survivors 独立于 SandBox
    glm::mat4 Translate(const glm::vec3& translation)
    {
        return glm::translate(glm::mat4(1.0f), translation);
    }

    glm::mat4 Scale(const glm::vec3& scale)
    {
        return glm::scale(glm::mat4(1.0f), scale);
    }

    glm::mat4 RotateZ(float radians)
    {
        return glm::rotate(glm::mat4(1.0f), radians, glm::vec3(0.0f, 0.0f, 1.0f));
    }

    glm::mat4 Orthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane)
    {
        return glm::ortho(left, right, bottom, top, nearPlane, farPlane);
    }

    constexpr float TAU = 2.0f * PI;
    constexpr float VIEW_HALF_HEIGHT = 450.0f;
    constexpr float VICTORY_TIME = 180.0f;
    constexpr int MAX_ENEMIES = 420;
    constexpr int MAX_PARTICLES = 700;
    constexpr int MAX_GEMS = 600;
    constexpr int MAX_DAMAGE_NUMBERS = 48;
    constexpr float PLAYER_RADIUS = 20.0f;

    const glm::vec4 WHITE(1.0f, 1.0f, 1.0f, 1.0f);
    const glm::vec4 C_BLADE(0.62f, 0.92f, 1.00f, 1.0f);
    const glm::vec4 C_AURA(1.00f, 0.82f, 0.42f, 1.0f);
    const glm::vec4 C_SCYTHE(0.78f, 0.42f, 1.00f, 1.0f);
    const glm::vec4 C_STORM(0.70f, 0.86f, 1.00f, 1.0f);
    const glm::vec4 C_BLOOD(0.88f, 0.20f, 0.26f, 1.0f);
    const glm::vec4 C_GOLD(1.00f, 0.84f, 0.45f, 1.0f);
    const glm::vec4 C_HEAL(0.55f, 0.95f, 0.70f, 1.0f);
    const glm::vec4 GEM_COLORS[3] = {
        glm::vec4(0.42f, 0.86f, 1.00f, 1.0f),
        glm::vec4(0.48f, 1.00f, 0.62f, 1.0f),
        glm::vec4(0.88f, 0.50f, 1.00f, 1.0f),
    };

    struct WeaponTuning {
        float damage[6];
        float cooldown[6];
        int count[6];
        int pierce[6];
        float radius[6];
    };

    // 月刃：默认武器，索敌直线弹道
    constexpr WeaponTuning BLADE = {
        { 0.0f, 11.0f, 16.0f, 18.0f, 24.0f, 32.0f },
        { 0.0f, 0.58f, 0.54f, 0.50f, 0.46f, 0.40f },
        { 0, 1, 1, 2, 2, 3 },
        { 0, 2, 2, 2, 3, 4 },
        { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    };

    // 圣光环：以玩家为圆心的脉冲伤害
    constexpr WeaponTuning AURA = {
        { 0.0f, 5.0f, 8.0f, 11.0f, 16.0f, 22.0f },
        { 0.0f, 0.52f, 0.50f, 0.46f, 0.42f, 0.36f },
        { 0, 1, 1, 1, 1, 1 },
        { 0, 0, 0, 0, 0, 0 },
        { 0.0f, 120.0f, 138.0f, 156.0f, 178.0f, 205.0f },
    };

    // 回旋镰：飞出后折返，可重复切割
    constexpr WeaponTuning SCYTHE = {
        { 0.0f, 22.0f, 30.0f, 36.0f, 48.0f, 62.0f },
        { 0.0f, 2.60f, 2.40f, 2.20f, 2.00f, 1.80f },
        { 0, 1, 1, 2, 2, 3 },
        { 0, 0, 0, 0, 0, 0 },
        { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    };

    // 雷暴：瞬发落雷
    constexpr WeaponTuning STORM = {
        { 0.0f, 34.0f, 46.0f, 56.0f, 74.0f, 96.0f },
        { 0.0f, 3.00f, 2.80f, 2.60f, 2.40f, 2.20f },
        { 0, 2, 2, 3, 3, 4 },
        { 0, 0, 0, 0, 0, 0 },
        { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    };

    struct EnemyTuning {
        float hp;
        float speed;
        float radius;
        float damage;
        int xp;
    };

    // 敌人速度普遍接近（略低于）玩家，围上来之后就不容易被一条直线甩掉
    constexpr EnemyTuning ENEMY_TUNING[5] = {
        { 10.0f, 238.0f, 15.0f, 2.0f, 2 },      // 蝙蝠
        { 28.0f, 188.0f, 19.0f, 3.0f, 4 },      // 食尸鬼
        { 44.0f, 228.0f, 22.0f, 4.0f, 6 },      // 怨灵
        { 130.0f, 158.0f, 31.0f, 6.0f, 14 },    // 石魔
        { 400.0f, 300.0f, 46.0f, 10.0f, 100 },  // 血月使徒
    };

    const char* const WEAPON_NAMES[4] = { "月刃", "圣光环", "回旋镰", "雷暴" };
    const glm::vec4 WEAPON_COLORS[4] = { C_BLADE, C_AURA, C_SCYTHE, C_STORM };
    const char* const PASSIVE_NAMES[7] = { "力量", "急速", "疾行", "体魄", "磁石", "幸运", "回春" };
    const int PASSIVE_MAX[7] = { 5, 5, 5, 5, 3, 5, 5 };
    const char* const ROMAN[6] = { "", "I", "II", "III", "IV", "V" };

    float Length(const glm::vec2& v)
    {
        return std::sqrt(v.x * v.x + v.y * v.y);
    }

    glm::vec2 Normalize(const glm::vec2& v)
    {
        const float length = Length(v);
        return length > 0.0001f ? v / length : glm::vec2(0.0f, 1.0f);
    }

    bool CircleHit(const glm::vec2& a, float aRadius, const glm::vec2& b, float bRadius)
    {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        const float radiusSum = aRadius + bRadius;
        return dx * dx + dy * dy < radiusSum * radiusSum;
    }

    uint32_t HashCell(int x, int y)
    {
        uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return h ^ (h >> 16);
    }

    float HashUnit(uint32_t value, int shift)
    {
        return static_cast<float>((value >> shift) & 1023u) / 1023.0f;
    }

    std::string FormatTime(float seconds)
    {
        const int total = static_cast<int>(seconds);
        return std::format("{:02d}:{:02d}", total / 60, total % 60);
    }
}

// ============================ 相机 ============================

glm::vec3 FollowCamera::GetPosition() const
{
    return glm::vec3(m_focus.x + m_shake.x, m_focus.y + m_shake.y, 10.0f);
}

glm::mat4 FollowCamera::GetViewMatrix() const
{
    return Translate(glm::vec3(-m_focus.x + m_shake.x, -m_focus.y + m_shake.y, 0.0f));
}

glm::mat4 FollowCamera::GetProjectionMatrix() const
{
    const uint32_t width = Application::Get().GetWindow().GetWidth();
    const uint32_t height = Application::Get().GetWindow().GetHeight();
    const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 16.0f / 9.0f;
    const float halfWidth = VIEW_HALF_HEIGHT * aspect;
    return Orthographic(-halfWidth, halfWidth, -VIEW_HALF_HEIGHT, VIEW_HALF_HEIGHT, -10.0f, 10.0f);
}

// ============================ 空间哈希 ============================

void SurvivorsLayer::EnemyGrid::Build(const std::vector<Enemy>& enemies)
{
    cells.clear();
    for (uint32_t i = 0; i < static_cast<uint32_t>(enemies.size()); ++i) {
        const Enemy& enemy = enemies[i];
        if (!enemy.alive) {
            continue;
        }
        const int cx = static_cast<int>(std::floor(enemy.position.x / CELL_SIZE));
        const int cy = static_cast<int>(std::floor(enemy.position.y / CELL_SIZE));
        cells[Key(cx, cy)].push_back(i);
    }
}

// ============================ 生命周期 ============================

SurvivorsLayer::SurvivorsLayer()
    : Layer("Survivors")
{
}

void SurvivorsLayer::OnAttach()
{
    CreateShaders();
    m_art.Load();
    RenderCommand::SetClearColor(glm::vec4(0.030f, 0.028f, 0.062f, 1.0f));
    ResetRun();
    // 启动先进标题界面；ResetRun 会刷开局蝙蝠，这里清掉，让标题画面保持干净
    m_state = State::Title;
    m_enemies.clear();
    m_gems.clear();
    m_particles.clear();
    m_effects.clear();
    m_grid.cells.clear();
    OI_INFO("Survivors OnAttach: 永夜幸存者已启动");
}

void SurvivorsLayer::OnDetach()
{
    m_art.Unload();
}

void SurvivorsLayer::CreateShaders()
{
    const char* VERTEX_SOURCE = R"(
        #version 450 core

        layout(location = 0) in vec3 a_Position;
        layout(location = 1) in vec4 a_Color;

        layout(std140, binding = 0) uniform Camera {
            mat4 u_Projection;
            mat4 u_View;
            vec4 u_CameraPosition;
        };

        uniform mat4 u_Transform;

        out vec4 v_Color;
        out vec2 v_Local;

        void main()
        {
            gl_Position = u_Projection * u_View * u_Transform * vec4(a_Position, 1.0);
            v_Color = a_Color;
            v_Local = a_Position.xy;
        }
    )";

    const char* FRAGMENT_SOURCE = R"(
        #version 450 core

        layout(location = 0) out vec4 o_Color;

        in vec4 v_Color;
        in vec2 v_Local;

        uniform vec4 u_Tint;
        uniform float u_Falloff;

        void main()
        {
            vec4 color = v_Color * u_Tint;
            if (u_Falloff > 0.0) {
                float distance = length(v_Local);
                color.a *= max(0.0, 1.0 - distance) * u_Falloff;
            }
            if (color.a <= 0.002) {
                discard;
            }
            o_Color = color;
        }
    )";

    m_shader = Shader::Create("Survivors", VERTEX_SOURCE, FRAGMENT_SOURCE);
}

void SurvivorsLayer::InitWisps()
{
    m_wisps.clear();
    for (int i = 0; i < 18; ++i) {
        Wisp wisp;
        const float angle = Rand(0.0f, TAU);
        const float distance = Rand(120.0f, 1100.0f);
        wisp.position = m_playerPosition + glm::vec2(std::cos(angle), std::sin(angle)) * distance;
        const float speed = Rand(6.0f, 18.0f);
        const float drift = Rand(0.0f, TAU);
        wisp.velocity = glm::vec2(std::cos(drift), std::sin(drift)) * speed;
        wisp.radius = Rand(220.0f, 460.0f);
        const glm::vec4 palette[3] = {
            glm::vec4(0.30f, 0.24f, 0.60f, 0.115f),
            glm::vec4(0.16f, 0.40f, 0.50f, 0.090f),
            glm::vec4(0.48f, 0.24f, 0.44f, 0.085f),
        };
        wisp.color = palette[i % 3];
        m_wisps.push_back(wisp);
    }
}

void SurvivorsLayer::ResetRun()
{
    m_state = State::Playing;
    m_time = 0.0f;
    m_kills = 0;
    m_damageDealt = 0.0f;
    m_shake = 0.0f;
    m_bossWarning = 0.0f;

    m_playerPosition = glm::vec2(0.0f, 0.0f);
    m_playerVelocity = glm::vec2(0.0f, 0.0f);

    // 属性：先回到基础值、清零被动，再汇总一次得到本局的初始最终属性
    m_baseStats = BaseStats{};
    m_passive = { 0, 0, 0, 0, 0, 0, 0 };
    m_finalStats = FinalStats{};
    m_recalculatingStats = false;
    m_playerHp = 0.0f;          // 避免 RecalculateStats 里的 clamp 读到上一局的血量
    RecalculateStats();
    m_playerHp = m_playerMaxHp;
    m_hurtCooldown = 2.0f;      // 开局短暂无敌，避免刚点开始就被贴脸的初始蝙蝠打一下
    m_walkPhase = 0.0f;
    m_hurtFlash = 0.0f;
    m_playerAlive = true;

    m_level = 1;
    m_exp = 0;
    m_expNext = ExpNeeded(1);
    m_pendingLevelUps = 0;
    m_levelUpLockout = 0.0f;
    m_weaponLevel = { 1, 0, 0, 0 };
    m_options.clear();
    m_levelFlash = 0.0f;

    m_enemies.clear();
    m_projectiles.clear();
    m_gems.clear();
    m_particles.clear();
    m_effects.clear();
    m_damageNumbers.clear();
    m_grid.cells.clear();
    m_nextEnemyId = 1;

    m_bladeCooldown = 0.20f;
    m_auraCooldown = 0.0f;
    m_scytheCooldown = 0.0f;
    m_stormCooldown = 0.0f;

    m_spawnTimer = 0.6f;
    m_nextBossTime = 60.0f;
    m_bossesSpawned = 0;
    m_bossAlive = false;
    m_bossId = 0;

    m_cameraFocus = m_playerPosition;
    InitWisps();

    // 开局就在玩家附近放几只蝙蝠：第一波击杀正好把等级顶到 2，立刻展示升级卡
    for (int i = 0; i < 4; ++i) {
        const float angle = TAU * static_cast<float>(i) / 4.0f + Rand(-0.35f, 0.35f);
        SpawnEnemy(EnemyType::Bat, m_playerPosition +
                   glm::vec2(std::cos(angle), std::sin(angle)) * Rand(160.0f, 250.0f));
    }
}

// ============================ 随机与数值 ============================

float SurvivorsLayer::Rand(float minValue, float maxValue)
{
    return std::uniform_real_distribution<float>(minValue, maxValue)(m_rng);
}

int SurvivorsLayer::RandInt(int minValue, int maxValue)
{
    if (maxValue <= minValue) {
        return minValue;
    }
    return std::uniform_int_distribution<int>(minValue, maxValue)(m_rng);
}

void SurvivorsLayer::RecalculateStats()
{
    // 重入保护：属性汇总里如果又去读「最终属性」并触发重算，会形成递归。
    // 正确做法是只读 m_baseStats + 被动/武器等级，一次性算出 FinalStats。
    if (m_recalculatingStats) {
        OI_ERROR("Survivors: RecalculateStats 重入，忽略本次调用（属性汇总不能引用最终属性）");
        return;
    }
    m_recalculatingStats = true;

    const auto level = [this](PassiveId id) {
        return static_cast<float>(m_passive[static_cast<int>(id)]);
    };

    FinalStats stats(m_baseStats);
    stats.maxHp = m_baseStats.maxHp + 25.0f * level(PassiveId::Vitality);
    stats.moveSpeed = m_baseStats.moveSpeed * (1.0f + 0.07f * level(PassiveId::Swift));
    stats.damageMul = m_baseStats.damageMul * (1.0f + 0.12f * level(PassiveId::Might));
    stats.attackSpeedMul = m_baseStats.attackSpeedMul * (1.0f + 0.10f * level(PassiveId::Haste));
    stats.critChance = m_baseStats.critChance + 0.06f * level(PassiveId::Luck);
    stats.pickupRadius = m_baseStats.pickupRadius * (1.0f + 0.35f * level(PassiveId::Magnet));
    stats.regenPerSecond = m_baseStats.regenPerSecond + 0.4f * level(PassiveId::Regen);

    m_finalStats = stats;
    m_playerMaxHp = m_finalStats.maxHp;
    m_playerHp = std::min(m_playerHp, m_playerMaxHp);

    m_recalculatingStats = false;
}

float SurvivorsLayer::DamageMul() const
{
    return m_finalStats.damageMul;
}

float SurvivorsLayer::AttackSpeedMul() const
{
    return m_finalStats.attackSpeedMul;
}

float SurvivorsLayer::CritChance() const
{
    return m_finalStats.critChance;
}

float SurvivorsLayer::MoveSpeed() const
{
    return m_finalStats.moveSpeed;
}

float SurvivorsLayer::PickupRadius() const
{
    return m_finalStats.pickupRadius;
}

float SurvivorsLayer::MaxHp() const
{
    return m_finalStats.maxHp;
}

int SurvivorsLayer::ExpNeeded(int level)
{
    // 升级节奏：首级约 10 秒，之后每级 12~18 秒一次（实测击杀速度约 5~6 经验/秒）。
    // 弹窗「过于频繁」的主因是这个需求曲线太浅，调它就能整体放慢/加快
    return 40 + level * 8;
}

// ============================ 主循环 ============================

void SurvivorsLayer::OnUpdate()
{
    // 本场景走非 HDR / 非 cluster 路径，显式声明引擎全局开关
    Renderer::SetHDREnabled(false);
    Renderer::SetLightCullingEnabled(false);

    m_viewWidth = Application::Get().GetWindow().GetWidth();
    m_viewHeight = Application::Get().GetWindow().GetHeight();
    m_halfHeight = VIEW_HALF_HEIGHT;
    m_halfWidth = m_viewHeight > 0
        ? VIEW_HALF_HEIGHT * static_cast<float>(m_viewWidth) / static_cast<float>(m_viewHeight)
        : 800.0f;

    const float deltaTime = std::min(Timer::GetDeltaTime(), 0.05f);

    if (m_state == State::Playing) {
        UpdateGame(deltaTime);
    }

    m_hurtFlash = std::max(0.0f, m_hurtFlash - deltaTime * 2.4f);
    m_levelFlash = std::max(0.0f, m_levelFlash - deltaTime * 1.8f);
    m_shake = std::max(0.0f, m_shake - deltaTime * 34.0f);

    UpdateCamera(deltaTime);

    RenderCommand::Clear();
    Renderer::BeginScene(m_camera);
    RenderWorld();
    Renderer::EndScene();
}

void SurvivorsLayer::UpdateGame(float deltaTime)
{
    m_time += deltaTime;

    UpdatePlayer(deltaTime);
    UpdateEnemies(deltaTime);
    UpdateEnemySpawning(deltaTime);
    UpdateWeapons(deltaTime);
    UpdateProjectiles(deltaTime);
    UpdateGems(deltaTime);
    UpdateContactDamage(deltaTime);
    UpdateParticles(deltaTime);
    UpdateEffects(deltaTime);

    std::erase_if(m_enemies, [](const Enemy& enemy) { return !enemy.alive; });
    std::erase_if(m_projectiles, [](const Projectile& projectile) { return !projectile.alive; });
    std::erase_if(m_gems, [](const Gem& gem) { return !gem.alive; });

    const float untilBoss = m_nextBossTime - m_time;
    m_bossWarning = (untilBoss > 0.0f && untilBoss < 6.0f) ? 1.0f - untilBoss / 6.0f : 0.0f;

    if (m_time >= VICTORY_TIME && m_state == State::Playing) {
        m_state = State::Victory;
        m_levelFlash = 1.0f;
        m_shake = 12.0f;
        OI_INFO("Survivors: 玩家撑到了黎明，胜利");
        return;
    }

    // 刚关掉升级面板时给一小段缓冲：避免宝石还在往身上飞、立刻又弹一次
    m_levelUpLockout = std::max(0.0f, m_levelUpLockout - deltaTime);
    if (m_pendingLevelUps > 0 && m_levelUpLockout <= 0.0f && m_state == State::Playing) {
        StartLevelUp();
    }
}

void SurvivorsLayer::UpdatePlayer(float deltaTime)
{
    glm::vec2 input(0.0f, 0.0f);
    if (Input::IsKeyPressed(Key::W) || Input::IsKeyPressed(Key::Up)) {
        input.y += 1.0f;
    }
    if (Input::IsKeyPressed(Key::S) || Input::IsKeyPressed(Key::Down)) {
        input.y -= 1.0f;
    }
    if (Input::IsKeyPressed(Key::A) || Input::IsKeyPressed(Key::Left)) {
        input.x -= 1.0f;
    }
    if (Input::IsKeyPressed(Key::D) || Input::IsKeyPressed(Key::Right)) {
        input.x += 1.0f;
    }
    // 只在真的有方向输入时归一化：Normalize 对零向量会兜底成 (0,1)（供索敌方向使用），
    // 直接拿去归一化会让"松开所有键"变成"一直向上走"
    const float inputLength = Length(input);
    if (inputLength > 0.0001f) {
        input /= inputLength;
    } else {
        input = glm::vec2(0.0f, 0.0f);
    }

    const float speed = MoveSpeed();
    const float blend = 1.0f - std::exp(-deltaTime * 15.0f);
    m_playerVelocity = glm::mix(m_playerVelocity, input * speed, blend);
    m_playerPosition += m_playerVelocity * deltaTime;
    m_walkPhase += deltaTime * (2.0f + Length(m_playerVelocity) * 0.022f);

    m_hurtCooldown = std::max(0.0f, m_hurtCooldown - deltaTime);

    if (m_finalStats.regenPerSecond > 0.0f && m_playerHp < m_playerMaxHp) {
        m_playerHp = std::min(m_playerMaxHp, m_playerHp + m_finalStats.regenPerSecond * deltaTime);
    }
}

void SurvivorsLayer::UpdateEnemies(float deltaTime)
{
    for (Enemy& enemy : m_enemies) {
        if (!enemy.alive) {
            continue;
        }
        enemy.age += deltaTime;
        enemy.hitFlash = std::max(0.0f, enemy.hitFlash - deltaTime * 6.5f);

        glm::vec2 toPlayer = m_playerPosition - enemy.position;
        const float distance = Length(toPlayer);
        glm::vec2 direction = distance > 0.001f ? toPlayer / distance : glm::vec2(0.0f, 1.0f);

        if (enemy.type == EnemyType::Bat || enemy.type == EnemyType::Wraith) {
            const glm::vec2 tangent(-direction.y, direction.x);
            const float amplitude = enemy.type == EnemyType::Bat ? 0.30f : 0.45f;
            const float frequency = enemy.type == EnemyType::Bat ? 7.0f : 2.1f;
            direction = Normalize(direction + tangent * (std::sin(enemy.age * frequency + enemy.phase) * amplitude));
        }

        const float blend = 1.0f - std::exp(-deltaTime * 5.0f);
        enemy.velocity = glm::mix(enemy.velocity, direction * enemy.speed, blend);
        enemy.position += (enemy.velocity + enemy.impulse) * deltaTime;
        enemy.impulse *= std::exp(-deltaTime * 5.5f);
    }

    // 群体分离：用空间哈希限制邻近查询规模
    m_grid.Build(m_enemies);
    for (uint32_t i = 0; i < static_cast<uint32_t>(m_enemies.size()); ++i) {
        Enemy& enemy = m_enemies[i];
        if (!enemy.alive) {
            continue;
        }
        glm::vec2 push(0.0f, 0.0f);
        m_grid.ForEachNear(enemy.position, enemy.radius + 96.0f, [&](uint32_t otherIndex) {
            if (otherIndex == i || otherIndex >= m_enemies.size()) {
                return;
            }
            const Enemy& other = m_enemies[otherIndex];
            if (!other.alive) {
                return;
            }
            const glm::vec2 offset = enemy.position - other.position;
            const float distance = Length(offset);
            const float minimum = enemy.radius + other.radius;
            if (distance > 0.001f && distance < minimum) {
                push += offset / distance * ((minimum - distance) / minimum);
            }
        });
        const float weight = enemy.type == EnemyType::Boss ? 24.0f : 90.0f;
        enemy.position += push * weight * deltaTime;

        // 玩家也是碰撞体：把贴在身上的敌人挤开一点（Boss 体型大，只要求少量间隙）
        const glm::vec2 away = enemy.position - m_playerPosition;
        const float distanceToPlayer = Length(away);
        const float minimumDistance = PLAYER_RADIUS + enemy.radius * (enemy.type == EnemyType::Boss ? 0.35f : 0.85f);
        if (distanceToPlayer > 0.001f && distanceToPlayer < minimumDistance) {
            enemy.position += away / distanceToPlayer * (minimumDistance - distanceToPlayer);
        }
    }

    // 跑出很远的敌人从视野外重新入场：避免玩家直线狂奔时屏幕里空无一人
    for (Enemy& enemy : m_enemies) {
        if (!enemy.alive) {
            continue;
        }
        const float distance = Length(enemy.position - m_playerPosition);
        if (enemy.type == EnemyType::Boss) {
            if (distance > 1100.0f) {
                const float angle = Rand(0.0f, TAU);
                enemy.position = m_playerPosition + glm::vec2(std::cos(angle), std::sin(angle)) * 560.0f;
                enemy.velocity = glm::vec2(0.0f, 0.0f);
            }
        } else if (distance > 1400.0f) {
            enemy.position = EdgeSpawnPoint(60.0f);
            enemy.velocity = glm::vec2(0.0f, 0.0f);
            enemy.impulse = glm::vec2(0.0f, 0.0f);
        }
    }

    // 供武器索敌 / 弹丸碰撞使用的最新网格
    m_grid.Build(m_enemies);
}

void SurvivorsLayer::UpdateEnemySpawning(float deltaTime)
{
    if (m_time >= m_nextBossTime) {
        SpawnBoss();
        m_nextBossTime += 55.0f;
    }

    m_spawnTimer -= deltaTime;
    if (m_spawnTimer > 0.0f) {
        return;
    }
    if (static_cast<int>(m_enemies.size()) >= MAX_ENEMIES) {
        m_spawnTimer = 0.3f;
        return;
    }

    const float interval = std::max(0.20f, 0.80f - m_time * 0.0019f);
    m_spawnTimer = interval * Rand(0.80f, 1.20f);
    const int wave = 2 + std::min(5, static_cast<int>(m_time / 45.0f));

    for (int i = 0; i < wave; ++i) {
        if (static_cast<int>(m_enemies.size()) >= MAX_ENEMIES) {
            break;
        }
        const glm::vec2 position = EdgeSpawnPoint(40.0f);

        float weightBat = 1.0f;
        const float weightGhoul = m_time > 18.0f ? std::min(0.90f, (m_time - 18.0f) / 70.0f) : 0.0f;
        const float weightWraith = m_time > 65.0f ? std::min(0.75f, (m_time - 65.0f) / 75.0f) : 0.0f;
        const float weightBrute = m_time > 105.0f ? std::min(0.50f, (m_time - 105.0f) / 100.0f) : 0.0f;
        const float total = weightBat + weightGhoul + weightWraith + weightBrute;
        float cursor = Rand(0.0f, total);
        EnemyType type = EnemyType::Bat;
        if (cursor >= weightBat) {
            cursor -= weightBat;
            if (cursor < weightGhoul) {
                type = EnemyType::Ghoul;
            } else if (cursor < weightGhoul + weightWraith) {
                type = EnemyType::Wraith;
            } else {
                type = EnemyType::Brute;
            }
        }

        SpawnEnemy(type, position);
    }
}

void SurvivorsLayer::UpdateWeapons(float deltaTime)
{
    m_bladeCooldown = std::max(0.0f, m_bladeCooldown - deltaTime);
    m_auraCooldown = std::max(0.0f, m_auraCooldown - deltaTime);
    m_scytheCooldown = std::max(0.0f, m_scytheCooldown - deltaTime);
    m_stormCooldown = std::max(0.0f, m_stormCooldown - deltaTime);

    const int bladeLevel = m_weaponLevel[static_cast<int>(WeaponId::MoonBlade)];
    if (bladeLevel > 0 && m_bladeCooldown <= 0.0f) {
        if (FireMoonBlade()) {
            m_bladeCooldown = BLADE.cooldown[bladeLevel] / AttackSpeedMul();
        }
    }

    const int auraLevel = m_weaponLevel[static_cast<int>(WeaponId::Aura)];
    if (auraLevel > 0 && m_auraCooldown <= 0.0f) {
        PulseAura();
        m_auraCooldown = AURA.cooldown[auraLevel] / AttackSpeedMul();
    }

    const int scytheLevel = m_weaponLevel[static_cast<int>(WeaponId::Scythe)];
    if (scytheLevel > 0 && m_scytheCooldown <= 0.0f) {
        if (ThrowScythes()) {
            m_scytheCooldown = SCYTHE.cooldown[scytheLevel] / AttackSpeedMul();
        }
    }

    const int stormLevel = m_weaponLevel[static_cast<int>(WeaponId::Storm)];
    if (stormLevel > 0 && m_stormCooldown <= 0.0f) {
        if (CastStorm()) {
            m_stormCooldown = STORM.cooldown[stormLevel] / AttackSpeedMul();
        }
    }
}

void SurvivorsLayer::UpdateProjectiles(float deltaTime)
{
    for (Projectile& projectile : m_projectiles) {
        if (!projectile.alive) {
            continue;
        }
        projectile.age += deltaTime;

        if (projectile.kind == ShotKind::Scythe) {
            if (!projectile.returning && projectile.age > 0.55f) {
                projectile.returning = true;
            }
            if (projectile.returning) {
                const glm::vec2 toPlayer = m_playerPosition - projectile.position;
                const float distance = Length(toPlayer);
                if (distance < 46.0f) {
                    projectile.alive = false;
                    continue;
                }
                projectile.position += (toPlayer / distance) * 780.0f * deltaTime;
            } else {
                projectile.position += projectile.velocity * deltaTime;
                projectile.velocity *= std::exp(-deltaTime * 1.5f);
            }
            projectile.rotation += 15.0f * deltaTime;
        } else {
            projectile.position += projectile.velocity * deltaTime;
            projectile.rotation += projectile.spin * deltaTime;
        }

        bool consumed = false;
        m_grid.ForEachNear(projectile.position, 72.0f, [&](uint32_t index) {
            if (consumed || index >= m_enemies.size()) {
                return;
            }
            Enemy& enemy = m_enemies[index];
            if (!enemy.alive) {
                return;
            }
            if (!CircleHit(projectile.position, projectile.radius, enemy.position, enemy.radius)) {
                return;
            }

            if (projectile.kind == ShotKind::Scythe) {
                if (std::find(projectile.hitIds.begin(), projectile.hitIds.end(), enemy.id) != projectile.hitIds.end()) {
                    return;
                }
                projectile.hitIds.push_back(enemy.id);
            }

            const bool crit = Rand(0.0f, 1.0f) < CritChance();
            DamageEnemy(enemy, projectile.damage * (crit ? 2.0f : 1.0f), crit);
            SpawnParticles(enemy.position, crit ? 6 : 3,
                           projectile.kind == ShotKind::Scythe ? C_SCYTHE : C_BLADE,
                           180.0f, 6.0f, 0.35f);

            if (projectile.kind == ShotKind::Blade) {
                --projectile.pierce;
                if (projectile.pierce <= 0) {
                    consumed = true;
                }
            }
        });

        if (consumed || projectile.age > projectile.lifetime ||
            Length(projectile.position - m_playerPosition) > 1600.0f) {
            projectile.alive = false;
        }
    }
}

void SurvivorsLayer::UpdateGems(float deltaTime)
{
    const float pickupRadius = PickupRadius();
    for (Gem& gem : m_gems) {
        if (!gem.alive) {
            continue;
        }
        gem.age += deltaTime;

        const glm::vec2 toPlayer = m_playerPosition - gem.position;
        const float distance = Length(toPlayer);
        if (distance < pickupRadius) {
            const glm::vec2 direction = distance > 0.001f ? toPlayer / distance : glm::vec2(0.0f, 1.0f);
            const float speed = 280.0f + (pickupRadius - distance) * 4.0f;
            gem.velocity = glm::mix(gem.velocity, direction * speed, 1.0f - std::exp(-deltaTime * 11.0f));
        } else {
            gem.velocity *= std::exp(-deltaTime * 3.0f);
        }
        gem.position += gem.velocity * deltaTime;

        if (distance < 26.0f) {
            gem.alive = false;
            GainExp(static_cast<int>(gem.value));
            SpawnParticles(gem.position, 4, GEM_COLORS[gem.tier], 130.0f, 5.0f, 0.3f);
        }
    }
}

void SurvivorsLayer::UpdateContactDamage(float deltaTime)
{
    (void)deltaTime;
    if (m_hurtCooldown > 0.0f || !m_playerAlive) {
        return;
    }

    for (const Enemy& enemy : m_enemies) {
        if (!enemy.alive) {
            continue;
        }
        if (!CircleHit(enemy.position, enemy.radius, m_playerPosition, PLAYER_RADIUS)) {
            continue;
        }

        DamagePlayer(enemy.damage);

        // 受击后把周围的敌人推开，避免连续贴脸
        for (Enemy& other : m_enemies) {
            if (!other.alive) {
                continue;
            }
            const glm::vec2 offset = other.position - m_playerPosition;
            const float distance = Length(offset);
            if (distance > 0.001f && distance < 180.0f) {
                const float strength = 520.0f * (1.0f - distance / 180.0f);
                other.impulse += (offset / distance) * strength;
            }
        }
        break;
    }
}

void SurvivorsLayer::UpdateParticles(float deltaTime)
{
    for (Particle& particle : m_particles) {
        particle.age += deltaTime;
        particle.position += particle.velocity * deltaTime;
        particle.velocity *= std::exp(-particle.drag * deltaTime);
    }
    std::erase_if(m_particles, [](const Particle& particle) { return particle.age >= particle.lifetime; });
}

void SurvivorsLayer::UpdateEffects(float deltaTime)
{
    for (Fx& effect : m_effects) {
        effect.age += deltaTime;
    }
    std::erase_if(m_effects, [](const Fx& effect) { return effect.age >= effect.lifetime; });

    for (DamageNumber& number : m_damageNumbers) {
        number.age += deltaTime;
        number.position += number.drift * deltaTime;
    }
    std::erase_if(m_damageNumbers, [](const DamageNumber& number) { return number.age >= number.lifetime; });
}

void SurvivorsLayer::UpdateCamera(float deltaTime)
{
    glm::vec2 target = m_playerPosition + m_playerVelocity * 0.16f;
    if (!m_playerAlive) {
        target = m_cameraFocus;
    }
    const float blend = 1.0f - std::exp(-deltaTime * 5.5f);
    m_cameraFocus = glm::mix(m_cameraFocus, target, blend);
    m_camera.SetFocus(m_cameraFocus);

    glm::vec2 shake(0.0f, 0.0f);
    if (m_shake > 0.01f) {
        shake = glm::vec2(Rand(-1.0f, 1.0f), Rand(-1.0f, 1.0f)) * m_shake;
    }
    m_camera.SetShake(shake);

    for (Wisp& wisp : m_wisps) {
        wisp.position += wisp.velocity * deltaTime;
        if (Length(wisp.position - m_cameraFocus) > 1400.0f) {
            const float angle = Rand(0.0f, TAU);
            wisp.position = m_cameraFocus + glm::vec2(std::cos(angle), std::sin(angle)) * Rand(700.0f, 1200.0f);
        }
    }
}

// ============================ 武器 ============================

SurvivorsLayer::Enemy* SurvivorsLayer::FindNearestEnemy(const glm::vec2& origin, float range)
{
    Enemy* best = nullptr;
    float bestDistanceSq = range * range;
    m_grid.ForEachNear(origin, range, [&](uint32_t index) {
        if (index >= m_enemies.size()) {
            return;
        }
        Enemy& enemy = m_enemies[index];
        if (!enemy.alive) {
            return;
        }
        const glm::vec2 offset = enemy.position - origin;
        const float distanceSq = offset.x * offset.x + offset.y * offset.y;
        if (distanceSq < bestDistanceSq) {
            bestDistanceSq = distanceSq;
            best = &enemy;
        }
    });
    return best;
}

bool SurvivorsLayer::FireMoonBlade()
{
    const int level = m_weaponLevel[static_cast<int>(WeaponId::MoonBlade)];
    // 索敌范围略小于画面短半轴：只打看得见的目标，敌人能真正围到身边来
    Enemy* target = FindNearestEnemy(m_playerPosition, m_halfHeight - 45.0f);
    if (target == nullptr) {
        return false;
    }

    const glm::vec2 direction = Normalize(target->position - m_playerPosition);
    const float baseAngle = std::atan2(direction.y, direction.x);
    const int count = BLADE.count[level];
    for (int i = 0; i < count; ++i) {
        const float offset = count > 1
            ? (static_cast<float>(i) - static_cast<float>(count - 1) * 0.5f) * 0.14f
            : 0.0f;
        const float angle = baseAngle + offset;
        const glm::vec2 shotDirection(std::cos(angle), std::sin(angle));

        Projectile projectile;
        projectile.kind = ShotKind::Blade;
        projectile.position = m_playerPosition + shotDirection * 26.0f;
        projectile.velocity = shotDirection * 880.0f;
        projectile.damage = BLADE.damage[level] * DamageMul();
        projectile.radius = 16.0f;
        projectile.rotation = std::atan2(-shotDirection.x, shotDirection.y);
        projectile.spin = 14.0f;
        projectile.lifetime = 1.15f;
        projectile.pierce = BLADE.pierce[level];
        m_projectiles.push_back(projectile);
    }

    SpawnParticles(m_playerPosition + direction * 28.0f, 4, C_BLADE, 140.0f, 5.0f, 0.25f);
    m_shake = std::max(m_shake, 1.2f);
    return true;
}

void SurvivorsLayer::PulseAura()
{
    const int level = m_weaponLevel[static_cast<int>(WeaponId::Aura)];
    const float radius = AURA.radius[level];
    const float damage = AURA.damage[level] * DamageMul();

    m_grid.ForEachNear(m_playerPosition, radius, [&](uint32_t index) {
        if (index >= m_enemies.size()) {
            return;
        }
        Enemy& enemy = m_enemies[index];
        if (!enemy.alive || !CircleHit(enemy.position, enemy.radius, m_playerPosition, radius)) {
            return;
        }
        const bool crit = Rand(0.0f, 1.0f) < CritChance();
        DamageEnemy(enemy, damage * (crit ? 2.0f : 1.0f), crit);
    });

    SpawnRing(m_playerPosition, radius, 0.34f, C_AURA);
    SpawnParticles(m_playerPosition, 3, C_AURA, 70.0f, 9.0f, 0.3f);
}

bool SurvivorsLayer::ThrowScythes()
{
    const int level = m_weaponLevel[static_cast<int>(WeaponId::Scythe)];
    const int count = SCYTHE.count[level];
    const float baseAngle = Rand(0.0f, TAU);

    for (int i = 0; i < count; ++i) {
        const float angle = baseAngle + TAU * static_cast<float>(i) / static_cast<float>(count);
        const glm::vec2 direction(std::cos(angle), std::sin(angle));

        Projectile projectile;
        projectile.kind = ShotKind::Scythe;
        projectile.position = m_playerPosition + direction * 34.0f;
        projectile.velocity = direction * 520.0f;
        projectile.damage = SCYTHE.damage[level] * DamageMul();
        projectile.radius = 30.0f;
        projectile.rotation = std::atan2(-direction.x, direction.y);
        projectile.lifetime = 4.0f;
        projectile.pierce = 99;
        m_projectiles.push_back(projectile);
    }
    return true;
}

bool SurvivorsLayer::CastStorm()
{
    const int level = m_weaponLevel[static_cast<int>(WeaponId::Storm)];
    std::vector<uint32_t> targets;
    m_grid.ForEachNear(m_playerPosition, 620.0f, [&](uint32_t index) {
        if (index >= m_enemies.size()) {
            return;
        }
        const Enemy& enemy = m_enemies[index];
        if (!enemy.alive || Length(enemy.position - m_playerPosition) > 620.0f) {
            return;
        }
        targets.push_back(index);
    });
    if (targets.empty()) {
        return false;
    }

    const int strikes = STORM.count[level];
    for (int strike = 0; strike < strikes && !targets.empty(); ++strike) {
        const int pick = RandInt(0, static_cast<int>(targets.size()) - 1);
        Enemy& enemy = m_enemies[targets[pick]];
        targets.erase(targets.begin() + pick);
        if (!enemy.alive) {
            continue;
        }

        const bool crit = Rand(0.0f, 1.0f) < CritChance();
        DamageEnemy(enemy, STORM.damage[level] * DamageMul() * (crit ? 2.0f : 1.0f), crit);

        Fx bolt;
        bolt.kind = FxKind::Bolt;
        bolt.position = enemy.position;
        bolt.radius = 260.0f;
        bolt.lifetime = 0.30f;
        bolt.color = C_STORM;
        m_effects.push_back(bolt);

        SpawnRing(enemy.position, 110.0f, 0.32f, C_STORM);
        SpawnParticles(enemy.position, 6, C_STORM, 220.0f, 8.0f, 0.4f);
        m_shake = std::max(m_shake, 4.0f);
    }
    return true;
}

// ============================ 战斗 ============================

void SurvivorsLayer::SpawnEnemy(EnemyType type, const glm::vec2& position)
{
    const int index = std::min(static_cast<int>(type), 4);
    const EnemyTuning& tuning = ENEMY_TUNING[index];

    Enemy enemy;
    enemy.type = type;
    enemy.id = m_nextEnemyId++;
    enemy.position = position;
    enemy.radius = tuning.radius;
    enemy.speed = tuning.speed;
    enemy.damage = tuning.damage;
    enemy.xp = tuning.xp;
    enemy.phase = Rand(0.0f, TAU);

    float hp = tuning.hp;
    if (type == EnemyType::Boss) {
        hp *= 1.0f + 0.55f * static_cast<float>(m_bossesSpawned - 1);
    } else {
        hp *= 1.0f + m_time * 0.0030f;
        enemy.damage *= 1.0f + m_time * 0.0016f;
    }
    enemy.hp = hp;
    enemy.maxHp = hp;

    m_enemies.push_back(enemy);
}

glm::vec2 SurvivorsLayer::EdgeSpawnPoint(float margin)
{
    // 沿随机方向投射到「画面矩形 + 边距」的外侧：无论从哪边来，都刚好在屏幕外一点
    const float angle = Rand(0.0f, TAU);
    const glm::vec2 direction(std::cos(angle), std::sin(angle));
    const float horizontal = std::abs(direction.x) > 0.0001f
        ? (m_halfWidth + margin) / std::abs(direction.x) : 100000.0f;
    const float vertical = std::abs(direction.y) > 0.0001f
        ? (m_halfHeight + margin) / std::abs(direction.y) : 100000.0f;
    const float radius = std::min(horizontal, vertical) * Rand(1.0f, 1.08f);
    return m_cameraFocus + direction * radius;
}

void SurvivorsLayer::SpawnBoss()
{
    ++m_bossesSpawned;
    const float angle = Rand(0.0f, TAU);
    const glm::vec2 position = m_playerPosition + glm::vec2(std::cos(angle), std::sin(angle)) * 560.0f;
    SpawnEnemy(EnemyType::Boss, position);

    if (!m_enemies.empty()) {
        m_bossId = m_enemies.back().id;
    }
    m_bossAlive = true;
    m_shake = 16.0f;
    SpawnRing(position, 220.0f, 0.9f, C_BLOOD);
    OI_INFO("Survivors: 血月使徒降临（第 {} 只）", m_bossesSpawned);
}

void SurvivorsLayer::DamageEnemy(Enemy& enemy, float damage, bool crit)
{
    if (!enemy.alive) {
        return;
    }
    enemy.hp -= damage;
    enemy.hitFlash = 0.18f;
    m_damageDealt += damage;
    SpawnDamageNumber(enemy.position + glm::vec2(0.0f, enemy.radius * 0.75f), damage, crit);
    if (enemy.hp <= 0.0f) {
        KillEnemy(enemy);
    }
}

void SurvivorsLayer::KillEnemy(Enemy& enemy)
{
    enemy.alive = false;
    ++m_kills;

    const bool isBoss = enemy.type == EnemyType::Boss;
    const glm::vec4 color = [&]() {
        switch (enemy.type) {
            case EnemyType::Bat: return glm::vec4(0.60f, 0.34f, 0.72f, 1.0f);
            case EnemyType::Ghoul: return glm::vec4(0.38f, 0.58f, 0.38f, 1.0f);
            case EnemyType::Wraith: return glm::vec4(0.46f, 0.84f, 0.92f, 1.0f);
            case EnemyType::Brute: return glm::vec4(0.72f, 0.38f, 0.24f, 1.0f);
            case EnemyType::Boss: return glm::vec4(0.90f, 0.16f, 0.20f, 1.0f);
            default: return glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        }
    }();

    SpawnParticles(enemy.position, isBoss ? 46 : (enemy.type == EnemyType::Brute ? 18 : 9),
                   color, isBoss ? 520.0f : 280.0f, isBoss ? 20.0f : 10.0f, 0.7f);
    SpawnRing(enemy.position, enemy.radius * (isBoss ? 6.0f : 2.2f), isBoss ? 0.9f : 0.35f, color);

    if (isBoss) {
        m_bossAlive = false;
        m_shake = 26.0f;
        for (int i = 0; i < 12; ++i) {
            SpawnGem(enemy.position + glm::vec2(Rand(-60.0f, 60.0f), Rand(-60.0f, 60.0f)), 10, true);
        }
        m_playerHp = std::min(m_playerMaxHp, m_playerHp + 30.0f);
        SpawnRing(enemy.position, 420.0f, 1.2f, C_GOLD);
        OI_INFO("Survivors: 血月使徒被击败");
        return;
    }

    if (enemy.type == EnemyType::Brute) {
        for (int i = 0; i < 4; ++i) {
            SpawnGem(enemy.position + glm::vec2(Rand(-26.0f, 26.0f), Rand(-26.0f, 26.0f)), 4, true);
        }
        m_shake = std::max(m_shake, 5.0f);
    } else if (enemy.xp >= 6) {
        SpawnGem(enemy.position, enemy.xp / 2, true);
        SpawnGem(enemy.position + glm::vec2(Rand(-16.0f, 16.0f), Rand(-16.0f, 16.0f)), enemy.xp - enemy.xp / 2, true);
    } else {
        SpawnGem(enemy.position, enemy.xp, false);
    }
}

void SurvivorsLayer::DamagePlayer(float amount)
{
    if (!m_playerAlive) {
        return;
    }
    m_playerHp -= amount;
    m_hurtCooldown = 0.95f;
    m_hurtFlash = 1.0f;
    m_shake = std::max(m_shake, 15.0f);
    SpawnParticles(m_playerPosition, 14, C_BLOOD, 300.0f, 8.0f, 0.55f);

    if (m_playerHp <= 0.0f) {
        m_playerHp = 0.0f;
        m_playerAlive = false;
        m_state = State::GameOver;
        m_shake = 34.0f;
        SpawnParticles(m_playerPosition, 46, C_BLOOD, 460.0f, 14.0f, 0.9f);
        SpawnRing(m_playerPosition, 360.0f, 1.1f, C_BLOOD);
        OI_INFO("Survivors: 玩家阵亡，存活 {} 秒，击杀 {}", static_cast<int>(m_time), m_kills);
    }
}

void SurvivorsLayer::SpawnGem(const glm::vec2& position, int value, bool burst)
{
    if (static_cast<int>(m_gems.size()) >= MAX_GEMS) {
        return;
    }
    Gem gem;
    gem.position = position;
    gem.value = static_cast<float>(std::max(1, value));
    gem.tier = value <= 2 ? 0 : (value <= 8 ? 1 : 2);
    gem.phase = Rand(0.0f, TAU);
    if (burst) {
        const float angle = Rand(0.0f, TAU);
        gem.velocity = glm::vec2(std::cos(angle), std::sin(angle)) * Rand(60.0f, 150.0f);
    }
    m_gems.push_back(gem);
}

void SurvivorsLayer::SpawnParticles(const glm::vec2& position, int count, const glm::vec4& color,
                                    float speed, float size, float lifetime, float zBias)
{
    (void)zBias;
    if (static_cast<int>(m_particles.size()) >= MAX_PARTICLES) {
        return;
    }
    for (int i = 0; i < count; ++i) {
        if (static_cast<int>(m_particles.size()) >= MAX_PARTICLES) {
            break;
        }
        Particle particle;
        particle.position = position;
        const float angle = Rand(0.0f, TAU);
        const float velocity = speed * Rand(0.25f, 1.0f);
        particle.velocity = glm::vec2(std::cos(angle), std::sin(angle)) * velocity;
        particle.size = size * Rand(0.6f, 1.4f);
        particle.lifetime = lifetime * Rand(0.6f, 1.2f);
        particle.drag = Rand(2.0f, 4.5f);
        particle.color = color;
        m_particles.push_back(particle);
    }
}

void SurvivorsLayer::SpawnRing(const glm::vec2& position, float radius, float lifetime, const glm::vec4& color)
{
    Fx fx;
    fx.kind = FxKind::Ring;
    fx.position = position;
    fx.radius = radius;
    fx.lifetime = lifetime;
    fx.color = color;
    m_effects.push_back(fx);
}

void SurvivorsLayer::SpawnFlash(const glm::vec2& position, float radius, float lifetime, const glm::vec4& color)
{
    Fx fx;
    fx.kind = FxKind::Flash;
    fx.position = position;
    fx.radius = radius;
    fx.lifetime = lifetime;
    fx.color = color;
    m_effects.push_back(fx);
}

void SurvivorsLayer::SpawnDamageNumber(const glm::vec2& position, float value, bool crit)
{
    if (static_cast<int>(m_damageNumbers.size()) >= MAX_DAMAGE_NUMBERS) {
        m_damageNumbers.erase(m_damageNumbers.begin());
    }
    DamageNumber number;
    number.position = position;
    number.value = value;
    number.crit = crit;
    number.lifetime = crit ? 0.85f : 0.6f;
    number.drift = glm::vec2(Rand(-18.0f, 18.0f), crit ? 70.0f : 46.0f);
    m_damageNumbers.push_back(number);
}

void SurvivorsLayer::GainExp(int amount)
{
    m_exp += amount;
    while (m_exp >= m_expNext) {
        m_exp -= m_expNext;
        ++m_level;
        ++m_pendingLevelUps;
        m_expNext = ExpNeeded(m_level);
    }
}

// ============================ 升级 ============================

void SurvivorsLayer::StartLevelUp()
{
    m_state = State::LevelUp;
    m_levelFlash = 1.0f;
    m_shake = 0.0f;
    m_camera.SetShake(glm::vec2(0.0f, 0.0f));
    BuildUpgradeOptions();
}

void SurvivorsLayer::BuildUpgradeOptions()
{
    std::vector<UpgradeOption> pool;

    for (int weapon = 0; weapon < 4; ++weapon) {
        const int level = m_weaponLevel[weapon];
        if (level >= 5) {
            continue;
        }
        UpgradeOption option;
        option.kind = UpgradeKind::Weapon;
        option.index = weapon;
        option.color = WEAPON_COLORS[weapon];

        if (level == 0) {
            option.tag = "新武器";
            option.title = std::string("解锁 · ") + WEAPON_NAMES[weapon];
            switch (weapon) {
                case 1: option.description = "以自身为中心脉动圣光，持续灼烧周围敌人。\n伤害 5 / 半径 120"; break;
                case 2: option.description = "掷出回旋镰，飞出后折返，可反复切割路径上的敌人。\n伤害 22"; break;
                case 3: option.description = "召唤雷暴，随机劈击附近敌人。\n伤害 34 / 落雷 2 道"; break;
                default: option.description = "自动锁定最近敌人，射出旋转月刃。\n伤害 11 / 穿透 2"; break;
            }
        } else {
            option.tag = std::format("Lv.{}", level + 1);
            option.title = std::format("{} {}", WEAPON_NAMES[weapon], ROMAN[level + 1]);
            switch (weapon) {
                case 0:
                    option.description = std::format("伤害 {:.0f} → {:.0f}\n飞刃 {} 道 · 穿透 {}",
                        BLADE.damage[level], BLADE.damage[level + 1],
                        BLADE.count[level + 1], BLADE.pierce[level + 1]);
                    break;
                case 1:
                    option.description = std::format("伤害 {:.0f} → {:.0f}\n光环半径 {:.0f} → {:.0f}",
                        AURA.damage[level], AURA.damage[level + 1],
                        AURA.radius[level], AURA.radius[level + 1]);
                    break;
                case 2:
                    option.description = std::format("伤害 {:.0f} → {:.0f}\n回旋镰 {} 把",
                        SCYTHE.damage[level], SCYTHE.damage[level + 1], SCYTHE.count[level + 1]);
                    break;
                default:
                    option.description = std::format("伤害 {:.0f} → {:.0f}\n落雷 {} 道",
                        STORM.damage[level], STORM.damage[level + 1], STORM.count[level + 1]);
                    break;
            }
        }
        pool.push_back(option);
    }

    for (int passive = 0; passive < static_cast<int>(PassiveId::Count); ++passive) {
        if (m_passive[passive] >= PASSIVE_MAX[passive]) {
            continue;
        }
        UpgradeOption option;
        option.kind = UpgradeKind::Passive;
        option.index = passive;
        option.tag = "被动";
        option.color = glm::vec4(0.66f, 0.78f, 0.95f, 1.0f);
        DescribePassive(passive, option.title, option.description);
        pool.push_back(option);
    }

    UpgradeOption heal;
    heal.kind = UpgradeKind::Heal;
    heal.tag = "补给";
    heal.title = "血之祭";
    heal.description = "立即恢复 35% 最大生命。";
    heal.color = C_HEAL;
    pool.push_back(heal);

    std::shuffle(pool.begin(), pool.end(), m_rng);
    const int count = std::min(3, static_cast<int>(pool.size()));
    m_options.assign(pool.begin(), pool.begin() + count);
}

void SurvivorsLayer::DescribePassive(int passive, std::string& title, std::string& description) const
{
    const int level = m_passive[passive];
    title = std::format("{} {}", PASSIVE_NAMES[passive], ROMAN[level + 1]);
    switch (passive) {
        case 0:
            description = std::format("全部伤害 +12%\n累计 +{}%", 12 * (level + 1));
            break;
        case 1:
            description = std::format("攻击速度 +10%\n累计 +{}%", 10 * (level + 1));
            break;
        case 2:
            description = std::format("移动速度 +7%\n累计 +{}%", 7 * (level + 1));
            break;
        case 3:
            description = std::format("最大生命 +25，并立即恢复 25\n上限变为 {:.0f}", MaxHp() + 25.0f);
            break;
        case 4:
            description = std::format("经验拾取范围 +35%\n累计 +{}%", 35 * (level + 1));
            break;
        case 5:
            description = std::format("暴击率 +6%（暴击造成双倍伤害）\n累计 {}%",
                static_cast<int>((CritChance() + 0.06f) * 100.0f));
            break;
        default:
            description = std::format("每秒回复 +0.4 生命\n累计 {:.1f}/秒", 0.4f * static_cast<float>(level + 1));
            break;
    }
}

void SurvivorsLayer::ApplyUpgrade(int optionIndex)
{
    if (optionIndex < 0 || optionIndex >= static_cast<int>(m_options.size())) {
        return;
    }
    const UpgradeOption option = m_options[optionIndex];

    switch (option.kind) {
        case UpgradeKind::Weapon:
            m_weaponLevel[option.index] = std::min(5, m_weaponLevel[option.index] + 1);
            break;
        case UpgradeKind::Passive:
            m_passive[option.index] = std::min(PASSIVE_MAX[option.index], m_passive[option.index] + 1);
            break;
        case UpgradeKind::Heal:
            break;
    }

    // 任何成长变化后统一重算最终属性（以后武器也带属性时不用再改这里）
    RecalculateStats();

    if (option.kind == UpgradeKind::Passive && option.index == static_cast<int>(PassiveId::Vitality)) {
        m_playerHp = std::min(m_playerMaxHp, m_playerHp + 25.0f);
    } else if (option.kind == UpgradeKind::Heal) {
        m_playerHp = std::min(m_playerMaxHp, m_playerHp + m_playerMaxHp * 0.35f);
    }

    SpawnRing(m_playerPosition, 110.0f, 0.5f, option.color);
    SpawnParticles(m_playerPosition, 12, option.color, 200.0f, 7.0f, 0.5f);

    --m_pendingLevelUps;
    if (m_pendingLevelUps > 0) {
        BuildUpgradeOptions();
    } else {
        m_state = State::Playing;
        m_levelFlash = 1.0f;
        m_levelUpLockout = 1.5f;
    }
}

// ============================ 渲染 ============================

void SurvivorsLayer::DrawMesh(const Ref<VertexArray>& mesh, const glm::mat4& transform,
                              const glm::vec4& tint, float falloff)
{
    m_shader->SetMat4("u_Transform", transform);
    m_shader->SetFloat4("u_Tint", tint);
    m_shader->SetFloat("u_Falloff", falloff);
    RenderCommand::DrawIndexed(mesh);
}

void SurvivorsLayer::RenderWorld()
{
    m_shader->Bind();
    m_shader->SetFloat("u_Falloff", 0.0f);

    DrawBackground();
    DrawGround();
    DrawGems();
    DrawEnemies();
    DrawEffectsUnder();
    DrawPlayer();
    DrawProjectiles();
    DrawParticles();
    DrawEffectsOver();
    DrawScreenOverlay();
}

void SurvivorsLayer::DrawBackground()
{
    // 月光：以玩家上方为中心的大范围冷色光
    DrawMesh(m_art.disc, Translate(glm::vec3(m_cameraFocus.x, m_cameraFocus.y + 120.0f, -9.2f)) *
                         Scale(glm::vec3(1650.0f, 1250.0f, 1.0f)),
             glm::vec4(0.34f, 0.30f, 0.62f, 0.26f), 0.9f);
    DrawMesh(m_art.disc, Translate(glm::vec3(m_cameraFocus.x, m_cameraFocus.y + 330.0f, -9.1f)) *
                         Scale(glm::vec3(250.0f, 250.0f, 1.0f)),
             glm::vec4(0.80f, 0.84f, 1.00f, 0.30f), 0.6f);

    // 雾团
    for (const Wisp& wisp : m_wisps) {
        DrawMesh(m_art.disc, Translate(glm::vec3(wisp.position.x, wisp.position.y, -8.8f)) *
                             Scale(glm::vec3(wisp.radius, wisp.radius * 0.72f, 1.0f)),
                 wisp.color, 1.0f);
    }

    // 上方紫色薄雾
    DrawMesh(m_art.vignette, Translate(glm::vec3(m_cameraFocus.x, m_cameraFocus.y + m_halfHeight - 210.0f, -8.6f)) *
                             Scale(glm::vec3(m_halfWidth * 2.0f, 420.0f, 1.0f)),
             glm::vec4(0.34f, 0.27f, 0.62f, 0.22f));


}

void SurvivorsLayer::DrawGround()
{
    constexpr float GRID_STEP = 120.0f;
    const float gridX = std::floor(m_cameraFocus.x / GRID_STEP) * GRID_STEP;
    const float gridY = std::floor(m_cameraFocus.y / GRID_STEP) * GRID_STEP;
    m_shader->SetMat4("u_Transform", Translate(glm::vec3(gridX, gridY, -7.55f)));
    m_shader->SetFloat4("u_Tint", WHITE);
    m_shader->SetFloat("u_Falloff", 0.0f);
    RenderCommand::DrawLines(m_art.lineGrid, m_art.lineGridVertexCount);

    // 按格子哈希放置的墓碑 / 草丛：无需存储，走多远都有稳定装饰
    constexpr float CELL = 190.0f;
    const int cx0 = static_cast<int>(std::floor((m_cameraFocus.x - m_halfWidth - CELL) / CELL));
    const int cx1 = static_cast<int>(std::floor((m_cameraFocus.x + m_halfWidth + CELL) / CELL));
    const int cy0 = static_cast<int>(std::floor((m_cameraFocus.y - m_halfHeight - CELL) / CELL));
    const int cy1 = static_cast<int>(std::floor((m_cameraFocus.y + m_halfHeight + CELL) / CELL));

    for (int cy = cy0; cy <= cy1; ++cy) {
        for (int cx = cx0; cx <= cx1; ++cx) {
            const uint32_t hash = HashCell(cx, cy);
            const uint32_t kind = hash & 15u;
            if (kind > 4) {
                continue;
            }
            const glm::vec2 position(
                (static_cast<float>(cx) + HashUnit(hash, 4)) * CELL,
                (static_cast<float>(cy) + HashUnit(hash, 14)) * CELL);
            if (kind <= 1) {
                const float scale = 52.0f + HashUnit(hash, 20) * 34.0f;
                const float rotation = (HashUnit(hash, 24) - 0.5f) * 0.6f;
                DrawMesh(m_art.tombstone, Translate(glm::vec3(position.x, position.y, -7.3f)) *
                                          RotateZ(rotation) * Scale(glm::vec3(scale, scale, 1.0f)),
                         glm::vec4(1.0f, 1.0f, 1.0f, 0.9f));
            } else {
                const float scale = 24.0f + HashUnit(hash, 20) * 22.0f;
                const float rotation = (HashUnit(hash, 24) - 0.5f) * 1.2f;
                DrawMesh(m_art.grass, Translate(glm::vec3(position.x, position.y, -7.4f)) *
                                      RotateZ(rotation) * Scale(glm::vec3(scale, scale, 1.0f)),
                         glm::vec4(1.0f, 1.0f, 1.0f, 0.7f));
            }
        }
    }
}

void SurvivorsLayer::DrawGems()
{
    for (const Gem& gem : m_gems) {
        if (!gem.alive) {
            continue;
        }
        const Ref<VertexArray>& mesh = gem.tier == 0 ? m_art.gemSmall
            : (gem.tier == 1 ? m_art.gemMedium : m_art.gemLarge);
        const float size = gem.tier == 0 ? 17.0f : (gem.tier == 1 ? 22.0f : 28.0f);
        const float pulse = 0.9f + 0.1f * std::sin(gem.age * 4.0f + gem.phase);
        const float bob = std::sin(gem.age * 3.0f + gem.phase) * 2.2f;
        const glm::vec4& color = GEM_COLORS[gem.tier];

        DrawMesh(m_art.disc, Translate(glm::vec3(gem.position.x, gem.position.y, -2.05f)) *
                             Scale(glm::vec3(size * 1.8f * pulse, size * 1.8f * pulse, 1.0f)),
                 glm::vec4(color.x, color.y, color.z, 0.20f), 1.0f);
        DrawMesh(mesh, Translate(glm::vec3(gem.position.x, gem.position.y + bob, -2.0f)) *
                        RotateZ(gem.age * 1.3f + gem.phase) * Scale(glm::vec3(size, size, 1.0f)),
                 WHITE);
    }
}

void SurvivorsLayer::DrawEnemies()
{
    for (const Enemy& enemy : m_enemies) {
        if (!enemy.alive) {
            continue;
        }
        const Ref<VertexArray>* mesh = &m_art.bat;
        float bob = 0.0f;
        float scaleX = 1.0f;
        switch (enemy.type) {
            case EnemyType::Bat:
                mesh = &m_art.bat;
                bob = std::sin(enemy.age * 13.0f + enemy.phase) * 3.0f;
                scaleX = 0.78f + 0.30f * std::abs(std::sin(enemy.age * 13.0f + enemy.phase));
                break;
            case EnemyType::Ghoul:
                mesh = &m_art.ghoul;
                bob = std::sin(enemy.age * 4.5f + enemy.phase) * 2.0f;
                break;
            case EnemyType::Wraith:
                mesh = &m_art.wraith;
                bob = std::sin(enemy.age * 2.6f + enemy.phase) * 4.0f;
                break;
            case EnemyType::Brute:
                mesh = &m_art.brute;
                bob = std::sin(enemy.age * 3.1f + enemy.phase) * 1.6f;
                break;
            case EnemyType::Boss:
                mesh = &m_art.boss;
                bob = std::sin(enemy.age * 1.7f) * 3.0f;
                break;
            default:
                break;
        }

        const float scale = enemy.radius * 2.0f;

        if (enemy.type != EnemyType::Bat) {
            DrawMesh(m_art.disc, Translate(glm::vec3(enemy.position.x, enemy.position.y - 4.0f, -1.5f)) *
                                 Scale(glm::vec3(scale * 0.58f, scale * 0.26f, 1.0f)),
                     glm::vec4(0.0f, 0.0f, 0.0f, 0.38f), 1.0f);
        }

        glm::vec4 tint = WHITE;
        if (enemy.hitFlash > 0.0f) {
            const float flash = enemy.hitFlash / 0.18f;
            tint = glm::vec4(1.0f + flash * 1.4f, 1.0f + flash * 0.6f, 1.0f + flash * 0.6f, 1.0f);
        }

        if (enemy.type == EnemyType::Boss) {
            DrawMesh(m_art.ring, Translate(glm::vec3(enemy.position.x, enemy.position.y - 6.0f, -1.2f)) *
                                 Scale(glm::vec3(scale * 1.9f, scale * 1.9f, 1.0f)),
                     glm::vec4(0.90f, 0.18f, 0.22f, 0.32f));
            DrawMesh(m_art.disc, Translate(glm::vec3(enemy.position.x, enemy.position.y, -1.3f)) *
                                 Scale(glm::vec3(scale * 1.2f, scale * 1.2f, 1.0f)),
                     glm::vec4(0.70f, 0.10f, 0.14f, 0.18f), 1.0f);
        }

        DrawMesh(*mesh, Translate(glm::vec3(enemy.position.x, enemy.position.y + bob, 0.0f)) *
                        Scale(glm::vec3(scale * scaleX, scale, 1.0f)), tint);
    }
}

void SurvivorsLayer::DrawPlayer()
{
    const float bob = std::sin(m_walkPhase * 2.1f) * 2.2f;
    const int auraLevel = m_weaponLevel[static_cast<int>(WeaponId::Aura)];

    if (!m_playerAlive) {
        DrawMesh(m_art.disc, Translate(glm::vec3(m_playerPosition.x, m_playerPosition.y - 4.0f, -1.0f)) *
                             Scale(glm::vec3(70.0f, 26.0f, 1.0f)),
                 glm::vec4(0.35f, 0.03f, 0.06f, 0.55f), 1.0f);
        return;
    }

    // 圣光环法阵与光晕
    if (auraLevel > 0) {
        const float radius = AURA.radius[auraLevel];
        DrawMesh(m_art.disc, Translate(glm::vec3(m_playerPosition.x, m_playerPosition.y, -1.1f)) *
                             Scale(glm::vec3(radius, radius, 1.0f)),
                 glm::vec4(1.0f, 0.82f, 0.42f, 0.075f), 1.0f);
        DrawMesh(m_art.sigil, Translate(glm::vec3(m_playerPosition.x, m_playerPosition.y, -1.05f)) *
                              RotateZ(m_time * 0.35f) * Scale(glm::vec3(radius * 0.92f, radius * 0.92f, 1.0f)),
                 glm::vec4(1.0f, 0.86f, 0.50f, 0.34f));
    }

    DrawMesh(m_art.disc, Translate(glm::vec3(m_playerPosition.x, m_playerPosition.y - 3.0f, -1.4f)) *
                         Scale(glm::vec3(44.0f, 18.0f, 1.0f)),
             glm::vec4(0.0f, 0.0f, 0.0f, 0.45f), 1.0f);

    // 身周微光；没有圣光环时额外补一圈小型法阵，避免角色淹没在黑暗里
    DrawMesh(m_art.disc, Translate(glm::vec3(m_playerPosition.x, m_playerPosition.y, -1.3f)) *
                         Scale(glm::vec3(130.0f, 130.0f, 1.0f)),
             glm::vec4(0.36f, 0.62f, 1.0f, 0.10f), 1.0f);
    if (auraLevel == 0) {
        DrawMesh(m_art.sigil, Translate(glm::vec3(m_playerPosition.x, m_playerPosition.y, -1.15f)) *
                              RotateZ(-m_time * 0.22f) * Scale(glm::vec3(62.0f, 62.0f, 1.0f)),
                 glm::vec4(0.55f, 0.85f, 1.0f, 0.18f));
    }

    // 法杖宝石的光
    DrawMesh(m_art.disc, Translate(glm::vec3(m_playerPosition.x + 16.0f, m_playerPosition.y + 22.0f + bob, 0.5f)) *
                         Scale(glm::vec3(30.0f, 30.0f, 1.0f)),
             glm::vec4(0.55f, 0.95f, 1.0f, 0.22f), 1.0f);

    glm::vec4 tint = WHITE;
    if (m_hurtCooldown > 0.0f && std::fmod(m_hurtCooldown, 0.22f) < 0.12f) {
        tint = glm::vec4(1.6f, 0.7f, 0.7f, 1.0f);
    }
    DrawMesh(m_art.player, Translate(glm::vec3(m_playerPosition.x, m_playerPosition.y + bob, 0.6f)) *
                           Scale(glm::vec3(64.0f, 64.0f, 1.0f)), tint);
}

void SurvivorsLayer::DrawProjectiles()
{
    for (const Projectile& projectile : m_projectiles) {
        if (!projectile.alive) {
            continue;
        }
        const bool blade = projectile.kind == ShotKind::Blade;
        const glm::vec4 color = blade ? C_BLADE : C_SCYTHE;
        const float size = blade ? 34.0f : 50.0f;

        DrawMesh(m_art.disc, Translate(glm::vec3(projectile.position.x, projectile.position.y, 0.72f)) *
                             Scale(glm::vec3(size * 0.9f, size * 0.9f, 1.0f)),
                 glm::vec4(color.x, color.y, color.z, blade ? 0.22f : 0.30f), 1.0f);
        DrawMesh(blade ? m_art.blade : m_art.scythe,
                 Translate(glm::vec3(projectile.position.x, projectile.position.y, 0.80f)) *
                     RotateZ(projectile.rotation) * Scale(glm::vec3(size, size, 1.0f)),
                 WHITE);
    }
}

void SurvivorsLayer::DrawParticles()
{
    for (const Particle& particle : m_particles) {
        const float t = std::min(particle.age / particle.lifetime, 1.0f);
        const float alpha = particle.color.w * (1.0f - t) * (1.0f - t);
        const float size = particle.size * (1.0f - t * 0.45f);
        DrawMesh(m_art.disc, Translate(glm::vec3(particle.position.x, particle.position.y, 1.25f)) *
                             Scale(glm::vec3(size, size, 1.0f)),
                 glm::vec4(particle.color.x, particle.color.y, particle.color.z, alpha), 1.0f);
    }
}

void SurvivorsLayer::DrawEffectsUnder()
{
    for (const Fx& effect : m_effects) {
        if (effect.kind != FxKind::Ring) {
            continue;
        }
        const float t = std::min(effect.age / effect.lifetime, 1.0f);
        const float expand = 0.30f + 0.70f * (t * t * (3.0f - 2.0f * t));
        const float radius = effect.radius * expand;
        const float alpha = (1.0f - t) * effect.color.w;
        DrawMesh(m_art.ring, Translate(glm::vec3(effect.position.x, effect.position.y, 0.22f)) *
                             Scale(glm::vec3(radius, radius, 1.0f)),
                 glm::vec4(effect.color.x, effect.color.y, effect.color.z, alpha));
    }
}

void SurvivorsLayer::DrawEffectsOver()
{
    for (const Fx& effect : m_effects) {
        const float t = std::min(effect.age / effect.lifetime, 1.0f);
        if (effect.kind == FxKind::Flash) {
            const float alpha = (1.0f - t) * (1.0f - t) * effect.color.w;
            const float radius = effect.radius * (0.6f + 0.5f * t);
            DrawMesh(m_art.disc, Translate(glm::vec3(effect.position.x, effect.position.y, 1.40f)) *
                                 Scale(glm::vec3(radius, radius, 1.0f)),
                 glm::vec4(effect.color.x, effect.color.y, effect.color.z, alpha), 1.0f);
        } else if (effect.kind == FxKind::Bolt) {
            const float alpha = (1.0f - t) * effect.color.w;
            DrawMesh(m_art.bolt, Translate(glm::vec3(effect.position.x, effect.position.y + 40.0f, 1.48f)) *
                                  Scale(glm::vec3(effect.radius * 0.55f, effect.radius * 2.0f, 1.0f)),
                 glm::vec4(1.0f, 1.0f, 1.0f, alpha));
            DrawMesh(m_art.disc, Translate(glm::vec3(effect.position.x, effect.position.y, 1.45f)) *
                                 Scale(glm::vec3(effect.radius * 0.55f, effect.radius * 0.55f, 1.0f)),
                 glm::vec4(effect.color.x, effect.color.y, effect.color.z, alpha * 0.7f), 1.0f);
        }
    }
}

void SurvivorsLayer::DrawScreenOverlay()
{
    // 径向暗角最后画：无接缝地压暗画面四周（深度也在这里写入，所以全屏闪光必须放在更高的 z）
    DrawMesh(m_art.vignetteDisc, Translate(glm::vec3(m_cameraFocus.x, m_cameraFocus.y, 3.0f)) *
                                 Scale(glm::vec3(m_halfWidth * 1.50f, m_halfHeight * 1.50f, 1.0f)),
             glm::vec4(0.0f, 0.0f, 0.0f, 0.62f));

    const glm::vec3 origin(m_cameraFocus.x, m_cameraFocus.y, 0.0f);
    const glm::mat4 fullScreen = Scale(glm::vec3(m_halfWidth * 2.0f, m_halfHeight * 2.0f, 1.0f));

    if (m_hurtFlash > 0.01f) {
        DrawMesh(m_art.quad, Translate(origin + glm::vec3(0.0f, 0.0f, 3.60f)) * fullScreen,
                 glm::vec4(0.75f, 0.06f, 0.10f, 0.30f * m_hurtFlash));
    }
    if (m_levelFlash > 0.01f) {
        DrawMesh(m_art.quad, Translate(origin + glm::vec3(0.0f, 0.0f, 3.55f)) * fullScreen,
                 glm::vec4(0.55f, 0.80f, 1.00f, 0.22f * m_levelFlash));
    }
    if (m_playerAlive && m_playerHp > 0.0f && m_playerHp / m_playerMaxHp < 0.30f) {
        const float pulse = 0.5f + 0.5f * std::sin(m_time * 7.0f);
        const float alpha = (0.30f - m_playerHp / m_playerMaxHp) / 0.30f * (0.10f + 0.10f * pulse);
        DrawMesh(m_art.quad, Translate(origin + glm::vec3(0.0f, 0.0f, 3.50f)) * fullScreen,
                 glm::vec4(0.55f, 0.02f, 0.06f, alpha));
    }
    if (m_bossWarning > 0.0f) {
        const float pulse = 0.5f + 0.5f * std::sin(m_time * 9.0f);
        DrawMesh(m_art.quad, Translate(origin + glm::vec3(0.0f, 0.0f, 3.45f)) * fullScreen,
                 glm::vec4(0.40f, 0.02f, 0.05f, 0.10f * m_bossWarning * pulse));
    }
}

// ============================ ImGui ============================

ImVec2 SurvivorsLayer::WorldToScreen(const glm::vec2& world) const
{
    const float scale = static_cast<float>(m_viewHeight) / (2.0f * VIEW_HALF_HEIGHT);
    return ImVec2(
        static_cast<float>(m_viewWidth) * 0.5f + (world.x - m_cameraFocus.x) * scale,
        static_cast<float>(m_viewHeight) * 0.5f - (world.y - m_cameraFocus.y) * scale);
}

void SurvivorsLayer::OnImGuiRender()
{
    if (!m_active) {
        return;
    }
    if (m_state == State::Title) {
        RenderTitle();
        return;
    }
    RenderHud();

    switch (m_state) {
        case State::LevelUp: RenderLevelUp(); break;
        case State::Paused: RenderPause(); break;
        case State::GameOver: RenderGameOver(); break;
        case State::Victory: RenderVictory(); break;
        default: break;
    }
}

void SurvivorsLayer::RenderTitle()
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), display, IM_COL32(3, 3, 10, 150));

    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, display.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500.0f, 0.0f), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.10f, 0.94f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.46f, 0.38f, 0.72f, 0.55f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28.0f, 24.0f));
    if (ImGui::Begin("##survivors-title-screen", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::SetWindowFontScale(2.4f);
        ImGui::TextColored(ImVec4(0.95f, 0.90f, 0.72f, 1.0f), "永 夜 幸 存 者");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        ImGui::TextColored(ImVec4(0.66f, 0.70f, 0.86f, 1.0f), "撑过 180 秒的黑暗，活到黎明。武器会自动开火。");

        ImGui::Dummy(ImVec2(0.0f, 14.0f));
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.46f, 0.38f, 0.72f, 0.45f));
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0.0f, 16.0f));

        const float buttonWidth = ImGui::GetContentRegionAvail().x;
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.42f, 0.34f, 0.14f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.56f, 0.46f, 0.19f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.70f, 0.57f, 0.23f, 1.0f));
        if (ImGui::Button("开始游戏   (Enter / 空格)", ImVec2(buttonWidth, 46.0f))) {
            ResetRun();
        }
        ImGui::PopStyleColor(3);

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.34f, 0.10f, 0.12f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.50f, 0.14f, 0.17f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.66f, 0.18f, 0.20f, 1.0f));
        if (ImGui::Button("退出游戏", ImVec2(buttonWidth, 40.0f))) {
            Application::Get().Close();
        }
        ImGui::PopStyleColor(3);

        ImGui::Dummy(ImVec2(0.0f, 14.0f));
        ImGui::TextColored(ImVec4(0.52f, 0.56f, 0.70f, 0.9f),
                           "WASD / 方向键移动   ·   自动攻击   ·   ESC 暂停   ·   F4 调试");

        // ImGui 可能把键盘事件标记为已处理，这里用 ImGui 的输入状态补一条开始路径
        if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Space)) {
            ResetRun();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

void SurvivorsLayer::RenderHud()
{
    const ImGuiIO& io = ImGui::GetIO();
    const ImVec2 display = io.DisplaySize;
    ImDrawList* background = ImGui::GetBackgroundDrawList();

    const ImVec4 colText(0.90f, 0.92f, 0.98f, 1.0f);
    const ImVec4 colDim(0.60f, 0.64f, 0.78f, 0.85f);
    const ImVec4 colHp(0.88f, 0.26f, 0.32f, 1.0f);
    const ImVec4 colXp(0.42f, 0.82f, 1.00f, 1.0f);

    // 顶部经验条
    const float expFraction = m_expNext > 0
        ? std::clamp(static_cast<float>(m_exp) / static_cast<float>(m_expNext), 0.0f, 1.0f)
        : 0.0f;
    background->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(display.x, 8.0f), IM_COL32(10, 10, 22, 220));
    background->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(display.x * expFraction, 8.0f),
                              IM_COL32(84, 196, 255, 255));

    // ---- 左上：生命 / 等级 ----
    ImGui::SetNextWindowPos(ImVec2(18.0f, 22.0f));
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.10f, 0.55f));
    if (ImGui::Begin("##survivors-left", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoInputs)) {
        ImGui::TextColored(colDim, "生命");
        char hpText[64];
        std::snprintf(hpText, sizeof(hpText), "%.0f / %.0f", m_playerHp, m_playerMaxHp);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, colHp);
        ImGui::ProgressBar(m_playerMaxHp > 0.0f ? m_playerHp / m_playerMaxHp : 0.0f,
                           ImVec2(244.0f, 18.0f), hpText);
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, colText);
        ImGui::Text("等级 %d", m_level);
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextColored(colDim, "经验 %d / %d", m_exp, m_expNext);
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    // ---- 顶部中央：计时 / 击杀 ----
    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, 18.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.10f, 0.45f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    if (ImGui::Begin("##survivors-timer", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoInputs)) {
        ImGui::SetWindowFontScale(1.9f);
        ImGui::TextColored(colText, "%s", FormatTime(m_time).c_str());
        ImGui::SetWindowFontScale(1.0f);
        ImGui::TextColored(colDim, "击杀 %d   ·   距黎明 %s", m_kills,
                           FormatTime(std::max(0.0f, VICTORY_TIME - m_time)).c_str());
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // ---- 血月使徒血条（直接扫描存活的 Boss，避免多只同名 Boss 时血条丢失） ----
    {
        const Enemy* boss = nullptr;
        for (const Enemy& enemy : m_enemies) {
            if (enemy.alive && enemy.type == EnemyType::Boss) {
                boss = &enemy;
                break;
            }
        }
        if (boss != nullptr) {
            ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, 96.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
            ImGui::SetNextWindowBgAlpha(0.0f);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.02f, 0.04f, 0.55f));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.86f, 0.16f, 0.22f, 1.0f));
            if (ImGui::Begin("##survivors-boss", nullptr,
                             ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoInputs)) {
                ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.55f, 1.0f), "血月使徒");
                ImGui::ProgressBar(boss->maxHp > 0.0f ? boss->hp / boss->maxHp : 0.0f,
                                   ImVec2(430.0f, 14.0f), "");
            }
            ImGui::End();
            ImGui::PopStyleColor(2);
        }
    }

    // ---- 左下：武器 ----
    ImGui::SetNextWindowPos(ImVec2(18.0f, display.y - 18.0f), ImGuiCond_Always, ImVec2(0.0f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.10f, 0.50f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    if (ImGui::Begin("##survivors-weapons", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoInputs)) {
        ImGui::TextColored(colDim, "武器");
        for (int weapon = 0; weapon < 4; ++weapon) {
            const int level = m_weaponLevel[weapon];
            if (level <= 0) {
                continue;
            }
            ImGui::TextColored(ImVec4(WEAPON_COLORS[weapon].x, WEAPON_COLORS[weapon].y,
                                      WEAPON_COLORS[weapon].z, 1.0f),
                               "%s  %s", WEAPON_NAMES[weapon], ROMAN[level]);
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // ---- 右下：被动 ----
    ImGui::SetNextWindowPos(ImVec2(display.x - 18.0f, display.y - 18.0f), ImGuiCond_Always, ImVec2(1.0f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.10f, 0.50f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    if (ImGui::Begin("##survivors-passives", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoInputs)) {
        ImGui::TextColored(colDim, "祝福");
        bool any = false;
        for (int passive = 0; passive < static_cast<int>(PassiveId::Count); ++passive) {
            const int level = m_passive[passive];
            if (level <= 0) {
                continue;
            }
            any = true;
            ImGui::SameLine(0.0f, 12.0f);
            ImGui::TextColored(ImVec4(0.72f, 0.82f, 0.98f, 0.9f), "%s %s", PASSIVE_NAMES[passive], ROMAN[level]);
        }
        if (!any) {
            ImGui::TextColored(ImVec4(0.5f, 0.55f, 0.65f, 0.7f), "暂无");
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // ---- 伤害数字 ----
    ImFont* font = ImGui::GetFont();
    for (const DamageNumber& number : m_damageNumbers) {
        const float t = std::min(number.age / number.lifetime, 1.0f);
        const ImVec2 screen = WorldToScreen(number.position);
        const int alpha = static_cast<int>(255.0f * (1.0f - t) * (1.0f - t));
        const ImU32 color = number.crit ? IM_COL32(255, 224, 130, alpha) : IM_COL32(228, 236, 255, alpha);
        const float fontSize = number.crit ? 23.0f : 16.0f;
        char text[32];
        std::snprintf(text, sizeof(text), "%.0f", number.value);
        const ImVec2 size = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
        background->AddText(font, fontSize, ImVec2(screen.x - size.x * 0.5f, screen.y), color, text);
    }

    if (m_state == State::Playing && m_time < 18.0f) {
        const float alpha = m_time < 12.0f ? 1.0f : std::clamp((18.0f - m_time) / 6.0f, 0.0f, 1.0f);
        ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, display.y - 96.0f), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
        ImGui::SetNextWindowBgAlpha(0.0f);
        if (ImGui::Begin("##survivors-hint", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoInputs)) {
            ImGui::TextColored(ImVec4(0.70f, 0.76f, 0.92f, alpha * 0.85f),
                               "WASD / 方向键移动   ·   自动攻击   ·   升级时点击卡片（或按 1 / 2 / 3）   ·   ESC 暂停");
        }
        ImGui::End();
    }

    if (m_bossWarning > 0.0f) {
        const float pulse = 0.55f + 0.45f * std::sin(m_time * 8.0f);
        ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, display.y * 0.30f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.0f);
        if (ImGui::Begin("##survivors-warning", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoInputs)) {
            ImGui::SetWindowFontScale(1.4f);
            ImGui::TextColored(ImVec4(1.0f, 0.30f, 0.32f, m_bossWarning * pulse), "血月使徒正在苏醒……");
            ImGui::SetWindowFontScale(1.0f);
        }
        ImGui::End();
    }

    if (m_showDebug) {
        ImGui::SetNextWindowPos(ImVec2(18.0f, 150.0f));
        ImGui::SetNextWindowBgAlpha(0.75f);
        if (ImGui::Begin("调试##survivors", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::Text("FPS %.1f (%.2f ms)", io.Framerate, io.Framerate > 0.0f ? 1000.0f / io.Framerate : 0.0f);
            ImGui::Text("状态 %s", m_state == State::Playing ? "游戏中" : m_state == State::LevelUp ? "升级" :
                                 m_state == State::Paused ? "暂停" : m_state == State::GameOver ? "阵亡" : "胜利");
            ImGui::Text("时间 %.1f s   击杀 %d", m_time, m_kills);
            ImGui::Text("敌人 %d   弹丸 %d   宝石 %d   粒子 %d",
                        static_cast<int>(m_enemies.size()), static_cast<int>(m_projectiles.size()),
                        static_cast<int>(m_gems.size()), static_cast<int>(m_particles.size()));
            ImGui::Text("伤害总计 %.0f", m_damageDealt);
            ImGui::Text("玩家 (%.0f, %.0f)  HP %.0f", m_playerPosition.x, m_playerPosition.y, m_playerHp);
            ImGui::Separator();
            if (ImGui::Button("重新开始")) {
                ResetRun();
            }
            ImGui::SameLine();
            if (ImGui::Button("关闭调试 (F4)")) {
                m_showDebug = false;
            }
        }
        ImGui::End();
    }
}

void SurvivorsLayer::RenderLevelUp()
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), display, IM_COL32(4, 4, 12, 205));

    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, display.y * 0.5f - 268.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(0.0f);
    if (ImGui::Begin("##survivors-levelup-title", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoInputs)) {
        ImGui::SetWindowFontScale(2.0f);
        ImGui::TextColored(ImVec4(1.0f, 0.86f, 0.50f, 1.0f), "等 级 提 升");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::TextColored(ImVec4(0.66f, 0.70f, 0.86f, 0.95f), "点击卡片选择一项祝福（也可按 1 / 2 / 3）");
    }
    ImGui::End();

    const float cardWidth = 286.0f;
    const float cardHeight = 296.0f;
    const float gap = 22.0f;
    const int count = static_cast<int>(m_options.size());
    const float totalWidth = cardWidth * static_cast<float>(count) + gap * static_cast<float>(count - 1);
    const float startX = (display.x - totalWidth) * 0.5f;
    const float startY = display.y * 0.5f - cardHeight * 0.5f + 54.0f;

    int picked = -1;
    for (int i = 0; i < count; ++i) {
        const UpgradeOption& option = m_options[i];
        ImGui::SetNextWindowPos(ImVec2(startX + static_cast<float>(i) * (cardWidth + gap), startY), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(cardWidth, cardHeight), ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.055f, 0.055f, 0.11f, 0.97f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(option.color.x, option.color.y, option.color.z, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(option.color.x * 0.42f, option.color.y * 0.42f,
                                                      option.color.z * 0.42f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(option.color.x * 0.62f, option.color.y * 0.62f,
                                                             option.color.z * 0.62f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(option.color.x * 0.80f, option.color.y * 0.80f,
                                                            option.color.z * 0.80f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 16.0f));

        char identifier[32];
        std::snprintf(identifier, sizeof(identifier), "##card%d", i);
        ImGui::Begin(identifier, nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);

        ImGui::TextColored(ImVec4(option.color.x, option.color.y, option.color.z, 1.0f), "%s", option.tag.c_str());
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::SetWindowFontScale(1.45f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 1.0f, 1.0f));
        ImGui::TextWrapped("%s", option.title.c_str());
        ImGui::PopStyleColor();
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.76f, 0.80f, 0.92f, 1.0f));
        ImGui::TextWrapped("%s", option.description.c_str());
        ImGui::PopStyleColor();

        ImGui::SetCursorPosY(cardHeight - 66.0f);
        char buttonLabel[32];
        std::snprintf(buttonLabel, sizeof(buttonLabel), "选择  (%d)", i + 1);
        if (ImGui::Button(buttonLabel, ImVec2(cardWidth - 36.0f, 40.0f))) {
            picked = i;
        }

        ImGui::End();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(5);
    }

    if (picked >= 0) {
        ApplyUpgrade(picked);
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_1) && count > 0) {
        ApplyUpgrade(0);
    } else if (ImGui::IsKeyPressed(ImGuiKey_2) && count > 1) {
        ApplyUpgrade(1);
    } else if (ImGui::IsKeyPressed(ImGuiKey_3) && count > 2) {
        ApplyUpgrade(2);
    }
}

void SurvivorsLayer::RenderPause()
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), display, IM_COL32(3, 3, 10, 170));

    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, display.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(480.0f, 0.0f), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.12f, 0.96f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
    if (ImGui::Begin("##survivors-pause", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                                  ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SetWindowFontScale(1.8f);
        ImGui::TextColored(ImVec4(0.95f, 0.95f, 1.0f, 1.0f), "已 暂 停");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::TextColored(ImVec4(0.62f, 0.66f, 0.80f, 1.0f), "存活 %s   ·   击杀 %d   ·   等级 %d",
                           FormatTime(m_time).c_str(), m_kills, m_level);

        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.46f, 0.38f, 0.72f, 0.45f));
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        // ---- 最终属性（BaseStats 叠加所有成长后的快照）----
        ImGui::TextColored(ImVec4(0.58f, 0.62f, 0.76f, 1.0f), "最终属性");
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        char hpText[48];
        char damageText[32];
        char hasteText[32];
        char critText[32];
        char speedText[32];
        char pickupText[32];
        char regenText[32];
        char bladesText[32];
        std::snprintf(hpText, sizeof(hpText), "%.0f / %.0f", m_playerHp, m_playerMaxHp);
        std::snprintf(damageText, sizeof(damageText), "+%.0f%%", (DamageMul() - 1.0f) * 100.0f);
        std::snprintf(hasteText, sizeof(hasteText), "+%.0f%%", (AttackSpeedMul() - 1.0f) * 100.0f);
        std::snprintf(critText, sizeof(critText), "%.0f%%", CritChance() * 100.0f);
        std::snprintf(speedText, sizeof(speedText), "%.0f", MoveSpeed());
        std::snprintf(pickupText, sizeof(pickupText), "%.0f", PickupRadius());
        std::snprintf(regenText, sizeof(regenText), "+%.1f / 秒", m_finalStats.regenPerSecond);
        std::snprintf(bladesText, sizeof(bladesText), "%d 道",
                      BLADE.count[m_weaponLevel[static_cast<int>(WeaponId::MoonBlade)]]);

        const ImVec4 statLabel(0.58f, 0.62f, 0.76f, 1.0f);
        const ImVec4 statValue(0.92f, 0.94f, 1.00f, 1.0f);
        if (ImGui::BeginTable("##survivors-stats", 4, ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("##sl1", ImGuiTableColumnFlags_WidthFixed, 44.0f);
            ImGui::TableSetupColumn("##sv1", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("##sl2", ImGuiTableColumnFlags_WidthFixed, 44.0f);
            ImGui::TableSetupColumn("##sv2", ImGuiTableColumnFlags_WidthStretch, 1.0f);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextColored(statLabel, "生命");
            ImGui::TableSetColumnIndex(1); ImGui::TextColored(statValue, "%s", hpText);
            ImGui::TableSetColumnIndex(2); ImGui::TextColored(statLabel, "伤害");
            ImGui::TableSetColumnIndex(3); ImGui::TextColored(statValue, "%s", damageText);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextColored(statLabel, "攻速");
            ImGui::TableSetColumnIndex(1); ImGui::TextColored(statValue, "%s", hasteText);
            ImGui::TableSetColumnIndex(2); ImGui::TextColored(statLabel, "暴击");
            ImGui::TableSetColumnIndex(3); ImGui::TextColored(statValue, "%s", critText);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextColored(statLabel, "移速");
            ImGui::TableSetColumnIndex(1); ImGui::TextColored(statValue, "%s", speedText);
            ImGui::TableSetColumnIndex(2); ImGui::TextColored(statLabel, "拾取");
            ImGui::TableSetColumnIndex(3); ImGui::TextColored(statValue, "%s", pickupText);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextColored(statLabel, "回复");
            ImGui::TableSetColumnIndex(1); ImGui::TextColored(statValue, "%s", regenText);
            ImGui::TableSetColumnIndex(2); ImGui::TextColored(statLabel, "飞刃");
            ImGui::TableSetColumnIndex(3); ImGui::TextColored(statValue, "%s", bladesText);
            ImGui::EndTable();
        }

        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::TextColored(statLabel, "武器");
        for (int weapon = 0; weapon < 4; ++weapon) {
            const int level = m_weaponLevel[weapon];
            if (level <= 0) {
                continue;
            }
            ImGui::SameLine(0.0f, 10.0f);
            ImGui::TextColored(ImVec4(WEAPON_COLORS[weapon].x, WEAPON_COLORS[weapon].y,
                                      WEAPON_COLORS[weapon].z, 1.0f),
                               "%s %s", WEAPON_NAMES[weapon], ROMAN[level]);
        }
        ImGui::TextColored(statLabel, "祝福");
        bool anyPassive = false;
        for (int passive = 0; passive < static_cast<int>(PassiveId::Count); ++passive) {
            const int level = m_passive[passive];
            if (level <= 0) {
                continue;
            }
            anyPassive = true;
            ImGui::SameLine(0.0f, 10.0f);
            ImGui::TextColored(ImVec4(0.72f, 0.82f, 0.98f, 0.95f), "%s %s", PASSIVE_NAMES[passive], ROMAN[level]);
        }
        if (!anyPassive) {
            ImGui::SameLine(0.0f, 10.0f);
            ImGui::TextColored(ImVec4(0.50f, 0.54f, 0.64f, 0.9f), "暂无");
        }

        ImGui::Dummy(ImVec2(0.0f, 12.0f));
        // 按钮宽度取内容区可用宽度，保证左右留白一致（写死宽度会因窗口更宽而右偏）
        const float buttonWidth = ImGui::GetContentRegionAvail().x;
        if (ImGui::Button("继续游戏  (ESC / P)", ImVec2(buttonWidth, 38.0f))) {
            m_state = State::Playing;
        }
        if (ImGui::Button("重新开始  (R)", ImVec2(buttonWidth, 38.0f))) {
            ResetRun();
        }
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.34f, 0.10f, 0.12f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.50f, 0.14f, 0.17f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.66f, 0.18f, 0.20f, 1.0f));
        if (ImGui::Button("退出游戏", ImVec2(buttonWidth, 38.0f))) {
            Application::Get().Close();
        }
        ImGui::PopStyleColor(3);

        // 暂停菜单出现时 ImGui 可能吃掉键盘事件，这里直接用 ImGui 的输入状态补一条 ESC/P 继续；
        // 开面板的同一帧仍可能在队列里，用几帧宽限期避开
        if (m_pauseGuardFrames > 0) {
            --m_pauseGuardFrames;
        } else if (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_P)) {
            m_state = State::Playing;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void SurvivorsLayer::RenderGameOver()
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), display, IM_COL32(8, 2, 4, 200));

    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, display.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.07f, 0.03f, 0.05f, 0.97f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
    if (ImGui::Begin("##survivors-gameover", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                                       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SetWindowFontScale(2.0f);
        ImGui::TextColored(ImVec4(0.95f, 0.28f, 0.32f, 1.0f), "你 被 黑 暗 吞 噬");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::TextColored(ImVec4(0.80f, 0.80f, 0.90f, 1.0f), "存活时间   %s", FormatTime(m_time).c_str());
        ImGui::TextColored(ImVec4(0.80f, 0.80f, 0.90f, 1.0f), "击杀数量   %d", m_kills);
        ImGui::TextColored(ImVec4(0.80f, 0.80f, 0.90f, 1.0f), "达到等级   %d", m_level);
        ImGui::TextColored(ImVec4(0.80f, 0.80f, 0.90f, 1.0f), "造成伤害   %.0f", m_damageDealt);
        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        if (ImGui::Button("再来一局  (R)", ImVec2(ImGui::GetContentRegionAvail().x, 42.0f))) {
            ResetRun();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void SurvivorsLayer::RenderVictory()
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), display, IM_COL32(4, 8, 18, 185));

    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, display.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(440.0f, 0.0f), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.07f, 0.13f, 0.97f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
    if (ImGui::Begin("##survivors-victory", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SetWindowFontScale(2.0f);
        ImGui::TextColored(ImVec4(1.0f, 0.86f, 0.50f, 1.0f), "黎 明 降 临");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::TextColored(ImVec4(0.75f, 0.80f, 0.95f, 1.0f), "你撑过了漫长的黑夜。");
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::TextColored(ImVec4(0.82f, 0.84f, 0.94f, 1.0f), "存活时间   %s", FormatTime(m_time).c_str());
        ImGui::TextColored(ImVec4(0.82f, 0.84f, 0.94f, 1.0f), "击杀数量   %d", m_kills);
        ImGui::TextColored(ImVec4(0.82f, 0.84f, 0.94f, 1.0f), "达到等级   %d", m_level);
        ImGui::TextColored(ImVec4(0.82f, 0.84f, 0.94f, 1.0f), "造成伤害   %.0f", m_damageDealt);
        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        if (ImGui::Button("再来一局  (R)", ImVec2(ImGui::GetContentRegionAvail().x, 42.0f))) {
            ResetRun();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ============================ 输入 ============================

void SurvivorsLayer::OnEvent(Event& event)
{
    if (event.GetEventType() != EventType::KeyPressed) {
        return;
    }
    const auto& keyEvent = static_cast<KeyPressedEvent&>(event);
    if (keyEvent.IsRepeat()) {
        return;
    }

    const int key = keyEvent.GetKeyCode();
    if (m_state == State::Title) {
        // 标题界面只响应开始；ESC 等快捷键留给游戏内
        if (key == Key::Enter || key == Key::Space) {
            ResetRun();
            event.handled = true;
        }
        return;
    }
    if (key == Key::Escape) {
        // ESC 不直接退出：暂停并弹出菜单，由玩家选择继续 / 重新开始 / 退出
        if (m_state == State::Playing) {
            m_state = State::Paused;
            m_pauseGuardFrames = 2;
        } else if (m_state == State::Paused) {
            m_state = State::Playing;
        }
        event.handled = true;
        return;
    }
    if (key == Key::R) {
        ResetRun();
        event.handled = true;
        return;
    }
    if (key == Key::P) {
        if (m_state == State::Playing) {
            m_state = State::Paused;
            m_pauseGuardFrames = 2;
        } else if (m_state == State::Paused) {
            m_state = State::Playing;
        }
        event.handled = true;
        return;
    }
    if (key == Key::F4) {
        m_showDebug = !m_showDebug;
        event.handled = true;
        return;
    }
    if (m_state == State::LevelUp) {
        if (key >= Key::D1 && key <= Key::D3) {
            const int index = key - Key::D1;
            if (index < static_cast<int>(m_options.size())) {
                ApplyUpgrade(index);
            }
            event.handled = true;
        }
    }
}
