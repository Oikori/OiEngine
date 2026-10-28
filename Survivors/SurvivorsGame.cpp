// 《永夜幸存者》主循环与生命周期。
//
// 本文件负责：BaseStats 定义、跟随相机、开局 / 重开 / 回标题、音频（BGM + 音效）、
// 关卡推进、玩家移动、相机跟随、输入事件。其余职责见 SurvivorsGame.h 顶部的文件分工表。
#include "SurvivorsGame.h"
#include "SurvivorsInternal.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <string>

#include <glm/gtc/matrix_transform.hpp>

#include "Engine/Core/Input.h"
#include "Engine/Events/KeyEvent.h"

using namespace OiEngine;
using namespace OiEngine::Survivors;

// ============================ 属性 ============================

// BaseStats 是嵌套类型：GCC 不允许在类内用默认成员初始值初始化 constexpr 静态成员，
// 因此这里在类外给出唯一一份定义（类内只声明）。
const SurvivorsLayer::BaseStats SurvivorsLayer::BARE_STATS{};

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
// ============================ 生命周期 ============================

SurvivorsLayer::SurvivorsLayer()
    : Layer("Survivors")
{
}

void SurvivorsLayer::OnAttach()
{
    CreateShaders();
    m_art.Load();
    LoadAudio();
    RenderCommand::SetClearColor(glm::vec4(0.030f, 0.028f, 0.062f, 1.0f));
    ReturnToTitle();
    OI_INFO("Survivors OnAttach: 永夜幸存者已启动");
}

void SurvivorsLayer::ReturnToTitle()
{
    // ResetRun 会重置关卡 / 属性 / 音频并刷开局蝙蝠；标题画面不跑游戏逻辑，
    // 所以接着切 Title 并把刚刷出来的敌人与残留特效清掉
    ResetRun();
    m_state = State::Title;
    m_enemies.clear();
    m_projectiles.clear();
    m_coinDrops.clear();
    m_particles.clear();
    m_effects.clear();
    m_damageNumbers.clear();
    m_grid.cells.clear();
    EnsureBgm(false);
    OI_INFO("Survivors: 标题界面就绪");
}

void SurvivorsLayer::OnDetach()
{
    if (m_bgm) {
        m_bgm->Stop();
        m_bgm.reset();
    }
    m_art.Unload();
}

void SurvivorsLayer::CreateShaders()
{
    m_shader = Shader::Create("assets/shader/scenes/survivors.glsl");
}

// ============================ 音频 ============================

void SurvivorsLayer::LoadAudio()
{
    if (!AudioEngine::IsInitialized()) {
        OI_WARN("Survivors: 音频设备不可用，本局静音");
        return;
    }

    m_bgmClip = AudioEngine::LoadClip("assets/audio/bgm.wav", /*streaming=*/true);
    m_bgmBossClip = AudioEngine::LoadClip("assets/audio/bgm_boss.wav", /*streaming=*/true);
    for (int i = 0; i < static_cast<int>(SfxId::Count); ++i) {
        m_sfxClips[i] = AudioEngine::LoadClip(std::string("assets/audio/") + SFX_FILES[i]);
        m_sfxThrottle[i] = 0.0f;
    }

    AudioEngine::SetMasterVolume(m_masterVolume);
    AudioEngine::SetMuted(m_audioMuted);
    OI_INFO("Survivors: 音频资源加载完成（2 首 BGM + {} 个音效）", static_cast<int>(SfxId::Count));
}

void SurvivorsLayer::EnsureBgm(bool boss)
{
    if (!AudioEngine::IsInitialized()) {
        return;
    }
    // 已经在放同一首：只负责恢复（暂停 / 过关后接着放），不重新起播
    if (m_bgm && !m_bgm->IsFinished() && m_bgmIsBoss == boss) {
        if (!m_bgm->IsPlaying()) {
            m_bgm->Resume();
        }
        return;
    }

    const Ref<AudioClip>& clip = boss ? m_bgmBossClip : m_bgmClip;
    if (!clip) {
        return;
    }
    if (m_bgm) {
        m_bgm->Stop();
        m_bgm.reset();
    }

    AudioPlaybackSettings settings;
    settings.volume = (boss ? BGM_BOSS_VOLUME : BGM_VOLUME) * m_bgmVolume;
    settings.looping = true;
    m_bgm = clip->Play(settings);
    m_bgmIsBoss = boss;
    OI_INFO("Survivors: BGM 切换到{}", boss ? "血月使徒曲" : "主曲");
}

void SurvivorsLayer::ApplyBgmVolume()
{
    if (m_bgm) {
        m_bgm->SetVolume((m_bgmIsBoss ? BGM_BOSS_VOLUME : BGM_VOLUME) * m_bgmVolume);
    }
}

void SurvivorsLayer::PlaySfx(SfxId id, float volumeScale, float pitch)
{
    if (!AudioEngine::IsInitialized()) {
        return;
    }
    const int index = static_cast<int>(id);
    if (index < 0 || index >= static_cast<int>(SfxId::Count)) {
        return;
    }
    if (!m_sfxClips[index]) {
        return;
    }
    if (m_sfxThrottle[index] > 0.0f) {
        return;   // 同一种音效还没过冷却，直接丢
    }
    m_sfxThrottle[index] = SFX_THROTTLE[index];

    AudioPlaybackSettings settings;
    settings.volume = SFX_VOLUME[index] * volumeScale * m_sfxVolume;
    settings.pitch = pitch > 0.0f ? pitch : 1.0f;
    m_sfxClips[index]->Play(settings);
}

void SurvivorsLayer::UpdateSfxThrottle(float deltaTime)
{
    for (float& timer : m_sfxThrottle) {
        timer = std::max(0.0f, timer - deltaTime);
    }
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

    // 关卡：回到第 1 关（30 秒），横幅提示开场
    m_stage = 1;
    m_stageTime = 0.0f;
    m_stageDuration = StageDuration(m_stage);
    m_stageBanner = 2.2f;
    m_stageBannerBoss = false;
    m_showHelp = false;
    m_showOptions = false;

    // 新一局：音效冷却清零，BGM 回到主曲（从 Boss 曲切回来时也走这里）
    m_sfxThrottle.fill(0.0f);
    EnsureBgm(false);

    m_playerPosition = glm::vec2(0.0f, 0.0f);
    m_playerVelocity = glm::vec2(0.0f, 0.0f);

    // 属性：回到开局裸值、清零被动，再同步一次生命上限
    m_baseStats = BARE_STATS;
    m_passive = { 0, 0, 0, 0, 0, 0, 0 };
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

    // 金币：新一局清零（货币与刷新价都从头开始）
    m_coins = 0;
    m_coinsEarned = 0;
    m_rerollCount = 0;

    m_enemies.clear();
    m_projectiles.clear();
    m_coinDrops.clear();
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
    // 升级已经把成长直接累加进 m_baseStats，这里只同步生命上限并钳制当前血量。
    m_playerMaxHp = m_baseStats.maxHp;
    m_playerHp = std::min(m_playerHp, m_playerMaxHp);
}

float SurvivorsLayer::DamageMul() const
{
    return m_baseStats.damageMul;
}

float SurvivorsLayer::AttackSpeedMul() const
{
    return m_baseStats.attackSpeedMul;
}

float SurvivorsLayer::CritChance() const
{
    return m_baseStats.critChance;
}

float SurvivorsLayer::MoveSpeed() const
{
    return m_baseStats.moveSpeed;
}

float SurvivorsLayer::PickupRadius() const
{
    return m_baseStats.pickupRadius;
}

float SurvivorsLayer::MaxHp() const
{
    return m_baseStats.maxHp;
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
    m_stageBanner = std::max(0.0f, m_stageBanner - deltaTime);
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
    UpdateSfxThrottle(deltaTime);

    UpdatePlayer(deltaTime);
    UpdateEnemies(deltaTime);
    UpdateEnemySpawning(deltaTime);
    UpdateWeapons(deltaTime);
    UpdateProjectiles(deltaTime);
    UpdateCoins(deltaTime);
    UpdateContactDamage(deltaTime);
    UpdateParticles(deltaTime);
    UpdateEffects(deltaTime);

    std::erase_if(m_enemies, [](const Enemy& enemy) { return !enemy.alive; });
    std::erase_if(m_projectiles, [](const Projectile& projectile) { return !projectile.alive; });
    std::erase_if(m_coinDrops, [](const Coin& coin) { return !coin.alive; });

    // 关卡计时：撑过本关时长就过关；第 10 关撑满即通关
    m_stageTime += deltaTime;
    if (m_stageTime >= m_stageDuration && m_state == State::Playing) {
        if (m_stage >= STAGE_COUNT) {
            m_state = State::Victory;
            m_levelFlash = 1.0f;
            m_shake = 12.0f;
            if (m_bgm) {
                m_bgm->Stop();
                m_bgm.reset();
            }
            PlaySfx(SfxId::Victory);
            OI_INFO("Survivors: 10 关全清，玩家撑到了黎明（总耗时 {} 秒）", static_cast<int>(m_time));
            return;
        }
        AdvanceStage();
    }

    // 刚关掉升级面板时给一小段缓冲：避免金币还在往身上飞时又弹一次面板
    m_levelUpLockout = std::max(0.0f, m_levelUpLockout - deltaTime);
    if (m_pendingLevelUps > 0 && m_levelUpLockout <= 0.0f && m_state == State::Playing) {
        StartLevelUp();
    }
}

void SurvivorsLayer::AdvanceStage()
{
    ++m_stage;
    m_stageTime = 0.0f;
    m_stageDuration = StageDuration(m_stage);
    m_stageBanner = 2.2f;
    m_stageBannerBoss = HasBossStage(m_stage);

    // 过关清场：残留敌人与弹丸不带进下一关；地上的金币直接收进钱包（经验是击杀时已经给过的）
    for (const Coin& coin : m_coinDrops) {
        if (coin.alive) {
            m_coins += coin.value;
            m_coinsEarned += coin.value;
        }
    }
    m_enemies.clear();
    m_projectiles.clear();
    m_coinDrops.clear();
    m_grid.cells.clear();

    m_spawnTimer = 0.8f;
    m_shake = std::max(m_shake, 6.0f);
    SpawnRing(m_playerPosition, 240.0f, 0.6f, C_GOLD);

    // Boss 关直接切到 Boss 曲，普通关切回主曲
    EnsureBgm(m_stageBannerBoss);

    if (m_stageBannerBoss) {
        SpawnBoss();
        PlaySfx(SfxId::BossSpawn);
        OI_INFO("Survivors: 进入第 {} / {} 关（{} 秒）——血月使徒登场", m_stage, STAGE_COUNT,
                static_cast<int>(m_stageDuration));
    } else {
        PlaySfx(SfxId::StageClear);
        OI_INFO("Survivors: 进入第 {} / {} 关（{} 秒）", m_stage, STAGE_COUNT,
                static_cast<int>(m_stageDuration));
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

    if (m_baseStats.regenPerSecond > 0.0f && m_playerHp < m_playerMaxHp) {
        m_playerHp = std::min(m_playerMaxHp, m_playerHp + m_baseStats.regenPerSecond * deltaTime);
    }
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
    if (m_showHelp || m_showOptions) {
        // 帮助 / 设置面板打开时只处理关闭：ESC 收起面板，不顺手开始 / 暂停 / 重开
        if (key == Key::Escape) {
            m_showHelp = false;
            m_showOptions = false;
            event.handled = true;
        }
        return;
    }
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

