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
// 玩法：WASD 移动，武器自动索敌开火；击杀敌人**直接获得经验**，掉落物是金币（商店与刷新升级卡用）。
// **开局先选一个角色和一把武器**；之后每次升级从三张**属性卡**里选一项（武器卡与立即回复已移出卡池），
// **每 5 级**从四把武器里选一把（新武器或已有武器 +1 级）；**每关通关后开商店**买道具（可整组刷新）。
// 道具有种类（普通 / 限制类 / 独特 / 角色）与稀有度（普通 / 高级 / 稀有 / 史诗）。
// 撑到黎明（第 10 关结束）即胜利。所有美术资源都是程序化几何，无图片依赖。
//
// 代码分工（所有 .cpp 都只定义 SurvivorsLayer 的成员函数，共享本头文件）：
//   SurvivorsGame.cpp     主循环 / 生命周期 / 音频 / 关卡推进 / 玩家 / 相机 / 输入
//   SurvivorsEnemies.cpp  敌人：空间哈希、刷怪、移动与分离、接触伤害、Boss
//   SurvivorsWeapons.cpp  武器：索敌、四把武器的开火与冷却、弹丸推进
//   SurvivorsCombat.cpp   伤害与击杀、金币掉落与拾取、粒子 / 光环 / 伤害数字
//   SurvivorsUpgrade.cpp  经验与升级卡（卡池、品质、整组刷新、属性结算）
//   SurvivorsItems.cpp    道具目录 / 持有上限 / 效果结算 / 商店 / 开局选角与每 5 级选武器
//   SurvivorsEvents.cpp   事件（击杀 / 拾取金币 / 受伤 / 过关）与挂在事件上的道具响应
//   SurvivorsRender.cpp   世界渲染（程序化网格的绘制顺序与变换）
//   SurvivorsUi.cpp       全部 ImGui 面板（标题 / 选角 / HUD / 升级 / 选武器 / 商店 / 暂停 / 结算 / 帮助 / 设置）
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
        // Setup = 开局选角色 + 武器；WeaponPick = 每 5 级从全部武器里选一把；Shop = 每关通关后的商店
        enum class State { Title, Setup, Playing, LevelUp, WeaponPick, Shop, Paused, GameOver, Victory };
        enum class EnemyType { Bat = 0, Ghoul, Wraith, Brute, Boss, Count };
        enum class WeaponId { MoonBlade = 0, Aura, Scythe, Storm, Count };
        enum class PassiveId { Might = 0, Haste, Swift, Vitality, Magnet, Luck, Regen, Bulwark, Sunder, Count };
        enum class ShotKind { Blade, Scythe };
        enum class FxKind { Ring, Flash, Bolt };
        // 事件：游戏里会「发生」、道具可以挂钩的时刻。
        // 发生处（KillEnemy / 金币拾取 / DamagePlayer / AdvanceStage）只调用一次 RaiseEvent()，
        // 道具的响应全部集中在 SurvivorsEvents.cpp 的 RaiseEvent 里：
        // 新增一种事件 = 加一项枚举 + 在发生处发一次；新增一件事件道具 = 在 RaiseEvent 里读它。
        enum class EventId { Kill = 0, CoinPickup, Hurt, StageClear, Count };
        // 品质 / 稀有度：同一套四档（普通 / 高级 / 稀有 / 史诗），但用法不同——
        //   升级卡（属性卡）每次抽卡**掷**品质，倍率放大加成；
        //   道具的稀有度是**目录里写死的固定属性**（见 ItemDef::rarity），只决定展示色、价格与出货权重。
        enum class Rarity { Common = 0, Advanced, Rare, Epic, Count };
        // 道具种类：
        //   普通   —— 无持有上限，可以一直叠
        //   限制类 —— 每件有持有上限（ItemCap 给 2~3）
        //   独特   —— 全局只能持有一件（手上已有任意独特道具就不再出），每件也只能拿一次
        //   角色   —— 开局选择的角色，固定史诗，不进商店
        enum class ItemCategory { Normal = 0, Limited, Unique, Character, Count };
        // 道具目录：顺序与 SurvivorsItems.cpp 里的 ITEM_DEFS 一一对应（读目录请走 ItemName / ItemColor）
        enum class ItemId {
            // 普通类：无持有上限
            BloodBag = 0, Whetstone, IronPlate, HunterMark, StarBoots, SoulLamp,
            CoinHammer,                                  // 事件 · 拾取金币
            // 限制类：有持有上限
            BloodSigil, GaleFeather, Grail, DragonScale,
            SoulDagger, WailingBell, ScavengerCharm,     // 事件 · 击杀 / 拾取金币
            MartyrStigma,                                // 事件 · 受伤
            // 独特类：全局一件
            UndyingHeart, ThornMail, GreedCoffin, MoonshotPupil,
            GreedChain,                                  // 事件 · 拾取金币
            DawnPact,                                    // 事件 · 过关
            // 角色类：史诗，开局选择
            CharWatchman, CharHunter, CharSister, CharWarlock,
            Count
        };
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

        // 升级卡：现在只有属性卡（武器与立即回复已移出卡池），所以只需要被动下标 + 掷出的品质
        struct UpgradeOption {
            int index = 0;                       // PassiveId
            Rarity rarity = Rarity::Common;
            std::string tag;
            std::string title;
            std::string description;
            glm::vec4 color{ 0.5f, 0.5f, 0.5f, 1.0f };
        };

        // 商店货位：一件道具 + 价格（售出后留空位展示，不再刷新）。
        // 稀有度是**道具本身的固定属性**（见 ItemDef::rarity），不随货位重掷
        struct ShopOffer {
            ItemId id = ItemId::BloodBag;
            int price = 0;
            bool sold = false;
        };

        // 道具目录条目（数据在 SurvivorsItems.cpp）：名字 / 种类 / 固定稀有度 / 持有上限（0 = 无上限）/ 配色
        struct ItemDef {
            const char* name = "";
            ItemCategory category = ItemCategory::Normal;
            Rarity rarity = Rarity::Common;
            int cap = 0;
            glm::vec4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
        };

        // 道具的单次加成（primary / secondary 的含义随道具不同，见 ItemGainFor）
        struct ItemGain {
            float primary = 0.0f;
            float secondary = 0.0f;
        };

        // 道具目录本体：唯一一份定义在 SurvivorsItems.cpp（数组下标 = ItemId）
        static const ItemDef ITEM_DEFS[static_cast<int>(ItemId::Count)];

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
        void ResetRun();        // 清一局的状态，然后进入 State::Setup（选角色 + 武器）
        void BeginRun();        // 应用开局选择（角色 / 武器）并开打
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

        // ---- 输入（ESC / R 都走每帧物理轮询，ImGui 抢焦点时事件通道收不到）----
        void PollPauseKeys();
        void PollHotkeys();   // R：面板里 = 整组刷新，游戏内 = 重开一局

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

        // ---- 升级：卡池只剩属性卡（武器在每 5 级单独选，立即回复改由商店的道具提供）----
        void StartLevelUp();
        void BuildUpgradeOptions(const std::vector<UpgradeOption>& avoid = {});
        std::vector<UpgradeOption> BuildUpgradePool() const;   // 当前可选的全部属性卡（未定品质、未洗牌）
        UpgradeOption RollOption(const UpgradeOption& base);   // 给一张基础卡掷品质并生成文案
        Rarity RollRarity(int depth);                          // 给升级卡掷品质：depth = 当前等级，越大越好
        int RerollCost() const;                                // 本次选取下一次刷新要花的金币
        void RerollOptions();                                  // 花金币整组重抽三张
        static bool SameCard(const UpgradeOption& a, const UpgradeOption& b);
        static PassiveGain PassiveGainFor(int passive, float multiplier);
        void ApplyUpgrade(int optionIndex);
        void DescribePassive(int passive, float multiplier, std::string& title, std::string& desc) const;

        // ---- 每 5 级的武器选择（从全部武器里选：新武器 or 已有武器 +1 级）----
        void StartWeaponPick();
        void ChooseWeapon(int weaponIndex);
        static std::string DescribeWeapon(int weaponIndex, int currentLevel);   // 卡片正文（含下一级数值）

        // ---- 开局选择（角色类道具按目录顺序编号）----
        static ItemId CharacterItem(int index);
        static int CharacterCount();
        static int CharacterIndexOf(ItemId id);   // 反向：角色类道具 → 0..3（非角色返回 0）
        void ApplyCharacter(ItemId character);

        // ---- 道具 ----
        // 道具的稀有度是**目录里写死的固定属性**（同一件道具永远是同一个稀有度），
        // 稀有度只决定档位：展示色、商店价格、出货权重；效果就是 ItemGainFor 里的那组数
        static const char* ItemName(ItemId id);
        static glm::vec4 ItemColor(ItemId id);
        static ItemCategory ItemCategoryOf(ItemId id);
        static Rarity ItemRarity(ItemId id);
        static int ItemCap(ItemId id);            // 0 = 无持有上限（普通类）
        static ItemGain ItemGainFor(ItemId id);
        static void DescribeItem(ItemId id, std::string& title, std::string& description);
        void ResetItems();
        int ItemCount(ItemId id) const;
        int UniqueHeld() const;                   // 手上的独特道具件数（>0 就不能再买独特）
        bool CanAcquireItem(ItemId id) const;     // 种类上限 + 独特全局一件都在这里判
        void AcquireItem(ItemId id);              // 结算效果并记入背包（重复购买就是叠加）
        void ApplyItemEffect(ItemId id);
        int CoinGain(int base) const;             // 金币收益倍率（贪婪之匣）

        // ---- 事件（SurvivorsEvents.cpp）：发生处只发一次，道具响应全在这里 ----
        // amount 的含义随事件不同：击杀 / 受伤 = 次数，拾取金币 = 面额，过关 = 关卡号
        void RaiseEvent(EventId id, const glm::vec2& position, int amount = 1);
        void ResetEvents();                                  // 清一局的事件统计与触发进度（ResetRun 调用）
        std::vector<std::string> EventSummary() const;       // 暂停面板用：每行一个事件的累计与进度
        // 事件爆发（阈值到的那些效果）：范围伤害 + 世界反馈
        void SpawnKillBurst(const glm::vec2& position);      // 哀嚎钟：音波
        void SpawnCoinBurst(const glm::vec2& position, int coinValue);   // 贪欲之链：金币碎片
        void SpawnHurtBurst(const glm::vec2& position);      // 受难者圣痕：暗焰

        // ---- 商店（每关通关后开）----
        void OpenShop();
        void RollShopOffers(const std::vector<ShopOffer>& avoid = {});
        int ShopPrice(Rarity rarity) const;
        int ShopRerollCost() const;
        void RerollShop();
        bool BuyShopOffer(int slot);

        // ---- 面板排队：升级 → 选武器 → 商店，全部处理完才回到 Playing ----
        void ContinueRun();

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
        void RenderSetup();       // 开局：选角色 + 武器
        void RenderHud();
        void RenderHelp();
        void RenderOptions();
        void RenderLevelUp();
        void RenderWeaponPick();  // 每 5 级：从全部武器里选
        void RenderShop();        // 每关通关后：买道具 / 整组刷新
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
            // 穿甲也只有固定值：先扣对方这么多点防御（不穿成负数），剩下的防御再换减伤率
            float defense = 0.0f;           // 防御（点）
            float penFlat = 0.0f;           // 固定穿甲（点）

            // 道具带来的额外维度（属性卡不给这些）
            float critDamageMul = 2.0f;     // 暴击伤害倍率（猎月瞳提升）
            float coinGainMul = 1.0f;       // 金币收益倍率（贪婪之匣）
            float thornRatio = 0.0f;        // 荆棘之甲：受击时按比例反弹给贴身的敌人
            int reviveCharges = 0;          // 不朽之心：阵亡时的复活次数

            // 事件类道具：只在对应事件发生时读取，不参与每帧结算（触发节奏见 SurvivorsInternal.h）
            float killHeal = 0.0f;          // 噬魂匕：每次击杀回复的生命
            float killBurstDamage = 0.0f;   // 哀嚎钟：每 KILL_EVENT_STEP 次击杀的音波伤害
            int coinEventBonus = 0;         // 铸币者之锤：每 COIN_EVENT_STEP 枚金币额外铸出的金币
            float coinHeal = 0.0f;          // 拾荒护符：每 COIN_EVENT_STEP 枚金币回复的生命
            float coinBurstRatio = 0.0f;    // 贪欲之链：拾币时按面额倍率对四周造成的伤害
            float hurtBurstDamage = 0.0f;   // 受难者圣痕：每 HURT_EVENT_STEP 次受伤的暗焰伤害
            float hurtBurstHeal = 0.0f;     // 受难者圣痕：触发时回复最大生命的比例
            float stageDamageGain = 0.0f;   // 黎明誓约：每次过关永久 +伤害
            float stageMaxHpGain = 0.0f;    // 黎明誓约：每次过关永久 +生命上限
            float stageHealRatio = 0.0f;    // 黎明誓约：每次过关回复最大生命的比例
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
        float CritMultiplier() const;
        float MoveSpeed() const;
        float PickupRadius() const;
        float MaxHp() const;
        static int ExpNeeded(int level);

        // ---- 数据 ----
        bool m_active = true;
        State m_state = State::Playing;
        // ESC / R 的边沿检测状态（每帧轮询用）：按住不放只算按了一次
        bool m_escapeDown = false;
        bool m_restartDown = false;

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
        int m_pendingWeaponPicks = 0;    // 每升到 5 的倍数就 +1（处理完才归零）
        bool m_shopPending = false;      // 本关已通关、商店还没开（排队等升级 / 选武器处理完）
        std::array<int, static_cast<int>(WeaponId::Count)> m_weaponLevel{ 0, 0, 0, 0 };   // 开局选的那把为 1
        std::array<int, static_cast<int>(PassiveId::Count)> m_passive{};
        std::vector<UpgradeOption> m_options;
        float m_levelFlash = 0.0f;
        float m_levelUpLockout = 0.0f;   // 关闭升级面板后的缓冲时间（秒）

        // 开局选择（State::Setup）：角色在角色类道具里的下标，武器是 WeaponId
        int m_setupCharacter = 0;
        int m_setupWeapon = 0;
        ItemId m_character = ItemId::CharWatchman;   // 本局选定的角色（决定玩家配色与初始属性）

        // 道具背包：件数（稀有度是道具的固定属性，不需要按件记）
        std::array<int, static_cast<int>(ItemId::Count)> m_itemCount{};

        // 事件：累计次数（展示用）+ 阈值类道具的触发进度（只在持有关联道具时累积）。
        // 事件会互相引发（音波击杀 → 又一次击杀事件），m_eventDepth 是连锁的递归护栏
        struct EventStats {
            int kills = 0;        // 击杀事件（次）
            int coinPickups = 0;  // 拾取金币事件（枚）
            int coinValue = 0;    // 拾取金币的累计面额
            int hurts = 0;        // 受伤事件（次）
            int stageClears = 0;  // 过关事件（次）
        };
        EventStats m_eventStats;
        int m_eventKillProgress = 0;
        int m_eventCoinProgress = 0;
        int m_eventHurtProgress = 0;
        int m_eventDepth = 0;

        // 商店
        std::vector<ShopOffer> m_shopOffers;
        int m_shopRerolls = 0;   // 本次商店已刷新次数（每次进商店清零）

        // 金币：击杀掉落、走近自动吸取；刷新升级卡 / 刷新商店 / 买道具的货币
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
