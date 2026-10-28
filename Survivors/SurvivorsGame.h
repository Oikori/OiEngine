#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include <imgui.h>

#include "OiEngine.h"
#include "SurvivorsArt.h"

// 《永夜幸存者》——类《吸血鬼幸存者》的俯视角自动战斗生存游戏。
//
// 玩法：WASD 移动，武器自动索敌开火；击杀敌人**直接获得经验**，掉落物是金币（刷新升级卡用）。
// 升级时从三张卡里选一项强化；属性卡没有层数上限，但有品质（普通 / 高级 / 稀有 / 史诗）。
// 撑到黎明（第 10 关结束）即胜利。所有美术资源都是程序化几何，无图片依赖。
//
// 代码分工（所有 .cpp 都只定义 SurvivorsLayer 的成员函数，共享本头文件）：
//   SurvivorsGame.cpp     主循环 / 生命周期 / 音频 / 关卡推进 / 玩家 / 相机 / 输入
//   SurvivorsEnemies.cpp  敌人：空间哈希、刷怪、移动与分离、接触伤害、Boss
//   SurvivorsWeapons.cpp  武器：索敌、四把武器的开火与冷却、弹丸推进
//   SurvivorsCombat.cpp   伤害与击杀、金币掉落与拾取、粒子 / 光环 / 伤害数字
//   SurvivorsUpgrade.cpp  经验与升级卡（卡池、品质、整组刷新、属性结算）
//   SurvivorsRender.cpp   世界渲染（程序化网格的绘制顺序与变换）
//   SurvivorsUi.cpp       全部 ImGui 面板（标题 / HUD / 升级 / 暂停 / 结算 / 帮助 / 设置）
//   SurvivorsArt.*        程序化几何库
//   SurvivorsInternal.h   内部共享的调参表 / 调色板 / 小工具（不要 include 到工程外）
namespace OiEngine::Survivors {

    // 跟随玩家的正交相机（世界单位与像素 1:1 于 900 高度基准）
    class FollowCamera final : public Camera {
    public:
        void SetFocus(const glm::vec2& focus) { m_focus = focus; }
        void SetShake(const glm::vec2& shake) { m_shake = shake; }

        glm::vec3 GetPosition() const override;
        glm::mat4 GetViewMatrix() const override;
        glm::mat4 GetProjectionMatrix() const override;

    private:
        glm::vec2 m_focus{ 0.0f, 0.0f };
        glm::vec2 m_shake{ 0.0f, 0.0f };
    };

    class SurvivorsLayer final : public Layer {
    public:
        SurvivorsLayer();
        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate() override;
        void OnImGuiRender() override;
        void OnEvent(Event& event) override;

    private:
        // ---- 类型 ----
        enum class State { Title, Playing, LevelUp, Paused, GameOver, Victory };
        enum class EnemyType { Bat = 0, Ghoul, Wraith, Brute, Boss, Count };
        enum class WeaponId { MoonBlade = 0, Aura, Scythe, Storm, Count };
        enum class PassiveId { Might = 0, Haste, Swift, Vitality, Magnet, Luck, Regen, Bulwark, Sunder, Count };
        enum class ShotKind { Blade, Scythe };
        enum class FxKind { Ring, Flash, Bolt };
        enum class UpgradeKind { Weapon, Passive, Heal };
        // 卡片品质：作用于**属性卡（被动）与补给卡**，决定加成幅度与出现概率。
        // 武器卡是固定的等级进阶，不带品质（tag 显示 Lv.N，颜色用武器色）。
        enum class Rarity { Common = 0, Advanced, Rare, Epic, Count };
        // 音效槽：顺序与 cpp 里的 SFX_FILES / SFX_VOLUME / SFX_THROTTLE 一致
        enum class SfxId {
            Blade = 0, Storm, Hit, Kill, Pickup, LevelUp, Hurt,
            StageClear, BossSpawn, Victory, GameOver, Count
        };

        struct Enemy {
            EnemyType type = EnemyType::Bat;
            glm::vec2 position{ 0.0f, 0.0f };
            glm::vec2 velocity{ 0.0f, 0.0f };
            glm::vec2 impulse{ 0.0f, 0.0f };
            float hp = 1.0f;
            float maxHp = 1.0f;
            float radius = 16.0f;
            float speed = 120.0f;
            float damage = 5.0f;
            float age = 0.0f;
            float phase = 0.0f;
            float hitFlash = 0.0f;
            int xp = 1;
            int coins = 1;      // 击杀后掉落的金币数量（经验是击杀当场直接给的）
            // 怪物只有防御（没有穿甲）：防御是固定值，减伤率 = 防御 / (防御 + 100)
            float defense = 0.0f;
            bool alive = true;
            uint32_t id = 0;
        };

        struct Projectile {
            ShotKind kind = ShotKind::Blade;
            glm::vec2 position{ 0.0f, 0.0f };
            glm::vec2 velocity{ 0.0f, 0.0f };
            float damage = 0.0f;
            float radius = 14.0f;
            float rotation = 0.0f;
            float spin = 0.0f;
            float age = 0.0f;
            float lifetime = 1.2f;
            int pierce = 1;
            bool returning = false;
            bool alive = true;
            std::vector<uint32_t> hitIds;  // 回旋镰：同一把镰刀对同一敌人只结算一次
        };

        struct Coin {
            glm::vec2 position{ 0.0f, 0.0f };
            glm::vec2 velocity{ 0.0f, 0.0f };
            int value = 1;
            float age = 0.0f;
            float lifetime = 22.0f;   // 地上的金币会消失，避免刷出来的钱永远躺在场上
            float phase = 0.0f;
            int tier = 0;             // 面额分档，只影响贴图大小
            bool alive = true;
        };

        struct Particle {
            glm::vec2 position{ 0.0f, 0.0f };
            glm::vec2 velocity{ 0.0f, 0.0f };
            float size = 4.0f;
            float age = 0.0f;
            float lifetime = 0.5f;
            float drag = 3.0f;
            glm::vec4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
        };

        struct Fx {
            FxKind kind = FxKind::Ring;
            glm::vec2 position{ 0.0f, 0.0f };
            float radius = 40.0f;
            float age = 0.0f;
            float lifetime = 0.4f;
            float rotation = 0.0f;
            glm::vec4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
        };

        struct DamageNumber {
            glm::vec2 position{ 0.0f, 0.0f };
            glm::vec2 drift{ 0.0f, 40.0f };
            float value = 0.0f;
            float age = 0.0f;
            float lifetime = 0.65f;
            bool crit = false;
            bool gold = false;   // 金币拾取提示（金色 +N，不参与暴击判定）
        };

        struct UpgradeOption {
            UpgradeKind kind = UpgradeKind::Passive;
            int index = 0;
            Rarity rarity = Rarity::Common;
            std::string tag;
            std::string title;
            std::string description;
            glm::vec4 color{ 0.5f, 0.5f, 0.5f, 1.0f };
        };

        // 属性卡的单张加成（primary / secondary 的含义随被动不同，见 PassiveGainFor）
        struct PassiveGain {
            float primary = 0.0f;
            float secondary = 0.0f;
        };

        // 敌人空间哈希：避免 400+ 敌人时的 O(n²) 邻近查询
        struct EnemyGrid {
            static constexpr float CELL_SIZE = 96.0f;
            std::unordered_map<uint64_t, std::vector<uint32_t>> cells;

            void Build(const std::vector<Enemy>& enemies);

            template <typename Func>
            void ForEachNear(const glm::vec2& center, float radius, Func&& func) const
            {
                const int x0 = static_cast<int>(std::floor((center.x - radius) / CELL_SIZE));
                const int x1 = static_cast<int>(std::floor((center.x + radius) / CELL_SIZE));
                const int y0 = static_cast<int>(std::floor((center.y - radius) / CELL_SIZE));
                const int y1 = static_cast<int>(std::floor((center.y + radius) / CELL_SIZE));
                for (int y = y0; y <= y1; ++y) {
                    for (int x = x0; x <= x1; ++x) {
                        const auto it = cells.find(Key(x, y));
                        if (it == cells.end()) {
                            continue;
                        }
                        for (const uint32_t index : it->second) {
                            func(index);
                        }
                    }
                }
            }

        private:
            static uint64_t Key(int x, int y)
            {
                return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) |
                       static_cast<uint32_t>(y);
            }
        };

        // ---- 初始化 ----
        void CreateShaders();
        void ResetRun();
        void ReturnToTitle();   // 清局并切回标题界面（结算窗口的「回到标题」也用它）
        void InitWisps();

        // ---- 音频（Survivors/assets/audio，构建后复制到输出目录）----
        void LoadAudio();
        void EnsureBgm(bool boss);                       // 切换 / 恢复 当前场景该放的那首
        void PlaySfx(SfxId id, float volumeScale = 1.0f, float pitch = 1.0f);
        void UpdateSfxThrottle(float deltaTime);
        void ApplyBgmVolume();                           // 设置面板改背景音乐音量时，实时作用到正在播的声部

        // ---- 更新 ----
        void UpdateGame(float deltaTime);
        void UpdatePlayer(float deltaTime);
        void UpdateEnemySpawning(float deltaTime);
        void AdvanceStage();
        void UpdateEnemies(float deltaTime);
        void UpdateWeapons(float deltaTime);
        void UpdateProjectiles(float deltaTime);
        void UpdateCoins(float deltaTime);
        void UpdateParticles(float deltaTime);
        void UpdateEffects(float deltaTime);
        void UpdateContactDamage(float deltaTime);
        void UpdateCamera(float deltaTime);

        // ---- 武器 ----
        bool FireMoonBlade();
        void PulseAura();
        bool ThrowScythes();
        bool CastStorm();

        // ---- 战斗 ----
        Enemy* FindNearestEnemy(const glm::vec2& origin, float range);
        void DamageEnemy(Enemy& enemy, float damage, bool crit);
        void KillEnemy(Enemy& enemy);
        void DamagePlayer(float amount);
        void SpawnEnemy(EnemyType type, const glm::vec2& position);
        void SpawnBoss();
        glm::vec2 EdgeSpawnPoint(float margin);
        void SpawnCoin(const glm::vec2& position, int value, bool burst);
        void SpawnParticles(const glm::vec2& position, int count, const glm::vec4& color,
                            float speed, float size, float lifetime, float zBias = 0.0f);
        void SpawnRing(const glm::vec2& position, float radius, float lifetime,
                       const glm::vec4& color);
        void SpawnFlash(const glm::vec2& position, float radius, float lifetime,
                        const glm::vec4& color);
        void SpawnDamageNumber(const glm::vec2& position, float value, bool crit);
        void SpawnCoinNumber(const glm::vec2& position, int value);
        void GainExp(int amount);

        // ---- 升级 ----
        void StartLevelUp();
        void BuildUpgradeOptions(const std::vector<UpgradeOption>& avoid = {});
        std::vector<UpgradeOption> BuildUpgradePool() const;   // 当前可选的全部卡（未定品质、未洗牌）
        UpgradeOption RollOption(const UpgradeOption& base);   // 给一张基础卡掷品质并生成文案
        Rarity RollRarity();
        int RerollCost() const;                                // 本次选取下一次刷新要花的金币
        void RerollOptions();                                  // 花金币整组重抽三张
        static bool SameCard(const UpgradeOption& a, const UpgradeOption& b);
        static PassiveGain PassiveGainFor(int passive, float multiplier);
        static float HealFraction(Rarity rarity);              // 血之祭按品质回复多少比例
        void ApplyUpgrade(int optionIndex);
        void DescribePassive(int passive, float multiplier, std::string& title, std::string& desc) const;

        // ---- 渲染 ----
        void DrawMesh(const Ref<VertexArray>& mesh, const glm::mat4& transform,
                      const glm::vec4& tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), float falloff = 0.0f);
        void RenderWorld();
        void DrawBackground();
        void DrawGround();
        void DrawCoins();
        void DrawEnemies();
        void DrawPlayer();
        void DrawProjectiles();
        void DrawParticles();
        void DrawEffectsUnder();
        void DrawEffectsOver();
        void DrawScreenOverlay();
        void RenderTitle();
        void RenderHud();
        void RenderHelp();
        void RenderOptions();
        void RenderLevelUp();
        void RenderPause();
        void RenderGameOver();
        void RenderVictory();
        ImVec2 WorldToScreen(const glm::vec2& world) const;

        // ---- 随机 ----
        float Rand(float minValue, float maxValue);
        int RandInt(int minValue, int maxValue);

        // ---- 属性模型 ----
        // 基础属性：一局内唯一的属性数据，战斗与 UI 都只读这一份。
        // 升级（被动）**直接累加**到这里，不再在汇总时按被动等级乘算；
        // 因此这里的值 = 开局裸值 + 全部成长，ResetRun() 会重置回 BARE_STATS。
        struct BaseStats {
            float maxHp = 20.0f;          // 初始生命值（体魄每级 +8，血之祭回复 35%）
            float moveSpeed = 240.0f;
            float damageMul = 1.0f;
            float attackSpeedMul = 1.0f;
            float critChance = 0.05f;
            float pickupRadius = 220.0f;
            float regenPerSecond = 0.0f;

            // 防御只有固定值：减伤率 = 防御 / (防御 + 100)（100 点 = 50% 减伤，趋近但永不到 100%）。
            // 穿甲分两档，都作用在对方的防御值上：百分比穿甲先按比例削，固定穿甲再扣点（不穿成负数）。
            float defense = 0.0f;           // 防御（点）
            float penFlat = 0.0f;           // 固定穿甲（点）
            float penPercent = 0.0f;        // 百分比穿甲（0~1）
        };

        // 开局裸值：ResetRun 重置成它；百分比型成长（移速 / 拾取半径）的每张卡步长也按它的比例给，
        // 加成是线性叠加而不是复利膨胀（属性卡没有层数上限，靠 MOVE_CAP / 暴击上限兜底）。
        // （定义在 SurvivorsGame.cpp）
        static const BaseStats BARE_STATS;

        // 成长写进基础属性后调用：同步生命上限并钳制当前血量（不做任何乘算）
        void RecalculateStats();

        // ---- 平衡数值（全部读基础属性） ----
        float DamageMul() const;
        float AttackSpeedMul() const;
        float CritChance() const;
        float MoveSpeed() const;
        float PickupRadius() const;
        float MaxHp() const;
        static int ExpNeeded(int level);

        // ---- 数据 ----
        bool m_active = true;
        State m_state = State::Playing;
        float m_pauseGuardTime = 0.0f;   // 刚进入暂停的宽限时间（秒）：这几帧内忽略 ESC/P，避免同一按键立刻关掉菜单（消息可能比物理状态晚到几帧）

        SurvivorsArt m_art;
        Ref<Shader> m_shader;
        FollowCamera m_camera;
        std::mt19937 m_rng{ std::random_device{}() };

        uint32_t m_viewWidth = 1600;
        uint32_t m_viewHeight = 900;
        float m_halfWidth = 800.0f;
        float m_halfHeight = 450.0f;

        // 属性：开局裸值 + 升级累加，战斗与 UI 只读这一份
        BaseStats m_baseStats;

        // 玩家
        glm::vec2 m_playerPosition{ 0.0f, 0.0f };
        glm::vec2 m_playerVelocity{ 0.0f, 0.0f };
        float m_playerHp = 20.0f;
        float m_playerMaxHp = 20.0f;
        float m_hurtCooldown = 0.0f;
        float m_walkPhase = 0.0f;
        float m_hurtFlash = 0.0f;
        bool m_playerAlive = true;

        // 成长
        int m_level = 1;
        int m_exp = 0;
        int m_expNext = 8;
        int m_pendingLevelUps = 0;
        std::array<int, static_cast<int>(WeaponId::Count)> m_weaponLevel{ 1, 0, 0, 0 };
        std::array<int, static_cast<int>(PassiveId::Count)> m_passive{ 0, 0, 0, 0, 0, 0, 0 };
        std::vector<UpgradeOption> m_options;
        float m_levelFlash = 0.0f;
        float m_levelUpLockout = 0.0f;   // 关闭升级面板后的缓冲时间（秒）

        // 金币：击杀掉落、走近自动吸取；升级面板里刷新卡片的唯一货币
        int m_coins = 0;
        int m_coinsEarned = 0;   // 本局累计获得（结算面板展示）
        int m_rerollCount = 0;   // **本次选取**已刷新次数（每次开面板清零），刷新价 = 4 + 3 × 它

        // 场上的东西
        std::vector<Enemy> m_enemies;
        std::vector<Projectile> m_projectiles;
        std::vector<Coin> m_coinDrops;
        std::vector<Particle> m_particles;
        std::vector<Fx> m_effects;
        std::vector<DamageNumber> m_damageNumbers;
        EnemyGrid m_grid;
        uint32_t m_nextEnemyId = 1;

        // 统计
        float m_time = 0.0f;
        int m_kills = 0;
        float m_damageDealt = 0.0f;
        float m_shake = 0.0f;

        // 关卡：10 关，第 1 关 30 秒、之后每关 +5 秒，只有第 5 / 10 关有 Boss
        int m_stage = 1;
        float m_stageTime = 0.0f;        // 本关已存活时间
        float m_stageDuration = 30.0f;   // 本关总时长
        float m_stageBanner = 0.0f;      // 进关横幅剩余时间
        bool m_stageBannerBoss = false;  // 本关有 Boss（横幅换成血月配色）

        // 冷却
        float m_bladeCooldown = 0.0f;
        float m_auraCooldown = 0.0f;
        float m_scytheCooldown = 0.0f;
        float m_stormCooldown = 0.0f;

        // 刷怪
        float m_spawnTimer = 0.0f;

        // 相机
        glm::vec2 m_cameraFocus{ 0.0f, 0.0f };

        // 背景雾团
        struct Wisp {
            glm::vec2 position{ 0.0f, 0.0f };
            glm::vec2 velocity{ 0.0f, 0.0f };
            float radius = 300.0f;
            glm::vec4 color{ 1.0f, 1.0f, 1.0f, 0.06f };
        };
        std::vector<Wisp> m_wisps;

        bool m_showDebug = false;
        bool m_showHelp = false;      // 帮助面板（标题页 / 暂停菜单里打开，ESC 关闭）
        bool m_showOptions = false;   // 设置面板（音量：主音量 / 背景音乐 / 音效）

        // 音频：BGM 常驻循环，Boss 关切换到另一首；音效按槽位节流
        std::array<Ref<AudioClip>, static_cast<int>(SfxId::Count)> m_sfxClips;
        std::array<float, static_cast<int>(SfxId::Count)> m_sfxThrottle{};
        Ref<AudioClip> m_bgmClip;
        Ref<AudioClip> m_bgmBossClip;
        Ref<SoundInstance> m_bgm;
        bool m_bgmIsBoss = false;    // 当前在放的是否 Boss 曲
        // 音量分三级：主音量走 AudioEngine，BGM / 音效是各自的分组系数（引擎没有总线，这里在应用层乘）
        float m_masterVolume = 0.85f;
        float m_bgmVolume = 0.80f;
        float m_sfxVolume = 0.90f;
        bool m_audioMuted = false;
    };
}
