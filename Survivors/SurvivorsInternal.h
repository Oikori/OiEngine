#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// 《永夜幸存者》**内部**共享的调参表 / 调色板 / 小工具。
//
// SurvivorsGame.cpp 拆成多个 .cpp 之后（主循环 / 敌人 / 武器 / 战斗掉落 / 升级 / 渲染 / 面板），
// 这些原本放在匿名命名空间里的东西需要被多个翻译单元共用，于是集中到这个私有头文件：
// 全部写成 inline / inline constexpr，保证整个程序只有一份定义，也不污染其它工程。
// 只允许 Survivors 目标内部 include，不要暴露给引擎或其它可执行工程。
namespace OiEngine::Survivors {

    constexpr float PI = 3.14159265358979323846f;
    constexpr float TAU = 2.0f * PI;

    // ---- 与 SandBox/RenderHelpers.h 相同的矩阵助手，Survivors 独立于 SandBox ----
    inline glm::mat4 Translate(const glm::vec3& translation)
    {
        return glm::translate(glm::mat4(1.0f), translation);
    }

    inline glm::mat4 Scale(const glm::vec3& scale)
    {
        return glm::scale(glm::mat4(1.0f), scale);
    }

    inline glm::mat4 RotateZ(float radians)
    {
        return glm::rotate(glm::mat4(1.0f), radians, glm::vec3(0.0f, 0.0f, 1.0f));
    }

    inline glm::mat4 Orthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane)
    {
        return glm::ortho(left, right, bottom, top, nearPlane, farPlane);
    }

    // ---- 尺度与上限 ----
    constexpr float VIEW_HALF_HEIGHT = 450.0f;
    constexpr int MAX_ENEMIES = 420;
    constexpr int MAX_PARTICLES = 700;
    constexpr int MAX_COINS = 420;
    constexpr int MAX_DAMAGE_NUMBERS = 48;
    constexpr float PLAYER_RADIUS = 20.0f;

    // ---- 场地围栏 ----
    // 玩家只能在 ±ARENA_HALF 的方形场地内移动（围栏画在四条边上）。
    // 敌人不拦：否则怪物会被关在栏杆外面，场地里反而空掉
    constexpr float ARENA_HALF = 1100.0f;
    constexpr float FENCE_SEGMENT = 260.0f;   // 围栏每段的长度（决定立柱密度）

    // 围栏外的阴影区：视觉上被压暗，且**里面的敌人无敌**（伤害、索敌都跳过它）。
    // 判定用中心点：敌人贴着围栏时以身子中心是否跨过围栏为准
    inline bool InShadow(const glm::vec2& position)
    {
        return std::abs(position.x) > ARENA_HALF || std::abs(position.y) > ARENA_HALF;
    }

    // 金币：地上的钱会过期消失（否则后期刷怪量会让掉落表无限增长）
    constexpr float COIN_LIFETIME = 22.0f;

    // 选完升级卡回到战斗的无敌时间：面板一关，贴脸的敌人不至于立刻打掉一截血
    // （复用 m_hurtCooldown：它 > 0 时接触伤害整段跳过，见 UpdateContactDamage）
    constexpr float LEVELUP_INVULN_TIME = 1.0f;

    // 刷新升级卡的价格：每次刷新都更贵（计数按「本次选取」清零重算）：
    // 4 + 3 × 本次已刷新次数。金币收入随关卡放大，系数不随等级变化
    constexpr int REROLL_BASE_COST = 4;
    constexpr int REROLL_STEP = 3;

    // ---- 道具与商店 ----
    // 每关通关后开一次商店：一次摆 4 件道具，可以整组刷新
    // （价 = (6 + 4 × 本次已刷新次数) × 刷新关卡系数，见 ShopRerollCost）
    constexpr int SHOP_SLOT_COUNT = 4;
    constexpr int SHOP_REROLL_BASE = 6;
    constexpr int SHOP_REROLL_STEP = 4;
    // 刷新价的关卡系数 = 1 + 0.4 × (已过关数 − 1)：第一次开商店（已过关数 = 1）正好是 1.0，
    // 之后每过一关 +0.4（n=5 → 2.6、n=9 → 4.2），与道具价格一起水涨船高
    constexpr float SHOP_REROLL_STAGE_STEP = 0.40f;
    // 按**道具固定稀有度**的基础价，再乘关卡系数（分段二次）：
    //   前 ITEM_PRICE_CURVE_KNEE（5）关：系数 = 1 + 0.20 × n，和原来的线性完全一致；
    //   第 6 关起：在上面叠 + ITEM_PRICE_STAGE_ACCEL × (n - 5)²，越往后涨得越快
    //   （n=6/7/8/9 → 系数 2.6 / 4.0 / 6.2 / 9.2，每多过一关的增量是 0.6 / 1.4 / 2.2 / 3.0）。
    // 金币掉落是固定值、不随关卡涨，所以越往后买东西越吃力
    inline constexpr int ITEM_PRICE[4] = { 12, 22, 40, 66 };
    constexpr float ITEM_PRICE_STAGE_STEP = 0.20f;    // 前段每关 +20% 基础价
    constexpr float ITEM_PRICE_STAGE_ACCEL = 0.40f;   // 第 6 关起的二次项系数（线性步长的两倍）
    constexpr int ITEM_PRICE_CURVE_KNEE = 5;          // 前 5 关保持纯线性

    // n = 已通过的关数（= stage - 1，进商店那一关的关卡号减 1）
    inline float ShopPriceScale(int stage)
    {
        const float n = static_cast<float>(stage - 1);
        const float linear = 1.0f + ITEM_PRICE_STAGE_STEP * n;
        if (n <= static_cast<float>(ITEM_PRICE_CURVE_KNEE)) {
            return linear;
        }
        const float past = n - static_cast<float>(ITEM_PRICE_CURVE_KNEE);
        return linear + ITEM_PRICE_STAGE_ACCEL * past * past;
    }
    // 商店出货权重：按道具的固定稀有度，越稀有越难刷出来
    inline constexpr float ITEM_WEIGHT[4] = { 6.0f, 3.0f, 1.5f, 0.6f };
    // 原「独特类」道具（现归限制类，每件上限 1）的机制数值
    constexpr float THORN_RADIUS = 240.0f;       // 荆棘之甲：反弹给这个半径内的敌人
    constexpr float REVIVE_HP_FRACTION = 0.60f;  // 不朽之心：复活时回多少血

    // ---- 事件与事件道具（SurvivorsEvents.cpp）----
    // 四类事件：击杀 / 拾取金币 / 受伤 / 过关。阈值类道具的触发间隔都在这里
    constexpr int KILL_EVENT_STEP = 12;    // 哀嚎钟：每 12 次击杀响一次
    constexpr int COIN_EVENT_STEP = 10;    // 铸币者之锤 / 拾荒护符：每累计 10 枚金币触发一次
    constexpr int HURT_EVENT_STEP = 5;     // 受难者圣痕：每 5 次受伤触发一次
    // 新四类事件：暴击 / 计时 / 消费 / 升级
    constexpr int CRIT_EVENT_STEP = 8;            // 破晓之刃：每 8 次暴击爆发一次
    constexpr float CRIT_BURST_RADIUS = 220.0f;   // 破晓之刃：爆发半径
    constexpr float TICK_EVENT_SECONDS = 8.0f;    // 沙漏 / 节拍器：每 8 秒触发一次
    constexpr int SPEND_EVENT_STEP = 50;          // 贪婪契约 / 商人之瞳：每累计花费 50 金币触发一次
    // 事件爆发的范围
    constexpr float BELL_RADIUS = 260.0f;        // 哀嚎钟：音波半径
    constexpr float STIGMA_RADIUS = 240.0f;      // 受难者圣痕：暗焰半径
    constexpr float GREED_CHAIN_RADIUS = 180.0f; // 贪欲之链：拾币时金币碎片的半径
    // 事件连锁上限：音波会再击杀敌人（又一次击杀事件），超过这个深度就只记账、
    // 不再引爆新的范围伤害，避免一屏敌人被递归连锁清空
    constexpr int MAX_EVENT_CHAIN = 3;
    constexpr float DAWN_PACT_MAXHP = 2.0f;      // 黎明誓约：每次过关 +生命上限
    // 事件道具的反馈色
    inline const glm::vec4 C_BELL(0.74f, 0.64f, 1.00f, 1.0f);    // 哀嚎钟
    inline const glm::vec4 C_STIGMA(0.72f, 0.24f, 0.34f, 1.0f);  // 受难者圣痕
    inline const glm::vec4 C_CRIT(0.86f, 0.94f, 1.00f, 1.0f);    // 破晓之刃：暴击爆发
    inline const glm::vec4 C_TICK(0.60f, 0.92f, 0.86f, 1.0f);    // 节拍器：计时落点

    // ---- 武器 ----
    // 四把武器各最高 5 级；开局选一把，之后**每 5 级**可以从全部武器里选一次
    // （新武器 = 从 0 级解锁到 1 级，已有武器 = +1 级）。全部满级后这次机会折成金币
    constexpr int WEAPON_COUNT = 4;
    constexpr int WEAPON_MAX_LEVEL = 5;
    constexpr int WEAPON_PICK_STEP = 5;
    constexpr int WEAPON_PICK_FALLBACK_COINS = 30;

    // 属性卡没有层数上限：移速**不设上限**（堆多少跑多快），只留暴击率这一条硬上限防止玩法被打崩
    constexpr float CRIT_CHANCE_CAP = 1.0f;      // 暴击率最多 100%
    constexpr int CARD_COUNT = 3;                // 升级面板一次给三张卡

    // 关卡节奏：10 关，第 1 关存活 30 秒，之后每关 +5 秒（共 525 秒）；
    // 只有第 5 / 10 关有 Boss，怪物数值随关卡递增（见 SpawnEnemy）
    constexpr int STAGE_COUNT = 10;
    constexpr float STAGE_BASE_DURATION = 30.0f;
    constexpr float STAGE_DURATION_STEP = 5.0f;
    constexpr int BOSS_STAGES[2] = { 5, 10 };

    inline float StageDuration(int stage)
    {
        return STAGE_BASE_DURATION + STAGE_DURATION_STEP * static_cast<float>(stage - 1);
    }

    inline bool HasBossStage(int stage)
    {
        return stage == BOSS_STAGES[0] || stage == BOSS_STAGES[1];
    }

    // ---- 调色板 ----
    inline const glm::vec4 WHITE(1.0f, 1.0f, 1.0f, 1.0f);
    inline const glm::vec4 C_BLADE(0.62f, 0.92f, 1.00f, 1.0f);
    inline const glm::vec4 C_AURA(1.00f, 0.82f, 0.42f, 1.0f);
    inline const glm::vec4 C_SCYTHE(0.78f, 0.42f, 1.00f, 1.0f);
    inline const glm::vec4 C_STORM(0.70f, 0.86f, 1.00f, 1.0f);
    inline const glm::vec4 C_BLOOD(0.88f, 0.20f, 0.26f, 1.0f);
    inline const glm::vec4 C_GOLD(1.00f, 0.84f, 0.45f, 1.0f);
    // 金币面额分档的拾取粒子色（越大越暖）
    inline const glm::vec4 COIN_COLORS[3] = {
        glm::vec4(1.00f, 0.86f, 0.44f, 1.0f),
        glm::vec4(1.00f, 0.76f, 0.28f, 1.0f),
        glm::vec4(1.00f, 0.62f, 0.18f, 1.0f),
    };

    // ---- 卡片品质：普通 / 高级 / 稀有 / 史诗 ----
    inline constexpr const char* RARITY_NAMES[4] = { "普通", "高级", "稀有", "史诗" };
    inline const glm::vec4 RARITY_COLORS[4] = {
        glm::vec4(0.64f, 0.68f, 0.78f, 1.0f),   // 普通：灰
        glm::vec4(0.40f, 0.86f, 0.62f, 1.0f),   // 高级：绿
        glm::vec4(0.72f, 0.50f, 1.00f, 1.0f),   // 稀有：紫（商店卡片边框 / 稀有度标签 / 购买按钮同色）
        glm::vec4(1.00f, 0.74f, 0.32f, 1.0f),   // 史诗：金
    };
    // 品质倍率：同一张属性卡按品质放大加成（普通 = 基准）
    constexpr float RARITY_MULTIPLIER[4] = { 1.0f, 1.6f, 2.4f, 3.5f };

    // ---- 道具种类：普通 / 限制类 / 角色（原先的「独特类」已并入限制类，每件上限 1）----
    inline constexpr const char* ITEM_CATEGORY_NAMES[3] = { "普通", "限制", "角色" };
    inline const glm::vec4 ITEM_CATEGORY_COLORS[3] = {
        glm::vec4(0.70f, 0.74f, 0.84f, 1.0f),   // 普通
        glm::vec4(0.62f, 0.84f, 0.72f, 1.0f),   // 限制类
        glm::vec4(1.00f, 0.78f, 0.42f, 1.0f),   // 角色
    };
    // 四个角色的配色（选角卡 / HUD / 法阵都取它）。道具目录本身不再存颜色：
    // 普通 / 限制道具按稀有度取色，只有角色需要单独一套
    inline const glm::vec4 CHARACTER_COLORS[4] = {
        glm::vec4(0.66f, 0.78f, 1.00f, 1.0f),   // 守夜人：冰蓝
        glm::vec4(0.62f, 0.90f, 0.66f, 1.0f),   // 猎手：苔绿
        glm::vec4(0.96f, 0.88f, 0.70f, 1.0f),   // 修女：暖白
        glm::vec4(0.86f, 0.52f, 1.00f, 1.0f),   // 术士：紫
    };
    inline const glm::vec4 C_THORN(0.92f, 0.46f, 0.72f, 1.0f);   // 荆棘之甲反弹的环

    // ---- 武器调参（下标 = 等级 1~5，0 号位不用）----
    struct WeaponTuning {
        float damage[6];
        float cooldown[6];
        int count[6];
        int pierce[6];
        float radius[6];
    };

    // 月刃：默认武器，索敌直线弹道
    inline constexpr WeaponTuning BLADE = {
        { 0.0f, 11.0f, 16.0f, 18.0f, 24.0f, 32.0f },
        { 0.0f, 0.58f, 0.54f, 0.50f, 0.46f, 0.40f },
        { 0, 1, 1, 2, 2, 3 },
        { 0, 2, 2, 2, 3, 4 },
        { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    };

    // 圣光环：以玩家为圆心的脉冲伤害
    inline constexpr WeaponTuning AURA = {
        { 0.0f, 5.0f, 8.0f, 11.0f, 16.0f, 22.0f },
        { 0.0f, 0.52f, 0.50f, 0.46f, 0.42f, 0.36f },
        { 0, 1, 1, 1, 1, 1 },
        { 0, 0, 0, 0, 0, 0 },
        { 0.0f, 120.0f, 138.0f, 156.0f, 178.0f, 205.0f },
    };

    // 回旋镰：飞出后折返，可重复切割
    inline constexpr WeaponTuning SCYTHE = {
        { 0.0f, 22.0f, 30.0f, 36.0f, 48.0f, 62.0f },
        { 0.0f, 2.60f, 2.40f, 2.20f, 2.00f, 1.80f },
        { 0, 1, 1, 2, 2, 3 },
        { 0, 0, 0, 0, 0, 0 },
        { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    };

    // 雷暴：瞬发落雷，**落点是范围伤害**（半径随等级放大，见 STORM.radius）
    inline constexpr WeaponTuning STORM = {
        { 0.0f, 24.0f, 32.0f, 40.0f, 52.0f, 68.0f },
        { 0.0f, 3.00f, 2.80f, 2.60f, 2.40f, 2.20f },
        { 0, 2, 2, 3, 3, 4 },
        { 0, 0, 0, 0, 0, 0 },
        { 0.0f, 90.0f, 100.0f, 112.0f, 126.0f, 145.0f },
    };

    struct EnemyTuning {
        float hp;
        float speed;
        float radius;
        float damage;
        int xp;
        int coins;
        float defense;
    };

    // 怪物数值的关卡成长（SpawnEnemy 统一按下面几条换算，改曲线只动这几个系数）：
    //   小怪血量 / 防御：**线性**，每关 +30% / +8%（第 10 关 = 3.7 倍 / 1.72 倍）；
    //   伤害（小怪与 Boss 共用）：**二次**，1 + 0.05·(n−1)² → 第 10 关 5.05 倍
    //   （原来是线性 +12%/关，末关只有 2.08 倍；改成二次后前期温和、后期陡）
    constexpr float ENEMY_HP_STEP = 0.30f;        // 每关 +30% 血量（线性）
    constexpr float ENEMY_DEFENSE_STEP = 0.08f;   // 每关 +8% 防御（线性）
    constexpr float ENEMY_DAMAGE_QUAD = 0.05f;    // 伤害的二次项系数

    inline constexpr float EnemyHpScale(int stage)
    {
        return 1.0f + ENEMY_HP_STEP * static_cast<float>(stage - 1);
    }

    inline constexpr float EnemyDefenseScale(int stage)
    {
        return 1.0f + ENEMY_DEFENSE_STEP * static_cast<float>(stage - 1);
    }

    // 伤害曲线：n=1 → 1.00，n=5 → 1.80，n=7 → 2.80，n=10 → 5.05（"第 10 关约 5 倍初始值"）
    inline constexpr float EnemyDamageScale(int stage)
    {
        const float n = static_cast<float>(stage - 1);
        return 1.0f + ENEMY_DAMAGE_QUAD * n * n;
    }

    // ---- Boss 单独一条血量曲线（伤害 / 防御与小怪共用上面那两条）----
    //   血月使徒的血量是**二次**：1 − 0.35·(n−1) + 0.15·(n−1)²，
    //   过 (1, 1.00)、(5, 2.00)、(10, 10.00) 三点 —— 第 5 关正好 2 倍基础值、第 10 关 10 倍。
    //   二次拟合在 n=2~4 会掉到 1 以下（0.80 / 0.90 / 1.30），但 Boss 只在第 5 / 10 关出现，
    //   那几关不会有 Boss，所以下限钳到 1.0 兜底。
    constexpr float BOSS_HP_LINEAR = -0.35f;   // 血量曲线的一次项
    constexpr float BOSS_HP_QUAD = 0.15f;      // 血量曲线的二次项

    inline constexpr float BossHpScale(int stage)
    {
        const float n = static_cast<float>(stage - 1);
        return std::max(1.0f, 1.0f + BOSS_HP_LINEAR * n + BOSS_HP_QUAD * n * n);
    }

    // 曲线验收：调系数时如果偏离设计目标，这里直接编译不过
    static_assert(EnemyDamageScale(10) > 4.95f && EnemyDamageScale(10) < 5.15f);   // 末关伤害 ≈ 5 倍
    static_assert(EnemyHpScale(10) > 3.6f && EnemyHpScale(10) < 3.8f);             // 末关小怪血量 ≈ 3.7 倍
    static_assert(BossHpScale(5) > 1.95f && BossHpScale(5) < 2.05f);               // 第 5 关 Boss ≈ 2 倍
    static_assert(BossHpScale(10) > 9.90f && BossHpScale(10) < 10.10f);            // 第 10 关 Boss ≈ 10 倍

    // 敌人速度普遍接近（略低于）玩家，围上来之后就不容易被一条直线甩掉。
    // 怪物只有防御（没有穿甲）：防御是固定值，减伤率 = 防御 / (防御 + 100)
    // （10 → 9.1%，24 → 19.4%，50 → 33.3%，100 → 50.0%），玩家的固定穿甲按点数扣它。
    // 血月使徒的 HP / 防御按普通怪（同级基准）的 3~4 倍档次给，所以体型与血量都明显压一档；
    // 它的**血量成长走 BossHpScale**（第 5 关 2 倍、第 10 关 10 倍），伤害 / 防御与小怪同一套。
    // 使徒的移速与玩家裸移速相同（240）：追不上也甩不掉，压迫感靠冲刺技能补（见 BOSS_DASH_*）
    // xp 是击杀当场直接结算的经验，coins 是掉在地上的金币数量（刷新升级卡的货币）
    inline constexpr EnemyTuning ENEMY_TUNING[5] = {
        { 20.0f, 238.0f, 15.0f, 2.0f, 2, 1, 0.0f },         // 蝙蝠
        { 56.0f, 188.0f, 19.0f, 3.0f, 4, 2, 10.0f },        // 食尸鬼
        { 88.0f, 228.0f, 22.0f, 4.0f, 6, 3, 24.0f },        // 怨灵
        { 350.0f, 158.0f, 31.0f, 6.0f, 14, 7, 50.0f },      // 石魔
        { 1000.0f, 240.0f, 46.0f, 10.0f, 100, 45, 100.0f }, // 血月使徒
    };

    // ---- 血月使徒的冲刺技能 ----
    // 靠近到 BOSS_DASH_TRIGGER_RANGE 内且冷却好了 → 锁定当时朝向玩家的方向 → 蓄力 1 秒 → 沿该方向冲刺。
    // 蓄力期间站定不动（方向已锁死，玩家横移就能躲），冲刺距离固定、撞到围栏提前结束。
    // 贴到 BOSS_DASH_MIN_RANGE 以内反而**不**冲：否则它会在玩家身上原地站桩蓄力
    constexpr float BOSS_DASH_MIN_RANGE = 180.0f;       // 比这更近就不发起冲刺（继续贴身追）
    constexpr float BOSS_DASH_TRIGGER_RANGE = 480.0f;   // 进入这个距离才发起冲刺
    constexpr float BOSS_DASH_CHARGE_TIME = 1.0f;       // 蓄力时长（方向在蓄力开始时锁定）
    constexpr float BOSS_DASH_SPEED = 1600.0f;          // 冲刺速度
    constexpr float BOSS_DASH_DISTANCE = 620.0f;        // 冲刺距离（时长 = 距离 / 速度 ≈ 0.39 秒）
    constexpr float BOSS_DASH_COOLDOWN = 4.5f;          // 一次冲刺结束后的再触发间隔（秒）

    inline constexpr const char* WEAPON_NAMES[4] = { "月刃", "圣光环", "回旋镰", "雷暴" };
    inline const glm::vec4 WEAPON_COLORS[4] = { C_BLADE, C_AURA, C_SCYTHE, C_STORM };
    // 九种被动：属性卡没有层数上限（PASSIVE_MAX 已移除），成长全靠品质与堆叠
    inline constexpr const char* PASSIVE_NAMES[9] = {
        "力量", "急速", "疾行", "体魄", "暴击", "爆伤", "回春", "铁壁", "破甲",
    };
    inline constexpr const char* ROMAN[6] = { "", "I", "II", "III", "IV", "V" };

    // 防御换减伤率：防御 / (防御 + 100)。100 点 = 50%，只会趋近 100%，所以不需要减伤保底
    inline float DefenseReduction(float defense)
    {
        const float clamped = std::max(0.0f, defense);
        return clamped / (clamped + 100.0f);
    }

    // 伤害结算（攻防两端共用）：
    //   1) 固定穿甲先扣对方的防御点数（扣到 0 为止，溢出部分不额外生效）；
    //   2) 剩下的防御进 防御/(防御+100) 换成减伤率，再乘到伤害上。
    // 旧版还有一档「百分比穿甲」（按比例削防御），随「破甲」只给固定穿甲一起删掉了
    inline float ResolveDamage(float raw, float penFlat, float defense)
    {
        const float effectiveDefense = defense - penFlat;
        return raw * (1.0f - DefenseReduction(effectiveDefense));
    }

    // ---- 音频表：顺序必须与 SurvivorsLayer::SfxId 完全一致 ----
    inline constexpr const char* SFX_FILES[] = {
        "blade.wav", "storm.wav", "hit.wav", "kill.wav", "pickup.wav",
        "levelup.wav", "hurt.wav", "stage_clear.wav", "boss_spawn.wav",
        "victory.wav", "gameover.wav",
    };
    // 基准音量：高频事件压低，关键事件抬高
    inline constexpr float SFX_VOLUME[] = {
        0.30f, 0.45f, 0.18f, 0.32f, 0.26f, 0.55f, 0.55f, 0.58f, 0.72f, 0.66f, 0.66f,
    };
    // 同一音效的最小间隔（秒）：命中 / 击杀 / 拾取一秒能触发几十次，全靠它压住
    inline constexpr float SFX_THROTTLE[] = {
        0.06f, 0.12f, 0.045f, 0.05f, 0.04f, 0.20f, 0.25f, 0.50f, 0.50f, 1.00f, 1.00f,
    };

    inline constexpr float BGM_VOLUME = 0.34f;
    inline constexpr float BGM_BOSS_VOLUME = 0.38f;

    // ---- 小工具 ----
    inline float Length(const glm::vec2& v)
    {
        return std::sqrt(v.x * v.x + v.y * v.y);
    }

    inline glm::vec2 Normalize(const glm::vec2& v)
    {
        const float length = Length(v);
        return length > 0.0001f ? v / length : glm::vec2(0.0f, 1.0f);
    }

    inline bool CircleHit(const glm::vec2& a, float aRadius, const glm::vec2& b, float bRadius)
    {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        const float radiusSum = aRadius + bRadius;
        return dx * dx + dy * dy < radiusSum * radiusSum;
    }

    inline uint32_t HashCell(int x, int y)
    {
        uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return h ^ (h >> 16);
    }

    inline float HashUnit(uint32_t value, int shift)
    {
        return static_cast<float>((value >> shift) & 1023u) / 1023.0f;
    }

    inline std::string FormatTime(float seconds)
    {
        const int total = static_cast<int>(seconds);
        return std::format("{:02d}:{:02d}", total / 60, total % 60);
    }
}

