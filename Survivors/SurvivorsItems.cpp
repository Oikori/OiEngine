// 《永夜幸存者》道具系统：道具目录、持有上限、效果结算、商店（掷货 / 整组刷新 / 购买），
// 以及开局（选角色 + 武器）与每 5 级的武器选择。
//
// 道具有两条独立的属性：
//   种类决定**能不能重复拿**（普通无上限 / 限制类有上限 / 角色开局选），
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

// 编号就是 ItemId 的枚举值（目录是 map，顺序不影响查找，这里只是便于对照）：
//   0 血袋 1 磨刀石 2 铁片 3 猎魂纹 4 追星靴 5 磁石
//   6 铁钉 7 猎手斗篷 8 迷雾灯
//   9 猩红吊坠 10 秘银指环 11 暗影长袍 12 流光冠冕
//   13 铸币者之锤 14 血月徽记 15 疾风羽 16 圣杯 17 龙鳞 18 哀嚎钟 19 拾荒护符 20 受难者圣痕
//   21 寒铁护腕 22 血玫瑰 23 断刃 24 诅咒之环 25 星屑
//   26 破晓之刃 27 淬血匕首 28 沙漏 29 节拍器 30 贪婪契约 31 商人之瞳 32 成长之书 33 星辉残页
//   34 不朽之心 35 荆棘之甲 36 贪婪之匣 37 猎月瞳 38 贪欲之链 39 黎明誓约 40 狂战徽记 41 血契
//   42 守夜人 43 猎手 44 修女 45 术士
// 目录是 unordered_map<ItemId, ItemDef>：键就是 ItemId，不再依赖枚举顺序。
// 新增道具 = ItemId 加一项 + 这里加一行；漏登记会在 .at() 处直接抛出来，不会静默错位。
// cap = 0 表示无持有上限；角色类不进商店，cap 只是占位。
// rarity 是固定档位：普通 / 高级 / 稀有 / 史诗（原独特道具与角色类一律史诗）。
const std::unordered_map<SurvivorsLayer::ItemId, SurvivorsLayer::ItemDef> SurvivorsLayer::ITEM_DEFS = {
    // 普通类：无持有上限
    { ItemId::BloodBag,       { "血袋",       ItemCategory::Normal,    Rarity::Common,   0 } },
    { ItemId::Whetstone,      { "磨刀石",     ItemCategory::Normal,    Rarity::Common,   0 } },
    { ItemId::IronPlate,      { "铁片",       ItemCategory::Normal,    Rarity::Advanced, 0 } },
    { ItemId::HunterMark,     { "猎魂纹",     ItemCategory::Normal,    Rarity::Advanced, 0 } },
    { ItemId::StarBoots,      { "追星靴",     ItemCategory::Normal,    Rarity::Advanced, 0 } },
    { ItemId::SoulLamp,       { "磁石",       ItemCategory::Normal,    Rarity::Common,   0 } },
    { ItemId::IronNail,       { "铁钉",       ItemCategory::Normal,    Rarity::Common,   0 } },
    { ItemId::HunterCloak,    { "猎手斗篷",   ItemCategory::Normal,    Rarity::Advanced, 0 } },
    { ItemId::MistLantern,    { "迷雾灯",     ItemCategory::Normal,    Rarity::Common,   0 } },
    { ItemId::CrimsonPendant, { "猩红吊坠",   ItemCategory::Normal,    Rarity::Rare,     0 } },
    { ItemId::MithrilRing,    { "秘银指环",   ItemCategory::Normal,    Rarity::Rare,     0 } },
    { ItemId::ShadowRobe,     { "暗影长袍",   ItemCategory::Normal,    Rarity::Epic,     0 } },
    { ItemId::LumenCrown,     { "流光冠冕",   ItemCategory::Normal,    Rarity::Epic,     0 } },
    // 限制类：有持有上限
    { ItemId::CoinHammer,     { "铸币者之锤", ItemCategory::Limited,   Rarity::Common,   3 } },
    { ItemId::BloodSigil,     { "血月徽记",   ItemCategory::Limited,   Rarity::Rare,     2 } },
    { ItemId::GaleFeather,    { "疾风羽",     ItemCategory::Limited,   Rarity::Rare,     3 } },
    { ItemId::Grail,          { "圣杯",       ItemCategory::Limited,   Rarity::Rare,     2 } },
    { ItemId::DragonScale,    { "龙鳞",       ItemCategory::Limited,   Rarity::Rare,     2 } },
    { ItemId::WailingBell,    { "哀嚎钟",     ItemCategory::Limited,   Rarity::Rare,     2 } },
    { ItemId::ScavengerCharm, { "拾荒护符",   ItemCategory::Limited,   Rarity::Advanced, 3 } },
    { ItemId::MartyrStigma,   { "受难者圣痕", ItemCategory::Limited,   Rarity::Rare,     2 } },
    { ItemId::ColdBracer,     { "寒铁护腕",   ItemCategory::Limited,   Rarity::Advanced, 2 } },
    { ItemId::BloodRose,      { "血玫瑰",     ItemCategory::Limited,   Rarity::Advanced, 2 } },
    { ItemId::BrokenBlade,    { "断刃",       ItemCategory::Limited,   Rarity::Rare,     1 } },
    { ItemId::CursedRing,     { "诅咒之环",   ItemCategory::Limited,   Rarity::Rare,     1 } },
    { ItemId::StarShard,      { "星屑",       ItemCategory::Limited,   Rarity::Rare,     3 } },
    { ItemId::DawnBlade,      { "破晓之刃",   ItemCategory::Limited,   Rarity::Rare,     1 } },
    { ItemId::BloodDagger,    { "淬血匕首",   ItemCategory::Limited,   Rarity::Advanced, 2 } },
    { ItemId::Hourglass,      { "沙漏",       ItemCategory::Limited,   Rarity::Common,   2 } },
    { ItemId::Metronome,      { "节拍器",     ItemCategory::Limited,   Rarity::Rare,     1 } },
    { ItemId::GreedPact,      { "贪婪契约",   ItemCategory::Limited,   Rarity::Advanced, 2 } },
    { ItemId::MerchantEye,    { "商人之瞳",   ItemCategory::Limited,   Rarity::Epic,     1 } },
    { ItemId::TomeOfGrowth,   { "成长之书",   ItemCategory::Limited,   Rarity::Common,   2 } },
    { ItemId::StarlightPage,  { "星辉残页",   ItemCategory::Limited,   Rarity::Rare,     1 } },
    // 限制类（史诗档）：原「独特类」并入这里，每件上限 1
    { ItemId::UndyingHeart,   { "不朽之心",   ItemCategory::Limited,   Rarity::Epic,     1 } },
    { ItemId::ThornMail,      { "荆棘之甲",   ItemCategory::Limited,   Rarity::Epic,     1 } },
    { ItemId::GreedCoffin,    { "贪婪之匣",   ItemCategory::Limited,   Rarity::Epic,     1 } },
    { ItemId::MoonshotPupil,  { "猎月瞳",     ItemCategory::Limited,   Rarity::Epic,     1 } },
    { ItemId::GreedChain,     { "贪欲之链",   ItemCategory::Limited,   Rarity::Epic,     1 } },
    { ItemId::DawnPact,       { "黎明誓约",   ItemCategory::Limited,   Rarity::Epic,     1 } },
    { ItemId::BerserkSigil,   { "狂战徽记",   ItemCategory::Limited,   Rarity::Epic,     1 } },
    { ItemId::BloodPact,      { "血契",       ItemCategory::Limited,   Rarity::Epic,     1 } },
    // 角色类：固定史诗，开局选择
    { ItemId::CharWatchman,   { "守夜人",     ItemCategory::Character, Rarity::Epic,     1 } },
    { ItemId::CharHunter,     { "猎手",       ItemCategory::Character, Rarity::Epic,     1 } },
    { ItemId::CharSister,     { "修女",       ItemCategory::Character, Rarity::Epic,     1 } },
    { ItemId::CharWarlock,    { "术士",       ItemCategory::Character, Rarity::Epic,     1 } },
};

const char* SurvivorsLayer::ItemName(ItemId id)
{
    return ITEM_DEFS.at(id).name;
}

glm::vec4 SurvivorsLayer::ItemColor(ItemId id)
{
    // 颜色不在目录里：角色类取各自配色，普通 / 限制道具直接取稀有度色
    if (ItemCategoryOf(id) == ItemCategory::Character) {
        return CHARACTER_COLORS[CharacterIndexOf(id)];
    }
    return RARITY_COLORS[static_cast<int>(ItemRarity(id))];
}

SurvivorsLayer::ItemCategory SurvivorsLayer::ItemCategoryOf(ItemId id)
{
    return ITEM_DEFS.at(id).category;
}

SurvivorsLayer::Rarity SurvivorsLayer::ItemRarity(ItemId id)
{
    return ITEM_DEFS.at(id).rarity;
}

int SurvivorsLayer::ItemCap(ItemId id)
{
    return ITEM_DEFS.at(id).cap;
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
        // 原独特类（现限制类 · 史诗）
        case ItemId::UndyingHeart: gain.primary = REVIVE_HP_FRACTION; break;             // 复活时回血比例
        case ItemId::ThornMail:    gain.primary = 1.20f; break;                          // 反弹比例
        case ItemId::GreedCoffin:  gain.primary = 1.00f; break;                          // 金币收益加成
        case ItemId::MoonshotPupil: gain.primary = 0.20f;                                // 暴击率
                                    gain.secondary = 0.60f; break;                       // 暴击伤害
        // 事件类：数值只在对应事件发生时读取（触发间隔见 SurvivorsInternal.h 的 *_EVENT_STEP）
        case ItemId::WailingBell:    gain.primary = 18.0f;  break;  // 每 12 杀的音波伤害
        case ItemId::CoinHammer:     gain.primary = 1.0f;   break;  // 每 10 枚金币额外铸出的金币
        case ItemId::ScavengerCharm: gain.primary = 2.0f;   break;  // 每 10 枚金币回复的生命
        case ItemId::GreedChain:     gain.primary = 4.0f;   break;  // 每枚金币面额的伤害倍率
        case ItemId::MartyrStigma:   gain.primary = 24.0f;          // 每 5 次受伤的暗焰伤害
                                     gain.secondary = 0.06f; break; // 触发时回复的最大生命比例
        case ItemId::DawnPact:       gain.primary = 0.04f;          // 每次过关永久 + 伤害
                                     gain.secondary = 0.15f; break; // 每次过关回复的最大生命比例
        case ItemId::IronNail:      gain.primary = 2.0f;   gain.secondary = 0.015f;  break;  // 穿甲 / 暴击率
        case ItemId::HunterCloak:   gain.primary = 16.0f;  gain.secondary = 0.03f;   break;  // 移速 / 暴击率
        case ItemId::MistLantern:   gain.primary = 40.0f;  gain.secondary = 0.10f;   break;  // 拾取 / 金币收益
        case ItemId::CrimsonPendant: gain.primary = 0.05f; gain.secondary = 0.15f;   break;  // 暴击率 / 暴击伤害
        case ItemId::MithrilRing:   gain.primary = 7.0f;   gain.secondary = 0.40f;   break;  // 防御 / 每秒回复
        case ItemId::ShadowRobe:    gain.primary = 24.0f;  gain.secondary = 4.0f;    break;  // 移速 / 防御
        case ItemId::LumenCrown:    gain.primary = 0.10f;  gain.secondary = 0.20f;   break;  // 攻速 / 暴击伤害
        case ItemId::ColdBracer:    gain.primary = 6.0f;   gain.secondary = 0.05f;   break;  // 防御 / 攻速
        case ItemId::BloodRose:     gain.primary = 14.0f;  gain.secondary = 0.50f;   break;  // 生命上限 / 每秒回复
        case ItemId::BrokenBlade:   gain.primary = 0.12f;  gain.secondary = 0.08f;   break;  // 伤害 / 攻速
        case ItemId::CursedRing:    gain.primary = 0.30f;  gain.secondary = -12.0f;  break;  // 伤害 / 生命上限（负面）
        case ItemId::StarShard:     gain.primary = 0.05f;  gain.secondary = 0.25f;   break;  // 暴击率 / 暴击伤害
        case ItemId::DawnBlade:     gain.primary = 25.0f;  break;   // 每 8 次暴击的爆发伤害
        case ItemId::BloodDagger:   gain.primary = 0.12f;  break;   // 每次暴击回复的生命
        case ItemId::Hourglass:     gain.primary = 3.0f;   break;   // 每 8 秒回复的生命
        case ItemId::Metronome:     gain.primary = 18.0f;  break;   // 每 8 秒对最近敌人的伤害
        case ItemId::GreedPact:     gain.primary = 4.0f;   break;   // 每累计 50 金币回复的生命
        case ItemId::MerchantEye:   gain.primary = 0.01f;  break;   // 每累计 50 金币永久 +伤害
        case ItemId::TomeOfGrowth:  gain.primary = 0.15f;  break;   // 每次升级回复最大生命的比例
        case ItemId::StarlightPage: gain.primary = 1.0f;   break;   // 每次升级 +固定穿甲
        case ItemId::BerserkSigil:  gain.primary = 0.30f;  gain.secondary = -8.0f;   break;  // 攻速 / 防御（负面）
        case ItemId::BloodPact:     gain.primary = 30.0f;  gain.secondary = 0.40f;   break;  // 生命上限 / 击杀回血
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
            description = std::format("移动速度 +{:.0f}（没有上限）。", gain.primary);
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
        case ItemId::IronNail:
            description = std::format("固定穿甲 +{:.0f} · 暴击率 +{:.1f}%。", gain.primary, gain.secondary * 100.0f);
            break;
        case ItemId::HunterCloak:
            description = std::format("移动速度 +{:.0f} · 暴击率 +{:.0f}%。", gain.primary, gain.secondary * 100.0f);
            break;
        case ItemId::MistLantern:
            description = std::format("金币拾取范围 +{:.0f} · 金币收益 +{:.0f}%。", gain.primary, gain.secondary * 100.0f);
            break;
        case ItemId::CrimsonPendant:
            description = std::format("暴击率 +{:.1f}% · 暴击伤害 +{:.0f}%。",
                                      gain.primary * 100.0f, gain.secondary * 100.0f);
            break;
        case ItemId::MithrilRing:
            description = std::format("防御 +{:.0f}（减伤 = 防御/(防御+100)）· 每秒回复 +{:.2f}。",
                                      gain.primary, gain.secondary);
            break;
        case ItemId::ShadowRobe:
            description = std::format("移动速度 +{:.0f}（没有上限）· 防御 +{:.0f}。", gain.primary, gain.secondary);
            break;
        case ItemId::LumenCrown:
            description = std::format("攻击速度 +{:.0f}% · 暴击伤害 +{:.0f}%。",
                                      gain.primary * 100.0f, gain.secondary * 100.0f);
            break;
        case ItemId::ColdBracer:
            description = std::format("防御 +{:.0f} · 攻击速度 +{:.0f}%。", gain.primary, gain.secondary * 100.0f);
            break;
        case ItemId::BloodRose:
            description = std::format("生命上限 +{:.0f} · 每秒回复 +{:.2f}。", gain.primary, gain.secondary);
            break;
        case ItemId::BrokenBlade:
            description = std::format("全部伤害 +{:.0f}% · 攻击速度 +{:.0f}%。",
                                      gain.primary * 100.0f, gain.secondary * 100.0f);
            break;
        case ItemId::CursedRing:
            description = std::format("全部伤害 +{:.0f}%，代价是生命上限 −{:.0f}。",
                                      gain.primary * 100.0f, -gain.secondary);
            break;
        case ItemId::StarShard:
            description = std::format("暴击率 +{:.0f}% · 暴击伤害 +{:.0f}%。",
                                      gain.primary * 100.0f, gain.secondary * 100.0f);
            break;
        case ItemId::DawnBlade:
            description = std::format("【暴击事件】每累计 {} 次暴击，向周围 {:.0f} 范围释放一次光爆，造成 {:.0f} 点伤害。",
                                      CRIT_EVENT_STEP, CRIT_BURST_RADIUS, gain.primary);
            break;
        case ItemId::BloodDagger:
            description = std::format("【暴击事件】每次暴击回复 {:.2f} 点生命。", gain.primary);
            break;
        case ItemId::Hourglass:
            description = std::format("【计时事件】每 {:.0f} 秒回复 {:.0f} 点生命。",
                                      TICK_EVENT_SECONDS, gain.primary);
            break;
        case ItemId::Metronome:
            description = std::format("【计时事件】每 {:.0f} 秒向最近的敌人打出一击，伤害 {:.0f}。",
                                      TICK_EVENT_SECONDS, gain.primary);
            break;
        case ItemId::GreedPact:
            description = std::format("【消费事件】每累计花费 {} 金币，回复 {:.0f} 点生命。",
                                      SPEND_EVENT_STEP, gain.primary);
            break;
        case ItemId::MerchantEye:
            description = std::format("【消费事件】每累计花费 {} 金币，永久 +{:.0f}% 伤害。",
                                      SPEND_EVENT_STEP, gain.primary * 100.0f);
            break;
        case ItemId::TomeOfGrowth:
            description = std::format("【升级事件】每次升级回复 {:.0f}% 最大生命。", gain.primary * 100.0f);
            break;
        case ItemId::StarlightPage:
            description = std::format("【升级事件】每次升级永久 +{:.0f} 点固定穿甲。", gain.primary);
            break;
        case ItemId::BerserkSigil:
            description = std::format("攻击速度 +{:.0f}%，代价是防御 −{:.0f}。",
                                      gain.primary * 100.0f, -gain.secondary);
            break;
        case ItemId::BloodPact:
            description = std::format("生命上限 +{:.0f} · 【击杀事件】每次击杀回复 {:.2f} 生命。",
                                      gain.primary, gain.secondary);
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
            m_baseStats.moveSpeed += gain.primary;   // 移速不设上限
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
        case ItemId::IronNail:
            m_baseStats.penFlat += gain.primary;
            m_baseStats.critChance = std::min(CRIT_CHANCE_CAP, m_baseStats.critChance + gain.secondary);
            break;
        case ItemId::HunterCloak:
            m_baseStats.moveSpeed += gain.primary;
            m_baseStats.critChance = std::min(CRIT_CHANCE_CAP, m_baseStats.critChance + gain.secondary);
            break;
        case ItemId::MistLantern:
            m_baseStats.pickupRadius += gain.primary;
            m_baseStats.coinGainMul += gain.secondary;
            break;
        case ItemId::CrimsonPendant:
            m_baseStats.critChance = std::min(CRIT_CHANCE_CAP, m_baseStats.critChance + gain.primary);
            m_baseStats.critDamageMul += gain.secondary;
            break;
        case ItemId::MithrilRing:
            m_baseStats.defense += gain.primary;
            m_baseStats.regenPerSecond += gain.secondary;
            break;
        case ItemId::ShadowRobe:
            m_baseStats.moveSpeed += gain.primary;   // 移速不设上限
            m_baseStats.defense += gain.secondary;
            break;
        case ItemId::LumenCrown:
            m_baseStats.attackSpeedMul += gain.primary;
            m_baseStats.critDamageMul += gain.secondary;
            break;
        case ItemId::ColdBracer:
            m_baseStats.defense += gain.primary;
            m_baseStats.attackSpeedMul += gain.secondary;
            break;
        case ItemId::BloodRose:
            m_baseStats.maxHp += gain.primary;
            m_baseStats.regenPerSecond += gain.secondary;
            break;
        case ItemId::BrokenBlade:
            m_baseStats.damageMul += gain.primary;
            m_baseStats.attackSpeedMul += gain.secondary;
            break;
        case ItemId::CursedRing:
            m_baseStats.damageMul += gain.primary;
            m_baseStats.maxHp = std::max(1.0f, m_baseStats.maxHp + gain.secondary);   // 生命上限不能被压到 0
            break;
        case ItemId::StarShard:
            m_baseStats.critChance = std::min(CRIT_CHANCE_CAP, m_baseStats.critChance + gain.primary);
            m_baseStats.critDamageMul += gain.secondary;
            break;
        case ItemId::DawnBlade:
            m_baseStats.critBurstDamage += gain.primary;
            break;
        case ItemId::BloodDagger:
            m_baseStats.critHeal += gain.primary;
            break;
        case ItemId::Hourglass:
            m_baseStats.tickHeal += gain.primary;
            break;
        case ItemId::Metronome:
            m_baseStats.tickBoltDamage += gain.primary;
            break;
        case ItemId::GreedPact:
            m_baseStats.spendHeal += gain.primary;
            break;
        case ItemId::MerchantEye:
            m_baseStats.spendDamageGain += gain.primary;
            break;
        case ItemId::TomeOfGrowth:
            m_baseStats.levelHealRatio += gain.primary;
            break;
        case ItemId::StarlightPage:
            m_baseStats.levelPenFlat += gain.primary;
            break;
        case ItemId::BerserkSigil:
            m_baseStats.attackSpeedMul += gain.primary;
            m_baseStats.defense = std::max(0.0f, m_baseStats.defense + gain.secondary);   // 防御不穿成负数
            break;
        case ItemId::BloodPact:
            m_baseStats.maxHp += gain.primary;
            m_baseStats.killHeal += gain.secondary;
            break;
        case ItemId::CharWatchman:
            m_baseStats.maxHp += gain.primary;
            m_baseStats.defense += gain.secondary;
            break;
        case ItemId::CharHunter:
            m_baseStats.moveSpeed += gain.primary;   // 移速不设上限
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

bool SurvivorsLayer::CanAcquireItem(ItemId id) const
{
    const int index = static_cast<int>(id);
    switch (ItemCategoryOf(id)) {
        case ItemCategory::Character:
            return false;                                  // 角色只在开局选，不进商店
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
    // 关卡系数是分段二次的（前 5 关与原来的 +20%/关一致，之后加速），见 ShopPriceScale
    return static_cast<int>(std::lround(static_cast<float>(base) * ShopPriceScale(m_stage)));
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
    // 消费事件：刷新商店也算花钱（贪婪契约 / 商人之瞳靠它累计）
    RaiseEvent(EventId::SpendCoins, m_playerPosition, cost);

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
    // 消费事件：买道具的钱也算（与刷新同一条事件）
    RaiseEvent(EventId::SpendCoins, m_playerPosition, offer.price);

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
