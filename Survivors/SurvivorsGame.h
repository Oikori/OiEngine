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
// 玩法：WASD 移动，武器自动索敌开火；击杀敌人掉落经验宝石，升级时从三张卡里选一项强化。
// 撑到黎明（180 秒）即胜利。所有美术资源都是程序化几何，无图片依赖。
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
        enum class PassiveId { Might = 0, Haste, Swift, Vitality, Magnet, Luck, Regen, Count };
        enum class ShotKind { Blade, Scythe };
        enum class FxKind { Ring, Flash, Bolt };
        enum class UpgradeKind { Weapon, Passive, Heal };

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

        struct Gem {
            glm::vec2 position{ 0.0f, 0.0f };
            glm::vec2 velocity{ 0.0f, 0.0f };
            float value = 1.0f;
            float age = 0.0f;
            float phase = 0.0f;
            int tier = 0;
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
        };

        struct UpgradeOption {
            UpgradeKind kind = UpgradeKind::Passive;
            int index = 0;
            std::string tag;
            std::string title;
            std::string description;
            glm::vec4 color{ 0.5f, 0.5f, 0.5f, 1.0f };
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
        void InitWisps();

        // ---- 更新 ----
        void UpdateGame(float deltaTime);
        void UpdatePlayer(float deltaTime);
        void UpdateEnemySpawning(float deltaTime);
        void UpdateEnemies(float deltaTime);
        void UpdateWeapons(float deltaTime);
        void UpdateProjectiles(float deltaTime);
        void UpdateGems(float deltaTime);
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
        void SpawnGem(const glm::vec2& position, int value, bool burst);
        void SpawnParticles(const glm::vec2& position, int count, const glm::vec4& color,
                            float speed, float size, float lifetime, float zBias = 0.0f);
        void SpawnRing(const glm::vec2& position, float radius, float lifetime,
                       const glm::vec4& color);
        void SpawnFlash(const glm::vec2& position, float radius, float lifetime,
                        const glm::vec4& color);
        void SpawnDamageNumber(const glm::vec2& position, float value, bool crit);
        void GainExp(int amount);

        // ---- 升级 ----
        void StartLevelUp();
        void BuildUpgradeOptions();
        void ApplyUpgrade(int optionIndex);
        void DescribePassive(int passive, std::string& title, std::string& desc) const;

        // ---- 渲染 ----
        void DrawMesh(const Ref<VertexArray>& mesh, const glm::mat4& transform,
                      const glm::vec4& tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), float falloff = 0.0f);
        void RenderWorld();
        void DrawBackground();
        void DrawGround();
        void DrawGems();
        void DrawEnemies();
        void DrawPlayer();
        void DrawProjectiles();
        void DrawParticles();
        void DrawEffectsUnder();
        void DrawEffectsOver();
        void DrawScreenOverlay();
        void RenderTitle();
        void RenderHud();
        void RenderLevelUp();
        void RenderPause();
        void RenderGameOver();
        void RenderVictory();
        ImVec2 WorldToScreen(const glm::vec2& world) const;

        // ---- 随机 ----
        float Rand(float minValue, float maxValue);
        int RandInt(int minValue, int maxValue);

        // ---- 属性模型 ----
        // 基础属性：开局裸数值，不随祝福/武器变化。
        // 以后做「按基础值的百分比」类词条只能读这里。
        struct BaseStats {
            float maxHp = 120.0f;
            float moveSpeed = 240.0f;
            float damageMul = 1.0f;
            float attackSpeedMul = 1.0f;
            float critChance = 0.05f;
            float pickupRadius = 220.0f;
            float regenPerSecond = 0.0f;
        };

        // 最终属性：一局内所有成长汇总后的只读快照，战斗只读这一份。
        // 规则：RecalculateStats() 内只能读 m_baseStats 与被动/武器等级；
        //      禁止在汇总过程中读 m_finalStats 的派生值再去算另一个最终值（会形成递归依赖）。
        //      汇总函数带了重入保护 m_recalculatingStats。
        struct FinalStats {
            float maxHp = 120.0f;
            float moveSpeed = 240.0f;
            float damageMul = 1.0f;
            float attackSpeedMul = 1.0f;
            float critChance = 0.05f;
            float pickupRadius = 220.0f;
            float regenPerSecond = 0.0f;

            FinalStats() = default;
            // 汇总起点：最终值从基础值拷贝，再叠加成长
            explicit FinalStats(const BaseStats& base)
                : maxHp(base.maxHp), moveSpeed(base.moveSpeed), damageMul(base.damageMul),
                  attackSpeedMul(base.attackSpeedMul), critChance(base.critChance),
                  pickupRadius(base.pickupRadius), regenPerSecond(base.regenPerSecond) {}
        };

        void RecalculateStats();

        // ---- 平衡数值（全部读最终属性快照） ----
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
        int m_pauseGuardFrames = 0;   // 刚进入暂停的几帧内忽略 ESC/P，避免同一按键立刻关掉菜单

        SurvivorsArt m_art;
        Ref<Shader> m_shader;
        FollowCamera m_camera;
        std::mt19937 m_rng{ std::random_device{}() };

        uint32_t m_viewWidth = 1600;
        uint32_t m_viewHeight = 900;
        float m_halfWidth = 800.0f;
        float m_halfHeight = 450.0f;

        // 属性：基础值 + 每局汇总出来的最终值
        BaseStats m_baseStats;
        FinalStats m_finalStats;
        bool m_recalculatingStats = false;   // RecalculateStats 重入保护

        // 玩家
        glm::vec2 m_playerPosition{ 0.0f, 0.0f };
        glm::vec2 m_playerVelocity{ 0.0f, 0.0f };
        float m_playerHp = 120.0f;
        float m_playerMaxHp = 120.0f;
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

        // 场上的东西
        std::vector<Enemy> m_enemies;
        std::vector<Projectile> m_projectiles;
        std::vector<Gem> m_gems;
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
        float m_bossWarning = 0.0f;

        // 冷却
        float m_bladeCooldown = 0.0f;
        float m_auraCooldown = 0.0f;
        float m_scytheCooldown = 0.0f;
        float m_stormCooldown = 0.0f;

        // 刷怪
        float m_spawnTimer = 0.0f;
        float m_nextBossTime = 60.0f;
        int m_bossesSpawned = 0;
        bool m_bossAlive = false;
        uint32_t m_bossId = 0;

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
    };
}
