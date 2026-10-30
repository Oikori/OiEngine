// 《永夜幸存者》道具系统：道具目录、持有上限、效果结算、商店（掷货 / 整组刷新 / 购买），
// 以及开局（选角色 + 武器）与每 5 级的武器选择。
//
// 道具有两条独立的属性：
//   种类决定**能不能重复拿**（普通无上限 / 限制类有上限 / 独特全局一件 / 角色开局选），
//   稀有度是**目录里写死的档位**（同一件道具永远同一个稀有度，不会出现「铁片-普通 / 铁片-高级」），
//   它只决定展示色、商店价格与出货权重——效果就是 ItemGainFor 里那组数，不再按稀有度放大。
#include "SurvivorsGame.h"
#include "SurvivorsInternal.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <string>
#include <vector>

using namespace OiEngine;
using namespace OiEngine::Survivors;

// ============================ 道具目录 ============================

// 顺序必须与 SurvivorsLayer::ItemId 完全一致（下标就是 ItemId）：
//   0 血袋 1 磨刀石 2 铁片 3 猎魂纹 4 追星靴 5 引魂灯 6 铸币者之锤
//   7 血月徽记 8 疾风羽 9 圣杯 10 龙鳞 11 噬魂匕 12 哀嚎钟 13 拾荒护符 14 受难者圣痕
//   15 不朽之心 16 荆棘之甲 17 贪婪之匣 18 猎月瞳 19 贪欲之链 20 黎明誓约
//   21 守夜人 22 猎手 23 修女 24 术士
// 改枚举顺序时必须同步这张表，否则名字和效果会错位。
// cap = 0 表示无持有上限；角色类不进商店，cap 只是占位。
// rarity 是固定档位：普通 / 高级 / 稀有 / 史诗（独特类与角色类一律史诗）。
const SurvivorsLayer::ItemDef SurvivorsLayer::ITEM_DEFS[static_cast<int>(ItemId::Count)] = {
    // 普通类：无持有上限
    { "血袋",       ItemCategory::Normal,    Rarity::Common,   0, glm::vec4(0.90f, 0.32f, 0.36f, 1.0f) },
    { "磨刀石",     ItemCategory::Normal,    Rarity::Common,   0, glm::vec4(0.72f, 0.78f, 0.92f, 1.0f) },
    { "铁片",       ItemCategory::Normal,    Rarity::Advanced, 0, glm::vec4(0.58f, 0.66f, 0.78f, 1.0f) },
    { "猎魂纹",     ItemCategory::Normal,    Rarity::Advanced, 0, glm::vec4(0.95f, 0.62f, 0.42f, 1.0f) },
    { "追星靴",     ItemCategory::Normal,    Rarity::Advanced, 0, glm::vec4(0.62f, 0.88f, 1.00f, 1.0f) },
    { "引魂灯",     ItemCategory::Normal,    Rarity::Common,   0, glm::vec4(0.58f, 0.92f, 0.86f, 1.0f) },
    { "铸币者之锤", ItemCategory::Normal,    Rarity::Common,   0, glm::vec4(1.00f, 0.78f, 0.40f, 1.0f) },
    // 限制类：有持有上限
    { "血月徽记",   ItemCategory::Limited,   Rarity::Rare,     2, glm::vec4(0.88f, 0.28f, 0.42f, 1.0f) },
    { "疾风羽",     ItemCategory::Limited,   Rarity::Rare,     3, glm::vec4(0.70f, 0.92f, 0.78f, 1.0f) },
    { "圣杯",       ItemCategory::Limited,   Rarity::Rare,     2, glm::vec4(0.95f, 0.82f, 0.48f, 1.0f) },
    { "龙鳞",       ItemCategory::Limited,   Rarity::Rare,     2, glm::vec4(0.52f, 0.72f, 0.95f, 1.0f) },
    { "噬魂匕",     ItemCategory::Limited,   Rarity::Advanced, 3, glm::vec4(0.86f, 0.36f, 0.52f, 1.0f) },
    { "哀嚎钟",     ItemCategory::Limited,   Rarity::Rare,     2, glm::vec4(0.74f, 0.64f, 1.00f, 1.0f) },
    { "拾荒护符",   ItemCategory::Limited,   Rarity::Advanced, 3, glm::vec4(0.58f, 0.86f, 0.66f, 1.0f) },
    { "受难者圣痕", ItemCategory::Limited,   Rarity::Rare,     2, glm::vec4(0.92f, 0.44f, 0.40f, 1.0f) },
    // 独特类：全局只能持有一件，固定史诗
    { "不朽之心",   ItemCategory::Unique,    Rarity::Epic,     1, glm::vec4(1.00f, 0.42f, 0.46f, 1.0f) },
    { "荆棘之甲",   ItemCategory::Unique,    Rarity::Epic,     1, glm::vec4(0.92f, 0.46f, 0.72f, 1.0f) },
    { "贪婪之匣",   ItemCategory::Unique,    Rarity::Epic,     1, glm::vec4(1.00f, 0.78f, 0.36f, 1.0f) },
    { "猎月瞳",     ItemCategory::Unique,    Rarity::Epic,     1, glm::vec4(0.66f, 0.60f, 1.00f, 1.0f) },
    { "贪欲之链",   ItemCategory::Unique,    Rarity::Epic,     1, glm::vec4(1.00f, 0.66f, 0.28f, 1.0f) },
    { "黎明誓约",   ItemCategory::Unique,    Rarity::Epic,     1, glm::vec4(1.00f, 0.90f, 0.64f, 1.0f) },
    // 角色类：固定史诗，开局选择
    { "守夜人",     ItemCategory::Character, Rarity::Epic,     1, glm::vec4(0.66f, 0.78f, 1.00f, 1.0f) },
    { "猎手",       ItemCategory::Character, Rarity::Epic,     1, glm::vec4(0.62f, 0.90f, 0.66f, 1.0f) },
    { "修女",       ItemCategory::Character, Rarity::Epic,     1, glm::vec4(0.96f, 0.88f, 0.70f, 1.0f) },
    { "术士",       ItemCategory::Character, Rarity::Epic,     1, glm::vec4(0.86f, 0.52f, 1.00f, 1.0f) },
};

const char* SurvivorsLayer::ItemName(ItemId id)
{
    return ITEM_DEFS[static_cast<int>(id)].name;
}

glm::vec4 SurvivorsLayer::ItemColor(ItemId id)
{
    return ITEM_DEFS[static_cast<int>(id)].color;
}

SurvivorsLayer::ItemCategory SurvivorsLayer::ItemCategoryOf(ItemId id)
{
    return ITEM_DEFS[static_cast<int>(id)].category;
}

SurvivorsLayer::Rarity SurvivorsLayer::ItemRarity(ItemId id)
{
    return ITEM_DEFS[static_cast<int>(id)].rarity;
}

int SurvivorsLayer::ItemCap(ItemId id)
{
    return ITEM_DEFS[static_cast<int>(id)].cap;
}

// ============================ 道具数值 ============================

// 单件道具的加成：稀有度是固定的，所以这里就是**最终数值**（不再乘品质倍率）。
// 描述与结算共用这一份算式
SurvivorsLayer::ItemGain SurvivorsLayer::ItemGainFor(ItemId id)
{
    ItemGain gain;
    switch (id) {
        // 普通类
        case ItemId::BloodBag:     gain.primary = 0.30f;                        break;   // 立刻回复的比例
        case ItemId::Whetstone:    gain.primary = 5.0f;                         break;   // 固定穿甲
        case ItemId::SoulLamp:     gain.primary = 35.0f;                        break;   // 拾取范围
        case ItemId::IronPlate:    gain.primary = 5.0f;                         break;   // 防御
        case ItemId::HunterMark:   gain.primary = 0.07f;                        break;   // 暴击率
        case ItemId::StarBoots:    gain.primary = BARE_STATS.moveSpeed * 0.06f; break;   // 移速（按裸值百分比）
        // 限制类
        case ItemId::Grail:        gain.primary = 0.60f;                        break;   // 每秒回复
        case ItemId::BloodSigil:   gain.primary = 0.12f;                        break;   // 伤害加成
        case ItemId::GaleFeather:  gain.primary = 0.10f;                        break;   // 攻速加成
        case ItemId::DragonScale:  gain.primary = 9.0f;                                  // 防御
                                   gain.secondary = 6.0f; break;                         // 生命上限
        // 独特类
        case ItemId::UndyingHeart: gain.primary = REVIVE_HP_FRACTION; break;             // 复活时回血比例
        case ItemId::ThornMail:    gain.primary = 1.20f; break;                          // 反弹比例
        case ItemId::GreedCoffin:  gain.primary = 1.00f; break;                          // 金币收益加成
        case ItemId::MoonshotPupil: gain.primary = 0.20f;                                // 暴击率
                                    gain.secondary = 0.60f; break;                       // 暴击伤害
        // 事件类：数值只在对应事件发生时读取（触发间隔见 SurvivorsInternal.h 的 *_EVENT_STEP）
        case ItemId::SoulDagger:     gain.primary = 0.35f; break;   // 每次击杀回复的生命
        case ItemId::WailingBell:    gain.primary = 18.0f;  break;  // 每 12 杀的音波伤害
        case ItemId::CoinHammer:     gain.primary = 1.0f;   break;  // 每 10 枚金币额外铸出的金币
        case ItemId::ScavengerCharm: gain.primary = 2.0f;   break;  // 每 10 枚金币回复的生命
        case ItemId::GreedChain:     gain.primary = 4.0f;   break;  // 每枚金币面额的伤害倍率
        case ItemId::MartyrStigma:   gain.primary = 24.0f;          // 每 5 次受伤的暗焰伤害
                                     gain.secondary = 0.06f; break; // 触发时回复的最大生命比例
        case ItemId::DawnPact:       gain.primary = 0.04f;          // 每次过关永久 + 伤害
                                     gain.secondary = 0.15f; break; // 每次过关回复的最大生命比例
        // 角色类
        case ItemId::CharWatchman: gain.primary = 24.0f; gain.secondary = 12.0f; break;  // 生命 / 防御
        case ItemId::CharHunter:   gain.primary = BARE_STATS.moveSpeed * 0.10f;          // 移速
                                   gain.secondary = 0.08f; break;                        // 暴击率
        case ItemId::CharSister:   gain.primary = 0.70f; gain.secondary = 10.0f; break;  // 回复 / 生命
        case ItemId::CharWarlock:  gain.primary = 0.25f; break;                          // 伤害加成
        default: break;
    }
    return gain;
}

void SurvivorsLayer::DescribeItem(ItemId id, std::string& title, std::string& description)
{
    title = ItemName(id);
    const ItemGain gain = ItemGainFor(id);

    switch (id) {
        case ItemId::BloodBag:
            description = std::format("立即回复 {:.0f}% 最大生命。", std::min(1.0f, gain.primary) * 100.0f);
            break;
        case ItemId::Whetstone:
            description = std::format("固定穿甲 +{:.0f}（无视对方这么多点防御）。", gain.primary);
            break;
        case ItemId::IronPlate:
            description = std::format("防御 +{:.0f}（减伤 = 防御/(防御+100)）。", gain.primary);
            break;
        case ItemId::HunterMark:
            description = std::format("暴击率 +{:.0f}%（暴击双倍伤害，上限 100%）。", gain.primary * 100.0f);
            break;
        case ItemId::StarBoots:
            description = std::format("移动速度 +{:.0f}（上限 2 倍裸值）。", gain.primary);
            break;
        case ItemId::SoulLamp:
            description = std::format("金币拾取范围 +{:.0f}。", gain.primary);
            break;
        case ItemId::BloodSigil:
            description = std::format("全部伤害 +{:.0f}%。", gain.primary * 100.0f);
            break;
        case ItemId::GaleFeather:
            description = std::format("攻击速度 +{:.0f}%（所有武器冷却缩短）。", gain.primary * 100.0f);
            break;
        case ItemId::Grail:
            description = std::format("每秒回复 +{:.2f} 生命。", gain.primary);
            break;
        case ItemId::DragonScale:
            description = std::format("防御 +{:.0f} · 生命上限 +{:.0f}。", gain.primary, gain.secondary);
            break;
        case ItemId::UndyingHeart:
            description = std::format("阵亡时以 {:.0f}% 生命复活一次（只能持有一件）。",
                                      gain.primary * 100.0f);
            break;
        case ItemId::ThornMail:
            description = std::format("受到伤害时，对周围 {:.0f} 范围内的敌人反弹 {:.0f}% 伤害。",
                                      THORN_RADIUS, gain.primary * 100.0f);
            break;
        case ItemId::GreedCoffin:
            description = std::format("金币收益 +{:.0f}%。", gain.primary * 100.0f);
            break;
        case ItemId::MoonshotPupil:
            description = std::format("暴击率 +{:.0f}% · 暴击伤害 +{:.0f}%。",
                                      gain.primary * 100.0f, gain.secondary * 100.0f);
            break;
        case ItemId::SoulDagger:
            description = std::format("【击杀事件】每次击杀敌人回复 {:.2f} 生命。", gain.primary);
            break;
        case ItemId::WailingBell:
            description = std::format("【击杀事件】每击杀 {} 个敌人，向周围 {:.0f} 范围发出哀嚎，造成 {:.0f} 点伤害。",
                                      KILL_EVENT_STEP, BELL_RADIUS, gain.primary);
            break;
        case ItemId::CoinHammer:
            description = std::format("【拾金事件】每累计拾取 {} 枚金币，额外铸出 {:.0f} 枚金币（直接进钱包）。",
                                      COIN_EVENT_STEP, gain.primary);
            break;
        case ItemId::ScavengerCharm:
            description = std::format("【拾金事件】每累计拾取 {} 枚金币，回复 {:.0f} 点生命。",
                                      COIN_EVENT_STEP, gain.primary);
            break;
        case ItemId::GreedChain:
            description = std::format("【拾金事件】每次拾取金币，向周围 {:.0f} 范围震出金币碎片：伤害 = 金币面额 × {:.0f}。",
                                      GREED_CHAIN_RADIUS, gain.primary);
            break;
        case ItemId::MartyrStigma:
            description = std::format("【受伤事件】每受到 {} 次伤害，向周围 {:.0f} 范围释放暗焰：伤害 {:.0f} · 回复 {:.0f}% 最大生命。",
                                      HURT_EVENT_STEP, STIGMA_RADIUS, gain.primary, gain.secondary * 100.0f);
            break;
        case ItemId::DawnPact:
            description = std::format("【过关事件】每通过一关永久获得 +{:.0f}% 伤害、+{:.0f} 生命上限，并回复 {:.0f}% 生命。",
                                      gain.primary * 100.0f, DAWN_PACT_MAXHP, gain.secondary * 100.0f);
            break;
        case ItemId::CharWatchman:
            description = std::format("生命上限 +{:.0f} · 防御 +{:.0f}。\n扛得住，适合正面硬顶。",
                                      gain.primary, gain.secondary);
            break;
        case ItemId::CharHunter:
            description = std::format("移动速度 +{:.0f} · 暴击率 +{:.0f}%。\n绕圈风筝，靠暴击输出。",
                                      gain.primary, gain.secondary * 100.0f);
            break;
        case ItemId::CharSister:
            description = std::format("每秒回复 +{:.2f} · 生命上限 +{:.0f}。\n续航最稳，容错最高。",
                                      gain.primary, gain.secondary);
            break;
        case ItemId::CharWarlock:
            description = std::format("全部伤害 +{:.0f}%。\n输出最高，但没有任何防御加成。",
                                      gain.primary * 100.0f);
            break;
        default:
            description = "（未知道具）";
            break;
    }
}

// 结算：属性类直接写进 m_baseStats（和属性卡同样的线性叠加），行为类（复活 / 荆棘 / 金币）落成标志位
void SurvivorsLayer::ApplyItemEffect(ItemId id)
{
    const ItemGain gain = ItemGainFor(id);
    switch (id) {
        case ItemId::BloodBag:
            m_playerHp = std::min(m_playerMaxHp, m_playerHp + m_playerMaxHp * std::min(1.0f, gain.primary));
            break;
        case ItemId::Whetstone:
            m_baseStats.penFlat += gain.primary;
            break;
        case ItemId::IronPlate:
        case ItemId::DragonScale:
            m_baseStats.defense += gain.primary;
            m_baseStats.maxHp += gain.secondary;
            break;
        case ItemId::HunterMark:
            m_baseStats.critChance = std::min(CRIT_CHANCE_CAP, m_baseStats.critChance + gain.primary);
            break;
        case ItemId::StarBoots:
            m_baseStats.moveSpeed = std::min(m_baseStats.moveSpeed + gain.primary,
                                             BARE_STATS.moveSpeed * MOVE_SPEED_CAP_MUL);
            break;
        case ItemId::SoulLamp:
            m_baseStats.pickupRadius += gain.primary;
            break;
        case ItemId::BloodSigil:
            m_baseStats.damageMul += gain.primary;
            break;
        case ItemId::GaleFeather:
            m_baseStats.attackSpeedMul += gain.primary;
            break;
        case ItemId::Grail:
            m_baseStats.regenPerSecond += gain.primary;
            break;
        case ItemId::UndyingHeart:
            ++m_baseStats.reviveCharges;
            break;
        case ItemId::ThornMail:
            m_baseStats.thornRatio += gain.primary;
            break;
        case ItemId::GreedCoffin:
            m_baseStats.coinGainMul += gain.primary;
            break;
        case ItemId::MoonshotPupil:
            m_baseStats.critChance = std::min(CRIT_CHANCE_CAP, m_baseStats.critChance + gain.primary);
            m_baseStats.critDamageMul += gain.secondary;
            break;
        // 事件类：只把数值写进 BaseStats，真正的效果在 SurvivorsEvents.cpp 的 RaiseEvent 里读
        case ItemId::SoulDagger:
            m_baseStats.killHeal += gain.primary;
            break;
        case ItemId::WailingBell:
            m_baseStats.killBurstDamage += gain.primary;
            break;
        case ItemId::CoinHammer:
            m_baseStats.coinEventBonus += static_cast<int>(std::lround(gain.primary));
            break;
        case ItemId::ScavengerCharm:
            m_baseStats.coinHeal += gain.primary;
            break;
        case ItemId::GreedChain:
            m_baseStats.coinBurstRatio += gain.primary;
            break;
        case ItemId::MartyrStigma:
            m_baseStats.hurtBurstDamage += gain.primary;
            m_baseStats.hurtBurstHeal += gain.secondary;
            break;
        case ItemId::DawnPact:
            m_baseStats.stageDamageGain += gain.primary;
            m_baseStats.stageMaxHpGain += DAWN_PACT_MAXHP;
            m_baseStats.stageHealRatio += gain.secondary;
            break;
        case ItemId::CharWatchman:
            m_baseStats.maxHp += gain.primary;
            m_baseStats.defense += gain.secondary;
            break;
        case ItemId::CharHunter:
            m_baseStats.moveSpeed = std::min(m_baseStats.moveSpeed + gain.primary,
                                             BARE_STATS.moveSpeed * MOVE_SPEED_CAP_MUL);
            m_baseStats.critChance = std::min(CRIT_CHANCE_CAP, m_baseStats.critChance + gain.secondary);
            break;
        case ItemId::CharSister:
            m_baseStats.regenPerSecond += gain.primary;
            m_baseStats.maxHp += gain.secondary;
            break;
        case ItemId::CharWarlock:
            m_baseStats.damageMul += gain.primary;
            break;
        default:
            break;
    }
}

// ============================ 背包与持有上限 ============================

void SurvivorsLayer::ResetItems()
{
    m_itemCount.fill(0);
    m_shopOffers.clear();
    m_shopRerolls = 0;
    m_pendingWeaponPicks = 0;
    m_shopPending = false;
}

int SurvivorsLayer::ItemCount(ItemId id) const
{
    return m_itemCount[static_cast<int>(id)];
}

int SurvivorsLayer::UniqueHeld() const
{
    int total = 0;
    for (int i = 0; i < static_cast<int>(ItemId::Count); ++i) {
        if (ItemCategoryOf(static_cast<ItemId>(i)) == ItemCategory::Unique) {
            total += m_itemCount[i];
        }
    }
    return total;
}

bool SurvivorsLayer::CanAcquireItem(ItemId id) const
{
    const int index = static_cast<int>(id);
    switch (ItemCategoryOf(id)) {
        case ItemCategory::Character:
            return false;                                  // 角色只在开局选，不进商店
        case ItemCategory::Unique:
            // 独特类：全局只能持有一件（手上已有任意独特道具就不再出价）
            return UniqueHeld() == 0 && m_itemCount[index] == 0;
        case ItemCategory::Limited: {
            const int cap = ItemCap(id);
            return cap <= 0 || m_itemCount[index] < cap;
        }
        case ItemCategory::Normal:
        default:
            return true;                                   // 普通类没有持有上限
    }
}

void SurvivorsLayer::AcquireItem(ItemId id)
{
    const int index = static_cast<int>(id);
    const float oldMaxHp = m_playerMaxHp;

    ApplyItemEffect(id);
    ++m_itemCount[index];

    RecalculateStats();
    // 生命上限涨了就把多出来的血一并补上：买体魄类道具不会「亏血」
    m_playerHp = std::min(m_playerMaxHp, m_playerHp + std::max(0.0f, m_playerMaxHp - oldMaxHp));

    SpawnRing(m_playerPosition, 96.0f, 0.40f, ItemColor(id));
    SpawnParticles(m_playerPosition, 10, ItemColor(id), 190.0f, 7.0f, 0.5f);
}

int SurvivorsLayer::CoinGain(int base) const
{
    return std::max(1, static_cast<int>(std::lround(static_cast<float>(base) * m_baseStats.coinGainMul)));
}

// ============================ 商店 ============================

int SurvivorsLayer::ShopPrice(Rarity rarity) const
{
    const int base = ITEM_PRICE[static_cast<int>(rarity)];
    const float stageScale = 1.0f + ITEM_PRICE_STAGE_STEP * static_cast<float>(m_stage - 1);
    return static_cast<int>(std::lround(static_cast<float>(base) * stageScale));
}

int SurvivorsLayer::ShopRerollCost() const
{
    return SHOP_REROLL_BASE + SHOP_REROLL_STEP * m_shopRerolls;
}

void SurvivorsLayer::OpenShop()
{
    m_shopPending = false;
    m_state = State::Shop;
    m_stageBanner = 0.0f;   // 进关横幅会被商店标题压住，直接用商店说明代替
    m_shopRerolls = 0;
    m_shopOffers.clear();
    RollShopOffers();
    m_shake = 0.0f;
    m_camera.SetShake(glm::vec2(0.0f));
    PlaySfx(SfxId::LevelUp, 0.70f, 0.88f);   // 压低的「叮」：商店开门
    OI_INFO("Survivors: 第 {} 关商店开启（{} 件货，金币 {}）",
            m_stage, static_cast<int>(m_shopOffers.size()), m_coins);
}

void SurvivorsLayer::RollShopOffers(const std::vector<ShopOffer>& avoid)
{
    // 候选池：所有非角色、且没到持有上限的道具
    std::vector<ItemId> pool;
    for (int i = 0; i < static_cast<int>(ItemId::Count); ++i) {
        const ItemId id = static_cast<ItemId>(i);
        if (CanAcquireItem(id)) {
            pool.push_back(id);
        }
    }

    // 整组刷新时尽量避开刚才那几件（池子够大才过滤，否则保持原池）
    if (!avoid.empty()) {
        std::vector<ItemId> filtered;
        for (const ItemId id : pool) {
            bool repeated = false;
            for (const ShopOffer& offer : avoid) {
                if (!offer.sold && offer.id == id) {
                    repeated = true;
                    break;
                }
            }
            if (!repeated) {
                filtered.push_back(id);
            }
        }
        if (static_cast<int>(filtered.size()) >= SHOP_SLOT_COUNT) {
            pool = std::move(filtered);
        }
    }

    // 出货权重按**道具的固定稀有度**给：普通最常见，越往上越稀
    const auto weightOf = [](ItemId id) {
        return ITEM_WEIGHT[static_cast<int>(ItemRarity(id))];
    };

    // 逐件抽不重复的货位
    for (int slot = 0; slot < SHOP_SLOT_COUNT && !pool.empty(); ++slot) {
        float total = 0.0f;
        for (const ItemId id : pool) {
            total += weightOf(id);
        }
        float cursor = Rand(0.0f, total);
        int pick = static_cast<int>(pool.size()) - 1;
        for (int i = 0; i < static_cast<int>(pool.size()); ++i) {
            if (cursor < weightOf(pool[i])) {
                pick = i;
                break;
            }
            cursor -= weightOf(pool[i]);
        }

        const ItemId chosen = pool[static_cast<size_t>(pick)];
        pool.erase(pool.begin() + pick);

        // 稀有度跟着道具走，不重掷：铁片永远是高级，猎月瞳永远是史诗
        ShopOffer offer;
        offer.id = chosen;
        offer.price = ShopPrice(ItemRarity(chosen));
        m_shopOffers.push_back(offer);
    }
}

void SurvivorsLayer::RerollShop()
{
    if (m_state != State::Shop) {
        return;
    }
    const int cost = ShopRerollCost();
    if (m_coins < cost) {
        PlaySfx(SfxId::Hurt, 0.30f, 1.70f);
        return;
    }
    m_coins -= cost;
    ++m_shopRerolls;

    const std::vector<ShopOffer> previous = m_shopOffers;
    m_shopOffers.clear();
    RollShopOffers(previous);

    PlaySfx(SfxId::LevelUp, 0.45f, 1.28f);
    SpawnRing(m_playerPosition, 80.0f, 0.30f, C_GOLD);
}

bool SurvivorsLayer::BuyShopOffer(int slot)
{
    if (m_state != State::Shop || slot < 0 || slot >= static_cast<int>(m_shopOffers.size())) {
        return false;
    }
    ShopOffer& offer = m_shopOffers[static_cast<size_t>(slot)];
    if (offer.sold || !CanAcquireItem(offer.id) || m_coins < offer.price) {
        PlaySfx(SfxId::Hurt, 0.30f, 1.70f);
        return false;
    }

    const ItemId id = offer.id;
    m_coins -= offer.price;
    offer.sold = true;

    AcquireItem(id);
    PlaySfx(SfxId::Pickup, 1.0f, 0.84f);
    OI_INFO("Survivors: 买入 {}（{}）花 {} 币，剩余 {}",
            ItemName(id), RARITY_NAMES[static_cast<int>(ItemRarity(id))], offer.price, m_coins);
    return true;
}

// ============================ 开局：角色 + 武器 ============================

SurvivorsLayer::ItemId SurvivorsLayer::CharacterItem(int index)
{
    int seen = 0;
    for (int i = 0; i < static_cast<int>(ItemId::Count); ++i) {
        const ItemId id = static_cast<ItemId>(i);
        if (ItemCategoryOf(id) != ItemCategory::Character) {
            continue;
        }
        if (seen == index) {
            return id;
        }
        ++seen;
    }
    return ItemId::CharWatchman;
}

int SurvivorsLayer::CharacterCount()
{
    int total = 0;
    for (int i = 0; i < static_cast<int>(ItemId::Count); ++i) {
        if (ItemCategoryOf(static_cast<ItemId>(i)) == ItemCategory::Character) {
            ++total;
        }
    }
    return total;
}

// 角色类道具 → 0..3（渲染按这个下标选形象；非角色一律当第一个）
int SurvivorsLayer::CharacterIndexOf(ItemId id)
{
    int seen = 0;
    for (int i = 0; i < static_cast<int>(ItemId::Count); ++i) {
        const ItemId candidate = static_cast<ItemId>(i);
        if (ItemCategoryOf(candidate) != ItemCategory::Character) {
            continue;
        }
        if (candidate == id) {
            return seen;
        }
        ++seen;
    }
    return 0;
}

void SurvivorsLayer::ApplyCharacter(ItemId character)
{
    m_character = character;
    ApplyItemEffect(character);   // 角色固定史诗，数值就是 ItemGainFor 里那组
}

void SurvivorsLayer::BeginRun()
{
    if (m_state != State::Setup) {
        return;   // 只允许从开局选择界面进入（防止连点 / 键盘与鼠标同帧触发，把角色加成结算两次）
    }

    // 角色：在 ResetRun 给的开局裸值之上结算一次初始属性
    ApplyCharacter(CharacterItem(m_setupCharacter));

    // 武器：开局只带一把，等级 1（其余武器从 0 级起，靠每 5 级的武器选择解锁）
    m_weaponLevel.fill(0);
    m_weaponLevel[static_cast<size_t>(std::clamp(m_setupWeapon, 0, WEAPON_COUNT - 1))] = 1;

    RecalculateStats();
    m_playerHp = m_playerMaxHp;

    m_state = State::Playing;
    m_stageBanner = 2.2f;
    m_stageBannerBoss = false;
    m_levelUpLockout = 0.6f;
    m_hurtCooldown = 2.0f;   // 开局短暂无敌，避免刚开局就被贴脸的蝙蝠打一下

    // 开局就在玩家附近放几只蝙蝠：第一波击杀正好把等级顶到 2，立刻展示升级卡
    for (int i = 0; i < 4; ++i) {
        const float angle = TAU * static_cast<float>(i) / 4.0f + Rand(-0.35f, 0.35f);
        SpawnEnemy(EnemyType::Bat, m_playerPosition +
                   glm::vec2(std::cos(angle), std::sin(angle)) * Rand(160.0f, 250.0f));
    }

    EnsureBgm(false);
    SpawnRing(m_playerPosition, 160.0f, 0.6f, ItemColor(m_character));
    PlaySfx(SfxId::StageClear, 0.55f, 1.34f);
    OI_INFO("Survivors: 开局 —— 角色 {} · 武器 {}",
            ItemName(m_character),
            WEAPON_NAMES[std::clamp(m_setupWeapon, 0, WEAPON_COUNT - 1)]);
}

// ============================ 每 5 级：武器选择 ============================

std::string SurvivorsLayer::DescribeWeapon(int weaponIndex, int currentLevel)
{
    // currentLevel = 0 表示还没有这把武器（这次选它就是从 1 级解锁）；否则展示下一级的提升
    const int level = std::clamp(currentLevel, 0, WEAPON_MAX_LEVEL);
    const int next = std::min(WEAPON_MAX_LEVEL, level + 1);

    switch (weaponIndex) {
        case 1:
            if (level <= 0) {
                return std::format("自身周围持续脉动圣光\n伤害 {:.0f} · 半径 {:.0f}",
                                   AURA.damage[1], AURA.radius[1]);
            }
            return std::format("伤害 {:.0f} → {:.0f}\n半径 {:.0f} → {:.0f}",
                               AURA.damage[level], AURA.damage[next],
                               AURA.radius[level], AURA.radius[next]);
        case 2:
            if (level <= 0) {
                return std::format("掷出回旋镰，飞出后折返\n伤害 {:.0f} · 回旋镰 {} 把",
                                   SCYTHE.damage[1], SCYTHE.count[1]);
            }
            return std::format("伤害 {:.0f} → {:.0f}\n回旋镰 {} → {} 把",
                               SCYTHE.damage[level], SCYTHE.damage[next],
                               SCYTHE.count[level], SCYTHE.count[next]);
        case 3:
            if (level <= 0) {
                return std::format("召唤落雷劈击敌群（范围伤害）\n伤害 {:.0f} · 范围 {:.0f} · 落雷 {} 道",
                                   STORM.damage[1], STORM.radius[1], STORM.count[1]);
            }
            return std::format("伤害 {:.0f} → {:.0f} · 范围 {:.0f} → {:.0f}\n落雷 {} → {} 道",
                               STORM.damage[level], STORM.damage[next],
                               STORM.radius[level], STORM.radius[next],
                               STORM.count[level], STORM.count[next]);
        case 0:
        default:
            if (level <= 0) {
                return std::format("锁定最近敌人，射出月刃\n伤害 {:.0f} · 飞刃 {} · 穿透 {}",
                                   BLADE.damage[1], BLADE.count[1], BLADE.pierce[1]);
            }
            return std::format("伤害 {:.0f} → {:.0f}\n飞刃 {} → {} · 穿透 {} → {}",
                               BLADE.damage[level], BLADE.damage[next],
                               BLADE.count[level], BLADE.count[next],
                               BLADE.pierce[level], BLADE.pierce[next]);
    }
}

void SurvivorsLayer::StartWeaponPick()
{
    // 全部武器满级：这次机会没法用，直接折成金币（放在这里而不是 UI 里，保证每一条进入路径都一致）
    bool anyAvailable = false;
    for (int weapon = 0; weapon < WEAPON_COUNT; ++weapon) {
        if (m_weaponLevel[static_cast<size_t>(weapon)] < WEAPON_MAX_LEVEL) {
            anyAvailable = true;
            break;
        }
    }
    if (!anyAvailable) {
        --m_pendingWeaponPicks;
        m_coins += WEAPON_PICK_FALLBACK_COINS;
        m_coinsEarned += WEAPON_PICK_FALLBACK_COINS;
        OI_INFO("Survivors: 武器已全部满级，第 {} 级的选择折成 {} 金币",
                m_level, WEAPON_PICK_FALLBACK_COINS);
        ContinueRun();
        return;
    }

    m_state = State::WeaponPick;
    m_levelFlash = 1.0f;
    m_shake = 0.0f;
    m_camera.SetShake(glm::vec2(0.0f));
    PlaySfx(SfxId::LevelUp, 1.0f, 1.14f);
}

void SurvivorsLayer::ChooseWeapon(int weaponIndex)
{
    if (m_state != State::WeaponPick || weaponIndex < 0 || weaponIndex >= WEAPON_COUNT) {
        return;
    }
    if (m_weaponLevel[static_cast<size_t>(weaponIndex)] >= WEAPON_MAX_LEVEL) {
        return;
    }
    if (m_pendingWeaponPicks <= 0) {
        return;
    }

    const int before = m_weaponLevel[static_cast<size_t>(weaponIndex)];
    ++m_weaponLevel[static_cast<size_t>(weaponIndex)];
    --m_pendingWeaponPicks;

    const glm::vec4 color = WEAPON_COLORS[weaponIndex];
    SpawnRing(m_playerPosition, 130.0f, 0.5f, color);
    SpawnParticles(m_playerPosition, 14, color, 220.0f, 8.0f, 0.55f);
    PlaySfx(SfxId::Pickup, 1.0f, 1.10f);

    OI_INFO("Survivors: 第 {} 级选了武器 {}（Lv.{} → Lv.{}）", m_level,
            WEAPON_NAMES[weaponIndex], before, before + 1);

    ContinueRun();
}

// ============================ 面板排队 ============================

// 一次事件可能同时排上好几件事（一次击杀顶两级、过关时还挂着升级）：统一按
// 升级 → 选武器 → 商店 的顺序处理，全部处理完才回到 Playing
void SurvivorsLayer::ContinueRun()
{
    m_rerollCount = 0;
    if (m_pendingLevelUps > 0) {
        StartLevelUp();
        return;
    }
    if (m_pendingWeaponPicks > 0) {
        StartWeaponPick();
        return;
    }
    if (m_shopPending) {
        OpenShop();
        return;
    }
    m_state = State::Playing;
    m_levelFlash = 1.0f;
    m_levelUpLockout = 1.5f;
}
