// 《永夜幸存者》成长与升级：经验结算、升级卡池 / 品质掷点、整组刷新、属性结算。
//
// 属性卡数值的唯一来源是 PassiveGainFor()，描述与结算共用它；品质倍率见 RARITY_MULTIPLIER。
#include "SurvivorsGame.h"
#include "SurvivorsInternal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <format>
#include <string>

using namespace OiEngine;
using namespace OiEngine::Survivors;

// ============================ 升级 ============================

// ============================ 经验 ============================

// ============================ 经验 ============================

void SurvivorsLayer::GainExp(int amount)
{
    m_exp += amount;
    while (m_exp >= m_expNext) {
        m_exp -= m_expNext;
        ++m_level;
        ++m_pendingLevelUps;
        // 升级事件：成长之书回血 / 星辉残页加穿甲都挂在这里
        RaiseEvent(EventId::LevelUp, m_playerPosition);
        // 每 5 级排一次武器选择（从全部武器里选：新武器或已有武器 +1 级）
        if (m_level % WEAPON_PICK_STEP == 0) {
            ++m_pendingWeaponPicks;
        }
        m_expNext = ExpNeeded(m_level);
    }
}

void SurvivorsLayer::StartLevelUp()
{
    m_state = State::LevelUp;
    m_levelFlash = 1.0f;
    m_shake = 0.0f;
    m_camera.SetShake(glm::vec2(0.0f, 0.0f));
    PlaySfx(SfxId::LevelUp);
    m_rerollCount = 0;   // 新的一次选取：刷新价从基础价开始
    BuildUpgradeOptions();
}

// 当前可选的全部卡：**只剩属性卡**（九种，无层数上限）。
// 武器与立即回复已经移出卡池——武器改由「每 5 级选武器」，回复改由商店的血袋等道具提供。
// 这里只决定「有哪张卡」，品质与数值由 RollOption 生成
std::vector<SurvivorsLayer::UpgradeOption> SurvivorsLayer::BuildUpgradePool() const
{
    std::vector<UpgradeOption> pool;
    for (int passive = 0; passive < static_cast<int>(PassiveId::Count); ++passive) {
        // 属性卡没有层数上限：只要池子里还剩位置，就永远可以继续叠
        UpgradeOption option;
        option.index = passive;
        pool.push_back(option);
    }
    return pool;
}

SurvivorsLayer::Rarity SurvivorsLayer::RollRarity(int depth)
{
    // 品质权重随深度成长：普通逐渐让位给稀有 / 史诗（关卡越深 / 等级越高，好卡越多）。
    // 升级卡用等级，商店用关卡深度（m_stage × 3）
    const float level = static_cast<float>(std::max(1, depth));
    const float weights[4] = {
        std::max(18.0f, 58.0f - 1.4f * level),   // 普通
        26.0f + 0.2f * level,                    // 高级
        12.0f + 0.8f * level,                    // 稀有
        4.0f + 0.55f * level,                    // 史诗
    };
    float total = 0.0f;
    for (const float weight : weights) {
        total += weight;
    }
    float cursor = Rand(0.0f, total);
    for (int i = 0; i < 4; ++i) {
        if (cursor < weights[i]) {
            return static_cast<Rarity>(i);
        }
        cursor -= weights[i];
    }
    return Rarity::Common;
}

SurvivorsLayer::UpgradeOption SurvivorsLayer::RollOption(const UpgradeOption& base)
{
    UpgradeOption option = base;
    option.rarity = RollRarity(m_level);

    const int rarityIndex = static_cast<int>(option.rarity);
    const float multiplier = RARITY_MULTIPLIER[rarityIndex];
    option.color = RARITY_COLORS[rarityIndex];
    // 标签只给品质名：加成倍率是内部数值，卡片上不再显示（层数同理，见 DescribePassive）
    option.tag = RARITY_NAMES[rarityIndex];
    DescribePassive(option.index, multiplier, option.title, option.description);
    return option;
}

bool SurvivorsLayer::SameCard(const UpgradeOption& a, const UpgradeOption& b)
{
    return a.index == b.index;
}

void SurvivorsLayer::BuildUpgradeOptions(const std::vector<UpgradeOption>& avoid)
{
    std::vector<UpgradeOption> pool = BuildUpgradePool();

    // 整组刷新时尽量避开刚抽到的那三张，保证「刷了就是不一样的卡」
    if (!avoid.empty()) {
        std::vector<UpgradeOption> filtered;
        for (const UpgradeOption& option : pool) {
            bool repeated = false;
            for (const UpgradeOption& old : avoid) {
                if (SameCard(option, old)) {
                    repeated = true;
                    break;
                }
            }
            if (!repeated) {
                filtered.push_back(option);
            }
        }
        if (static_cast<int>(filtered.size()) >= CARD_COUNT) {
            pool = std::move(filtered);
        }
    }

    std::shuffle(pool.begin(), pool.end(), m_rng);
    m_options.clear();
    const int count = std::min(CARD_COUNT, static_cast<int>(pool.size()));
    for (int i = 0; i < count; ++i) {
        m_options.push_back(RollOption(pool[i]));
    }
}

int SurvivorsLayer::RerollCost() const
{
    // 价格只看「本次选取已刷新次数」：每刷一次 +3，开新面板时计数清零
    return REROLL_BASE_COST + REROLL_STEP * m_rerollCount;
}

void SurvivorsLayer::RerollOptions()
{
    if (m_state != State::LevelUp) {
        return;
    }
    const int cost = RerollCost();
    if (m_coins < cost) {
        PlaySfx(SfxId::Hurt, 0.30f, 1.70f);   // 金币不够：短促的「不行」提示
        return;
    }

    m_coins -= cost;
    ++m_rerollCount;
    // 消费事件：刷新升级卡也算花钱
    RaiseEvent(EventId::SpendCoins, m_playerPosition, cost);

    // 整组刷新：三张一起换（新的一手内部不重复，也尽量不重复刚才那三张）
    const std::vector<UpgradeOption> previous = m_options;
    BuildUpgradeOptions(previous);

    PlaySfx(SfxId::LevelUp, 0.45f, 1.28f);   // 刷新音：压低的升级音当「抽卡」的叮声
    SpawnRing(m_playerPosition, 70.0f, 0.30f, C_GOLD);
}

SurvivorsLayer::PassiveGain SurvivorsLayer::PassiveGainFor(int passive, float multiplier)
{
    // 单张属性卡（祝福）的加成 = 基准步长 × 品质倍率（普通 = 基准）。
    // 描述与结算共用这一份算式，改数值只改这里。
    // 基准值是「上一版再砍到 1/3」的结果（累计比最初下调了约 75%），
    // 配合品质倍率后跨度仍是 普通 ×1 ~ 史诗 ×3.5
    PassiveGain gain;
    switch (static_cast<PassiveId>(passive)) {
        case PassiveId::Might:    gain.primary = 0.03f * multiplier; break;                        // 伤害 +3%
        case PassiveId::Haste:    gain.primary = 0.025f * multiplier; break;                       // 攻速 +2.5%
        case PassiveId::Swift:    gain.primary = BARE_STATS.moveSpeed * 0.0167f * multiplier; break; // 移速 +1.67% 裸值（≈ +4）
        case PassiveId::Vitality: gain.primary = 4.0f * multiplier; break;                         // 生命 +4
        case PassiveId::Crit:     gain.primary = 0.015f * multiplier; break;                       // 暴击率 +1.5%
        case PassiveId::CritDamage: gain.primary = 0.15f * multiplier; break;                      // 暴击伤害 +15%
        case PassiveId::Regen:    gain.primary = 0.1f * multiplier; break;                         // 每秒回血 +0.1
        case PassiveId::Bulwark:  gain.primary = 2.0f * multiplier; break;                         // 防御 +2
        case PassiveId::Sunder:
        default:
            // 破甲**只给固定穿甲**，不再附带百分比穿甲
            gain.primary = 1.0f * multiplier;
            break;
    }
    return gain;
}

void SurvivorsLayer::DescribePassive(int passive, float multiplier, std::string& title, std::string& description) const
{
    // 卡片不显示「当前层数」：标题就是被动名，层数只留在 m_passive 里（结算与调试用）
    title = PASSIVE_NAMES[passive];

    const PassiveGain gain = PassiveGainFor(passive, multiplier);
    switch (static_cast<PassiveId>(passive)) {
        // 百分比类的步长已经很小（普通品质只有个位数百分比），显示保留一位小数，
        // 否则 2.5% 会被 %.0f 四舍五入成 2%
        case PassiveId::Might:
            description = std::format("全部伤害 +{:.1f}%\n累计 +{:.1f}%",
                gain.primary * 100.0f, (m_baseStats.damageMul - 1.0f + gain.primary) * 100.0f);
            break;
        case PassiveId::Haste:
            description = std::format("攻击速度 +{:.1f}%\n累计 +{:.1f}%",
                gain.primary * 100.0f, (m_baseStats.attackSpeedMul - 1.0f + gain.primary) * 100.0f);
            break;
        case PassiveId::Swift: {
            const float after = m_baseStats.moveSpeed + gain.primary;
            description = std::format("移动速度 +{:.0f}（没有上限）\n当前 {:.0f}（+{:.0f}%）",
                gain.primary, after,
                (after - BARE_STATS.moveSpeed) / BARE_STATS.moveSpeed * 100.0f);
            break;
        }
        case PassiveId::Vitality:
            description = std::format("最大生命 +{:.0f}，并立即回复 {:.0f}\n上限变为 {:.0f}",
                gain.primary, gain.primary, MaxHp() + gain.primary);
            break;
        case PassiveId::Crit:
            description = std::format("暴击率 +{:.1f}%（暴击伤害 ×{:.1f}）\n累计 {:.1f}% · 上限 100%",
                gain.primary * 100.0f, CritMultiplier(),
                std::min(CRIT_CHANCE_CAP, CritChance() + gain.primary) * 100.0f);
            break;
        case PassiveId::CritDamage: {
            const float after = m_baseStats.critDamageMul + gain.primary;
            description = std::format("暴击伤害 +{:.0f}%\n当前 ×{:.2f}（暴击率 {:.1f}%）",
                gain.primary * 100.0f, after, CritChance() * 100.0f);
            break;
        }
        case PassiveId::Regen:
            description = std::format("每秒回复 +{:.1f} 生命\n累计 {:.1f}/秒",
                gain.primary, m_baseStats.regenPerSecond + gain.primary);
            break;
        case PassiveId::Bulwark: {
            const float after = m_baseStats.defense + gain.primary;
            description = std::format("防御 +{:.0f}（减伤 = 防御/(防御+100)）\n累计 {:.0f} 点 · 减伤 {:.0f}%",
                gain.primary, after, DefenseReduction(after) * 100.0f);
            break;
        }
        case PassiveId::Sunder:
        default:
            // 破甲只加固定穿甲（百分比穿甲已从这张卡上移除）
            description = std::format("固定穿甲 +{:.0f}（无视对方这么多点防御）\n累计 {:.0f} 点",
                gain.primary, m_baseStats.penFlat + gain.primary);
            break;
    }
}

void SurvivorsLayer::ApplyUpgrade(int optionIndex)
{
    if (optionIndex < 0 || optionIndex >= static_cast<int>(m_options.size())) {
        return;
    }
    const UpgradeOption option = m_options[optionIndex];
    const float multiplier = RARITY_MULTIPLIER[static_cast<int>(option.rarity)];

    ++m_passive[option.index];   // 属性卡没有上限；层数只记在 m_passive 里，不再显示在卡片上
    // 成长直接累加进基础属性：百分比型步长按开局裸值取比例，线性叠加（不随当前值复利）
    const PassiveGain gain = PassiveGainFor(option.index, multiplier);
    switch (static_cast<PassiveId>(option.index)) {
        case PassiveId::Might:
            m_baseStats.damageMul += gain.primary;
            break;
        case PassiveId::Haste:
            m_baseStats.attackSpeedMul += gain.primary;
            break;
        case PassiveId::Swift:
            m_baseStats.moveSpeed += gain.primary;   // 移速不设上限
            break;
        case PassiveId::Vitality:
            m_baseStats.maxHp += gain.primary;
            break;
        case PassiveId::Crit:
            m_baseStats.critChance = std::min(CRIT_CHANCE_CAP, m_baseStats.critChance + gain.primary);
            break;
        case PassiveId::CritDamage:
            m_baseStats.critDamageMul += gain.primary;   // 暴击伤害倍率（基础 2.0）
            break;
        case PassiveId::Bulwark:
            m_baseStats.defense += gain.primary;
            break;
        case PassiveId::Sunder:
            m_baseStats.penFlat += gain.primary;   // 只加固定穿甲
            break;
        case PassiveId::Regen:
        default:
            m_baseStats.regenPerSecond += gain.primary;
            break;
    }

    // 成长已经写进基础属性，这里只需要同步生命上限（并钳制当前血量）
    RecalculateStats();

    // 体魄额外补上「多出来的那份血」
    if (option.index == static_cast<int>(PassiveId::Vitality)) {
        m_playerHp = std::min(m_playerMaxHp, m_playerHp + gain.primary);
    }

    SpawnRing(m_playerPosition, 110.0f, 0.5f, option.color);
    SpawnParticles(m_playerPosition, 12, option.color, 200.0f, 7.0f, 0.5f);

    --m_pendingLevelUps;
    // 选卡后给 1 秒无敌：面板关掉的瞬间周围往往已经围满了怪，
    // 不缓冲一下就会出现「刚选完卡就被贴脸打掉半管血」。连续升级时冷却不递减，
    // 所以实际无敌时间从真正回到战斗那一下开始算
    m_hurtCooldown = std::max(m_hurtCooldown, LEVELUP_INVULN_TIME);
    // 升级 → 选武器 → 商店 的排队统一交给 ContinueRun（它顺便把刷新计数清零）
    ContinueRun();
}

