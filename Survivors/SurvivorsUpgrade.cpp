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

// 当前可选的全部卡：武器（未满级）+ 九种属性卡（无层数上限）+ 补给卡。
// 这里只决定「有哪张卡」，品质与数值由 RollOption 生成（武器卡是等级进阶，不带品质）
std::vector<SurvivorsLayer::UpgradeOption> SurvivorsLayer::BuildUpgradePool() const
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
            option.tag = std::format("武器 Lv.{}", level + 1);
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
        // 属性卡不再有层数上限：只要池子里还剩位置，就永远可以继续叠
        UpgradeOption option;
        option.kind = UpgradeKind::Passive;
        option.index = passive;
        pool.push_back(option);
    }

    UpgradeOption heal;
    heal.kind = UpgradeKind::Heal;
    pool.push_back(heal);

    return pool;
}

SurvivorsLayer::Rarity SurvivorsLayer::RollRarity()
{
    // 品质权重随等级成长：普通逐渐让位给稀有 / 史诗（关卡越深，好卡越多）
    const float level = static_cast<float>(m_level);
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
    option.rarity = RollRarity();
    const int rarityIndex = static_cast<int>(option.rarity);
    const float multiplier = RARITY_MULTIPLIER[rarityIndex];
    option.color = RARITY_COLORS[rarityIndex];

    switch (option.kind) {
        case UpgradeKind::Weapon:
            // 武器卡沿用武器配色与 Lv 标签（品质只属于属性卡 / 补给卡）
            option.rarity = Rarity::Common;
            option.color = WEAPON_COLORS[option.index];
            break;
        case UpgradeKind::Passive:
            option.tag = std::format("{} · 加成 ×{:.1f}", RARITY_NAMES[rarityIndex], multiplier);
            DescribePassive(option.index, multiplier, option.title, option.description);
            break;
        case UpgradeKind::Heal:
            // 补给卡保留一点绿色底子，再按品质染色（越高品质越偏金）
            option.tag = std::format("{} · 回血 ×{:.1f}", RARITY_NAMES[rarityIndex], multiplier);
            option.title = "血之祭";
            option.color = glm::mix(C_HEAL, RARITY_COLORS[rarityIndex], 0.55f);
            option.description = std::format("立即恢复 {}% 最大生命。",
                static_cast<int>(std::lround(HealFraction(option.rarity) * 100.0f)));
            break;
    }
    return option;
}

bool SurvivorsLayer::SameCard(const UpgradeOption& a, const UpgradeOption& b)
{
    return a.kind == b.kind && a.index == b.index;
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

    // 整组刷新：三张一起换（新的一手内部不重复，也尽量不重复刚才那三张）
    const std::vector<UpgradeOption> previous = m_options;
    BuildUpgradeOptions(previous);

    PlaySfx(SfxId::LevelUp, 0.45f, 1.28f);   // 刷新音：压低的升级音当「抽卡」的叮声
    SpawnRing(m_playerPosition, 70.0f, 0.30f, C_GOLD);
}

SurvivorsLayer::PassiveGain SurvivorsLayer::PassiveGainFor(int passive, float multiplier)
{
    // 单张属性卡的加成 = 基准步长 × 品质倍率（普通 = 基准）。
    // 描述与结算共用这一份算式，改数值只改这里
    PassiveGain gain;
    switch (static_cast<PassiveId>(passive)) {
        case PassiveId::Might:    gain.primary = 0.12f * multiplier; break;
        case PassiveId::Haste:    gain.primary = 0.10f * multiplier; break;
        case PassiveId::Swift:    gain.primary = BARE_STATS.moveSpeed * 0.07f * multiplier; break;
        case PassiveId::Vitality: gain.primary = 8.0f * multiplier; break;
        case PassiveId::Magnet:   gain.primary = BARE_STATS.pickupRadius * 0.35f * multiplier; break;
        case PassiveId::Luck:     gain.primary = 0.06f * multiplier; break;
        case PassiveId::Regen:    gain.primary = 0.4f * multiplier; break;
        case PassiveId::Bulwark:  gain.primary = 8.0f * multiplier; break;
        case PassiveId::Sunder:
        default:
            gain.primary = 0.08f * multiplier;    // 百分比穿甲
            gain.secondary = 4.0f * multiplier;   // 固定穿甲
            break;
    }
    return gain;
}

float SurvivorsLayer::HealFraction(Rarity rarity)
{
    const float fraction = HEAL_BASE_PERCENT * RARITY_MULTIPLIER[static_cast<int>(rarity)];
    return std::min(1.0f, fraction);
}

void SurvivorsLayer::DescribePassive(int passive, float multiplier, std::string& title, std::string& description) const
{
    const int stacks = m_passive[passive] + 1;   // 选下这张卡之后的总层数
    title = stacks > 1
        ? std::format("{} · {} 层", PASSIVE_NAMES[passive], stacks)
        : std::string(PASSIVE_NAMES[passive]);

    const PassiveGain gain = PassiveGainFor(passive, multiplier);
    switch (static_cast<PassiveId>(passive)) {
        case PassiveId::Might:
            description = std::format("全部伤害 +{:.0f}%\n累计 +{:.0f}%",
                gain.primary * 100.0f, (m_baseStats.damageMul - 1.0f + gain.primary) * 100.0f);
            break;
        case PassiveId::Haste:
            description = std::format("攻击速度 +{:.0f}%\n累计 +{:.0f}%",
                gain.primary * 100.0f, (m_baseStats.attackSpeedMul - 1.0f + gain.primary) * 100.0f);
            break;
        case PassiveId::Swift: {
            const float after = std::min(m_baseStats.moveSpeed + gain.primary,
                                         BARE_STATS.moveSpeed * MOVE_SPEED_CAP_MUL);
            description = std::format("移动速度 +{:.0f}（上限 ×{:.1f}）\n当前 {:.0f}（+{:.0f}%）",
                gain.primary, MOVE_SPEED_CAP_MUL, after,
                (after - BARE_STATS.moveSpeed) / BARE_STATS.moveSpeed * 100.0f);
            break;
        }
        case PassiveId::Vitality:
            description = std::format("最大生命 +{:.0f}，并立即回复 {:.0f}\n上限变为 {:.0f}",
                gain.primary, gain.primary, MaxHp() + gain.primary);
            break;
        case PassiveId::Magnet: {
            const float pct = gain.primary / BARE_STATS.pickupRadius * 100.0f;
            const float total = (PickupRadius() - BARE_STATS.pickupRadius + gain.primary) /
                                BARE_STATS.pickupRadius * 100.0f;
            description = std::format("金币拾取范围 +{:.0f}%\n累计 +{:.0f}%", pct, total);
            break;
        }
        case PassiveId::Luck:
            description = std::format("暴击率 +{:.0f}%（暴击双倍伤害）\n累计 {:.0f}% · 上限 100%",
                gain.primary * 100.0f, std::min(CRIT_CHANCE_CAP, CritChance() + gain.primary) * 100.0f);
            break;
        case PassiveId::Regen:
            description = std::format("每秒回复 +{:.2f} 生命\n累计 {:.2f}/秒",
                gain.primary, m_baseStats.regenPerSecond + gain.primary);
            break;
        case PassiveId::Bulwark: {
            const float after = m_baseStats.defense + gain.primary;
            description = std::format("防御 +{:.0f}（减伤 = 防御/(防御+100)）\n累计 {:.0f} 点 · 减伤 {:.0f}%",
                gain.primary, after, DefenseReduction(after) * 100.0f);
            break;
        }
        case PassiveId::Sunder:
        default: {
            const float percent = std::min(1.0f, m_baseStats.penPercent + gain.primary);
            description = std::format("百分比穿甲 +{:.0f}% · 固定穿甲 +{:.0f}\n累计 {:.0f}% / {:.0f} 点",
                gain.primary * 100.0f, gain.secondary, percent * 100.0f,
                m_baseStats.penFlat + gain.secondary);
            break;
        }
    }
}

void SurvivorsLayer::ApplyUpgrade(int optionIndex)
{
    if (optionIndex < 0 || optionIndex >= static_cast<int>(m_options.size())) {
        return;
    }
    const UpgradeOption option = m_options[optionIndex];
    const float multiplier = RARITY_MULTIPLIER[static_cast<int>(option.rarity)];

    switch (option.kind) {
        case UpgradeKind::Weapon:
            m_weaponLevel[option.index] = std::min(5, m_weaponLevel[option.index] + 1);
            break;
        case UpgradeKind::Passive: {
            ++m_passive[option.index];   // 属性卡没有上限，层数只用于展示与描述
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
                    m_baseStats.moveSpeed = std::min(m_baseStats.moveSpeed + gain.primary,
                                                     BARE_STATS.moveSpeed * MOVE_SPEED_CAP_MUL);
                    break;
                case PassiveId::Vitality:
                    m_baseStats.maxHp += gain.primary;
                    break;
                case PassiveId::Magnet:
                    m_baseStats.pickupRadius += gain.primary;
                    break;
                case PassiveId::Luck:
                    m_baseStats.critChance = std::min(CRIT_CHANCE_CAP, m_baseStats.critChance + gain.primary);
                    break;
                case PassiveId::Bulwark:
                    m_baseStats.defense += gain.primary;
                    break;
                case PassiveId::Sunder:
                    m_baseStats.penPercent = std::min(1.0f, m_baseStats.penPercent + gain.primary);
                    m_baseStats.penFlat += gain.secondary;
                    break;
                case PassiveId::Regen:
                default:
                    m_baseStats.regenPerSecond += gain.primary;
                    break;
            }
            break;
        }
        case UpgradeKind::Heal:
            break;
    }

    // 成长已经写进基础属性，这里只需要同步生命上限（并钳制当前血量）
    RecalculateStats();

    if (option.kind == UpgradeKind::Passive && option.index == static_cast<int>(PassiveId::Vitality)) {
        m_playerHp = std::min(m_playerMaxHp,
                              m_playerHp + PassiveGainFor(option.index, multiplier).primary);
    } else if (option.kind == UpgradeKind::Heal) {
        m_playerHp = std::min(m_playerMaxHp, m_playerHp + m_playerMaxHp * HealFraction(option.rarity));
    }

    SpawnRing(m_playerPosition, 110.0f, 0.5f, option.color);
    SpawnParticles(m_playerPosition, 12, option.color, 200.0f, 7.0f, 0.5f);

    --m_pendingLevelUps;
    m_rerollCount = 0;   // 下一次选取重新计价（面板关掉后暂停面板显示的就是首刷价）
    if (m_pendingLevelUps > 0) {
        BuildUpgradeOptions();
    } else {
        m_state = State::Playing;
        m_levelFlash = 1.0f;
        m_levelUpLockout = 1.5f;
    }
}

