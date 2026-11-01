// 《永夜幸存者》道具系统：道具目录、持有上限、效果结算、商店（掷货 / 整组刷新 / 购买），
// 以及开局（选角色 + 武器）与每 5 级的武器选择。
//
// 道具有两条独立的属性：
//   种类决定**能不能重复拿**（普通无上限 / 限制类有上限 / 角色开局选），
//   稀有度是**目录里写死的档位**（同一件道具永远同一个稀有度，不会出现「铁片-普通 / 铁片-高级」），
//   它只决定展示色、商店价格与出货权重——效果就是 ITEM_DEFS 里 gain 那组数，不再按稀有度放大。
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
//   6 铁钉 7 猎手斗篷
//   8 猩红吊坠 9 秘银指环 10 流光冠冕
//   11 铸币者之锤 12 血月徽记 13 疾风羽 14 龙鳞 15 迷雾灯 16 哀嚎钟 17 拾荒护符 18 受难者圣痕
//   19 寒铁护腕 20 血玫瑰 21 断刃 22 诅咒之环 23 星屑
//   24 破晓之刃 25 淬血匕首 26 沙漏 27 节拍器 28 贪婪契约 29 商人之瞳 30 成长之书 31 星辉残页
//   32 不朽之心 33 荆棘之甲 34 贪婪之匣 35 猎月瞳 36 贪欲之链 37 黎明誓约 38 狂战徽记 39 血契
//   40 守夜人 41 猎手 42 修女 43 术士
// 目录是 unordered_map<ItemId, ItemDef>：键就是 ItemId，不再依赖枚举顺序。
// 一行就是一个道具的**全部数据**：名字 / 种类（决定能否重复拿）/ 固定稀有度 / 持有上限（0 = 无上限）/
// gain[5] 加成分量（主 / 次 / 三 / 四 / 五，没用到的留 0；每格含义随道具写在行尾注释里）。
// 新增道具 = ItemId 加一项 + 这里加一行 + DescribeItem / ApplyItemEffect 各加一个 case；
// 删除道具 = 两边一起删（已移除：圣杯、暗影长袍）。
// 表是静态初始化的，所以这里只能用常量表达式：移速类按「开局裸移速 240」的百分比给死值（BARE_MOVE_SPEED）
constexpr float BARE_MOVE_SPEED = 240.0f;

const std::unordered_map<SurvivorsLayer::ItemId, SurvivorsLayer::ItemDef> SurvivorsLayer::ITEM_DEFS = {
    // 普通类：无持有上限
    { ItemId::BloodBag,       { "血袋",       ItemCategory::Normal,    Rarity::Common,   0, { 0.30f } } },   // 立即回复比例
    { ItemId::Whetstone,      { "磨刀石",     ItemCategory::Normal,    Rarity::Common,   0, { 5.0f } } },    // 固定穿甲
    { ItemId::IronPlate,      { "铁片",       ItemCategory::Normal,    Rarity::Advanced, 0, { 5.0f } } },    // 防御
    { ItemId::HunterMark,     { "猎魂纹",     ItemCategory::Normal,    Rarity::Advanced, 0, { 0.07f } } },   // 暴击率
    { ItemId::StarBoots,      { "追星靴",     ItemCategory::Normal,    Rarity::Advanced, 0,
                                { BARE_MOVE_SPEED * 0.06f } } },                                             // 移速
    { ItemId::SoulLamp,       { "磁石",       ItemCategory::Normal,    Rarity::Common,   0, { 35.0f } } },   // 拾取范围
    { ItemId::IronNail,       { "铁钉",       ItemCategory::Normal,    Rarity::Common,   0, { 2.0f, 0.015f } } },  // 穿甲 / 暴击率
    { ItemId::HunterCloak,    { "猎手斗篷",   ItemCategory::Normal,    Rarity::Advanced, 0, { 16.0f, 0.03f } } },  // 移速 / 暴击率
    { ItemId::CrimsonPendant, { "猩红吊坠",   ItemCategory::Normal,    Rarity::Rare,     0, { 0.05f, 0.15f } } },  // 暴击率 / 暴击伤害
    { ItemId::MithrilRing,    { "秘银指环",   ItemCategory::Normal,    Rarity::Rare,     0, { 7.0f, 0.40f } } },   // 防御 / 每秒回复
    { ItemId::LumenCrown,     { "流光冠冕",   ItemCategory::Normal,    Rarity::Epic,     0, { 0.10f, 0.20f } } },  // 攻速 / 暴击伤害
    // 限制类：有持有上限
    { ItemId::CoinHammer,     { "铸币者之锤", ItemCategory::Limited,   Rarity::Common,   3, { 1.0f } } },    // 每 10 枚金币额外铸出
    { ItemId::BloodSigil,     { "血月徽记",   ItemCategory::Limited,   Rarity::Rare,     2, { 0.12f } } },   // 伤害加成
    { ItemId::GaleFeather,    { "疾风羽",     ItemCategory::Limited,   Rarity::Rare,     3, { 0.10f } } },   // 攻速加成
    { ItemId::DragonScale,    { "龙鳞",       ItemCategory::Limited,   Rarity::Rare,     2, { 9.0f, 6.0f } } },  // 防御 / 生命上限
    { ItemId::MistLantern,    { "迷雾灯",     ItemCategory::Limited,   Rarity::Common,   3, { 40.0f, 0.10f } } },  // 拾取范围 / 金币收益
    { ItemId::WailingBell,    { "哀嚎钟",     ItemCategory::Limited,   Rarity::Rare,     2, { 18.0f } } },   // 每 12 杀的音波伤害
    { ItemId::ScavengerCharm, { "拾荒护符",   ItemCategory::Limited,   Rarity::Advanced, 3, { 0.5f } } },    // 每 10 枚金币回复
    { ItemId::MartyrStigma,   { "受难者圣痕", ItemCategory::Limited,   Rarity::Rare,     2, { 24.0f, 0.06f } } },  // 每 5 次受伤暗焰伤害 / 回复比例
    { ItemId::ColdBracer,     { "寒铁护腕",   ItemCategory::Limited,   Rarity::Advanced, 2, { 6.0f, 0.05f } } },   // 防御 / 攻速
    { ItemId::BloodRose,      { "血玫瑰",     ItemCategory::Limited,   Rarity::Advanced, 2, { 14.0f, 0.50f } } },  // 生命上限 / 每秒回复
    { ItemId::BrokenBlade,    { "断刃",       ItemCategory::Limited,   Rarity::Rare,     1, { 0.12f, 0.08f } } },  // 伤害 / 攻速
    { ItemId::CursedRing,     { "诅咒之环",   ItemCategory::Limited,   Rarity::Rare,     1, { 0.30f, -12.0f } } }, // 伤害 / 生命上限（负面）
    { ItemId::StarShard,      { "星屑",       ItemCategory::Limited,   Rarity::Rare,     3, { 0.05f, 0.25f } } },  // 暴击率 / 暴击伤害
    { ItemId::DawnBlade,      { "破晓之刃",   ItemCategory::Limited,   Rarity::Rare,     1, { 25.0f } } },   // 每 8 次暴击的爆发伤害
    { ItemId::BloodDagger,    { "淬血匕首",   ItemCategory::Limited,   Rarity::Advanced, 2, { 0.12f } } },   // 每次暴击回复生命
    { ItemId::Hourglass,      { "沙漏",       ItemCategory::Limited,   Rarity::Common,   2, { 3.0f } } },    // 每 8 秒回复生命
    { ItemId::Metronome,      { "节拍器",     ItemCategory::Limited,   Rarity::Rare,     1, { 18.0f } } },   // 每 8 秒对最近敌人的伤害
    { ItemId::GreedPact,      { "贪婪契约",   ItemCategory::Limited,   Rarity::Advanced, 2, { 4.0f } } },    // 每花 50 币回复生命
    { ItemId::MerchantEye,    { "商人之瞳",   ItemCategory::Limited,   Rarity::Epic,     1, { 0.01f } } },   // 每花 50 币永久 +伤害
    { ItemId::TomeOfGrowth,   { "成长之书",   ItemCategory::Limited,   Rarity::Common,   2, { 0.15f } } },   // 每次升级回复比例
    { ItemId::StarlightPage,  { "星辉残页",   ItemCategory::Limited,   Rarity::Rare,     1, { 1.0f } } },    // 每次升级 +固定穿甲
    // 限制类（史诗档）：原「独特类」并入这里，每件上限 1
    { ItemId::UndyingHeart,   { "不朽之心",   ItemCategory::Limited,   Rarity::Epic,     1, { REVIVE_HP_FRACTION } } },  // 复活时回血比例
    { ItemId::ThornMail,      { "荆棘之甲",   ItemCategory::Limited,   Rarity::Epic,     1, { 1.20f } } },   // 反弹比例
    { ItemId::GreedCoffin,    { "贪婪之匣",   ItemCategory::Limited,   Rarity::Epic,     1, { 1.00f } } },   // 金币收益加成
    { ItemId::MoonshotPupil,  { "猎月瞳",     ItemCategory::Limited,   Rarity::Epic,     1, { 0.20f, 0.60f } } },  // 暴击率 / 暴击伤害
    { ItemId::GreedChain,     { "贪欲之链",   ItemCategory::Limited,   Rarity::Epic,     1, { 4.0f } } },    // 每枚金币面额的伤害倍率
    { ItemId::DawnPact,       { "黎明誓约",   ItemCategory::Limited,   Rarity::Epic,     1, { 0.04f, 0.15f } } },  // 每关永久 +伤害 / 回复比例
    { ItemId::BerserkSigil,   { "狂战徽记",   ItemCategory::Limited,   Rarity::Epic,     1, { 0.30f, -8.0f } } }, // 攻速 / 防御（负面）
    { ItemId::BloodPact,      { "血契",       ItemCategory::Limited,   Rarity::Epic,     1, { 30.0f, 0.40f } } },  // 生命上限 / 击杀回血
    // 角色类：固定史诗，开局选择（gain 是开局的初始属性）
    { ItemId::CharWatchman,   { "守夜人",     ItemCategory::Character, Rarity::Epic,     1, { 24.0f, 12.0f } } },  // 生命上限 / 防御
    { ItemId::CharHunter,     { "猎手",       ItemCategory::Character, Rarity::Epic,     1,
                                { BARE_MOVE_SPEED * 0.10f, 0.08f } } },                                      // 移速 / 暴击率
    { ItemId::CharSister,     { "修女",       ItemCategory::Character, Rarity::Epic,     1, { 0.70f, 10.0f } } },  // 每秒回复 / 生命上限
    { ItemId::CharWarlock,    { "术士",       ItemCategory::Character, Rarity::Epic,     1, { 0.25f } } },   // 伤害加成
};

// ============================ 道具目录查询 ============================

// 目录条目的唯一入口：漏登记会在 at() 处直接抛出来，不会静默错位
const SurvivorsLayer::ItemDef& SurvivorsLayer::ItemDefOf(ItemId id)
{
    return ITEM_DEFS.at(id);
}

const char* SurvivorsLayer::ItemName(ItemId id)
{
    return ItemDefOf(id).name;
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
    return ItemDefOf(id).category;
}

SurvivorsLayer::Rarity SurvivorsLayer::ItemRarity(ItemId id)
{
    return ItemDefOf(id).rarity;
}

int SurvivorsLayer::ItemCap(ItemId id)
{
    return ItemDefOf(id).cap;
}

// ============================ 道具数值 ============================
// 数值本身全部在 ITEM_DEFS 的 gain[5] 里（每格含义见各行注释），这里只是读取与结算。
// 描述与结算读的是同一份数据，不会出现「卡片写 A、实际给 B」

void SurvivorsLayer::DescribeItem(ItemId id, std::string& title, std::string& description)
{
    title = ItemName(id);
    const float* g = ItemDefOf(id).gain;   // 数值全在目录里，见 ITEM_DEFS 各行注释

    switch (id) {
        case ItemId::BloodBag:
            description = std::format("立即回复 {:.0f}% 最大生命。", std::min(1.0f, g[0]) * 100.0f);
            break;
        case ItemId::Whetstone:
            description = std::format("固定穿甲 +{:.0f}（无视对方这么多点防御）。", g[0]);
            break;
        case ItemId::IronPlate:
            description = std::format("防御 +{:.0f}（减伤 = 防御/(防御+100)）。", g[0]);
            break;
        case ItemId::HunterMark:
            description = std::format("暴击率 +{:.0f}%（暴击双倍伤害，上限 100%）。", g[0] * 100.0f);
            break;
        case ItemId::StarBoots:
            description = std::format("移动速度 +{:.0f}（没有上限）。", g[0]);
            break;
        case ItemId::SoulLamp:
            description = std::format("金币拾取范围 +{:.0f}。", g[0]);
            break;
        case ItemId::BloodSigil:
            description = std::format("全部伤害 +{:.0f}%。", g[0] * 100.0f);
            break;
        case ItemId::GaleFeather:
            description = std::format("攻击速度 +{:.0f}%（所有武器冷却缩短）。", g[0] * 100.0f);
            break;
        case ItemId::DragonScale:
            description = std::format("防御 +{:.0f} · 生命上限 +{:.0f}。", g[0], g[1]);
            break;
        case ItemId::UndyingHeart:
            description = std::format("阵亡时以 {:.0f}% 生命复活一次（只能持有一件）。",
                                      g[0] * 100.0f);
            break;
        case ItemId::ThornMail:
            description = std::format("受到伤害时，对周围 {:.0f} 范围内的敌人反弹 {:.0f}% 伤害。",
                                      THORN_RADIUS, g[0] * 100.0f);
            break;
        case ItemId::GreedCoffin:
            description = std::format("金币收益 +{:.0f}%。", g[0] * 100.0f);
            break;
        case ItemId::MoonshotPupil:
            description = std::format("暴击率 +{:.0f}% · 暴击伤害 +{:.0f}%。",
                                      g[0] * 100.0f, g[1] * 100.0f);
            break;
        case ItemId::WailingBell:
            description = std::format("【击杀事件】每击杀 {} 个敌人，向周围 {:.0f} 范围发出哀嚎，造成 {:.0f} 点伤害。",
                                      KILL_EVENT_STEP, BELL_RADIUS, g[0]);
            break;
        case ItemId::CoinHammer:
            description = std::format("【拾金事件】每累计拾取 {} 枚金币，额外铸出 {:.0f} 枚金币（直接进钱包）。",
                                      COIN_EVENT_STEP, g[0]);
            break;
        case ItemId::ScavengerCharm:
            // 单件只有 0.5 点，整数格式会显示成「回复 0 点生命」，这里保留一位小数
            description = std::format("【拾金事件】每累计拾取 {} 枚金币，回复 {:.1f} 点生命。",
                                      COIN_EVENT_STEP, g[0]);
            break;
        case ItemId::GreedChain:
            description = std::format("【拾金事件】每次拾取金币，向周围 {:.0f} 范围震出金币碎片：伤害 = 金币面额 × {:.0f}。",
                                      GREED_CHAIN_RADIUS, g[0]);
            break;
        case ItemId::MartyrStigma:
            description = std::format("【受伤事件】每受到 {} 次伤害，向周围 {:.0f} 范围释放暗焰：伤害 {:.0f} · 回复 {:.0f}% 最大生命。",
                                      HURT_EVENT_STEP, STIGMA_RADIUS, g[0], g[1] * 100.0f);
            break;
        case ItemId::DawnPact:
            description = std::format("【过关事件】每通过一关永久获得 +{:.0f}% 伤害、+{:.0f} 生命上限，并回复 {:.0f}% 生命。",
                                      g[0] * 100.0f, DAWN_PACT_MAXHP, g[1] * 100.0f);
            break;
        case ItemId::IronNail:
            description = std::format("固定穿甲 +{:.0f} · 暴击率 +{:.1f}%。", g[0], g[1] * 100.0f);
            break;
        case ItemId::HunterCloak:
            description = std::format("移动速度 +{:.0f} · 暴击率 +{:.0f}%。", g[0], g[1] * 100.0f);
            break;
        case ItemId::MistLantern:
            description = std::format("金币拾取范围 +{:.0f} · 金币收益 +{:.0f}%。", g[0], g[1] * 100.0f);
            break;
        case ItemId::CrimsonPendant:
            description = std::format("暴击率 +{:.1f}% · 暴击伤害 +{:.0f}%。",
                                      g[0] * 100.0f, g[1] * 100.0f);
            break;
        case ItemId::MithrilRing:
            description = std::format("防御 +{:.0f}（减伤 = 防御/(防御+100)）· 每秒回复 +{:.2f}。",
                                      g[0], g[1]);
            break;
        case ItemId::LumenCrown:
            description = std::format("攻击速度 +{:.0f}% · 暴击伤害 +{:.0f}%。",
                                      g[0] * 100.0f, g[1] * 100.0f);
            break;
        case ItemId::ColdBracer:
            description = std::format("防御 +{:.0f} · 攻击速度 +{:.0f}%。", g[0], g[1] * 100.0f);
            break;
        case ItemId::BloodRose:
            description = std::format("生命上限 +{:.0f} · 每秒回复 +{:.2f}。", g[0], g[1]);
            break;
        case ItemId::BrokenBlade:
            description = std::format("全部伤害 +{:.0f}% · 攻击速度 +{:.0f}%。",
                                      g[0] * 100.0f, g[1] * 100.0f);
            break;
        case ItemId::CursedRing:
            description = std::format("全部伤害 +{:.0f}%，代价是生命上限 −{:.0f}。",
                                      g[0] * 100.0f, -g[1]);
            break;
        case ItemId::StarShard:
            description = std::format("暴击率 +{:.0f}% · 暴击伤害 +{:.0f}%。",
                                      g[0] * 100.0f, g[1] * 100.0f);
            break;
        case ItemId::DawnBlade:
            description = std::format("【暴击事件】每累计 {} 次暴击，向周围 {:.0f} 范围释放一次光爆，造成 {:.0f} 点伤害。",
                                      CRIT_EVENT_STEP, CRIT_BURST_RADIUS, g[0]);
            break;
        case ItemId::BloodDagger:
            description = std::format("【暴击事件】每次暴击回复 {:.2f} 点生命。", g[0]);
            break;
        case ItemId::Hourglass:
            description = std::format("【计时事件】每 {:.0f} 秒回复 {:.0f} 点生命。",
                                      TICK_EVENT_SECONDS, g[0]);
            break;
        case ItemId::Metronome:
            description = std::format("【计时事件】每 {:.0f} 秒向最近的敌人打出一击，伤害 {:.0f}。",
                                      TICK_EVENT_SECONDS, g[0]);
            break;
        case ItemId::GreedPact:
            description = std::format("【消费事件】每累计花费 {} 金币，回复 {:.0f} 点生命。",
                                      SPEND_EVENT_STEP, g[0]);
            break;
        case ItemId::MerchantEye:
            description = std::format("【消费事件】每累计花费 {} 金币，永久 +{:.0f}% 伤害。",
                                      SPEND_EVENT_STEP, g[0] * 100.0f);
            break;
        case ItemId::TomeOfGrowth:
            description = std::format("【升级事件】每次升级回复 {:.0f}% 最大生命。", g[0] * 100.0f);
            break;
        case ItemId::StarlightPage:
            description = std::format("【升级事件】每次升级永久 +{:.0f} 点固定穿甲。", g[0]);
            break;
        case ItemId::BerserkSigil:
            description = std::format("攻击速度 +{:.0f}%，代价是防御 −{:.0f}。",
                                      g[0] * 100.0f, -g[1]);
            break;
        case ItemId::BloodPact:
            description = std::format("生命上限 +{:.0f} · 【击杀事件】每次击杀回复 {:.2f} 生命。",
                                      g[0], g[1]);
            break;
        case ItemId::CharWatchman:
            description = std::format("生命上限 +{:.0f} · 防御 +{:.0f}。\n扛得住，适合正面硬顶。",
                                      g[0], g[1]);
            break;
        case ItemId::CharHunter:
            description = std::format("移动速度 +{:.0f} · 暴击率 +{:.0f}%。\n绕圈风筝，靠暴击输出。",
                                      g[0], g[1] * 100.0f);
            break;
        case ItemId::CharSister:
            description = std::format("每秒回复 +{:.2f} · 生命上限 +{:.0f}。\n续航最稳，容错最高。",
                                      g[0], g[1]);
            break;
        case ItemId::CharWarlock:
            description = std::format("全部伤害 +{:.0f}%。\n输出最高，但没有任何防御加成。",
                                      g[0] * 100.0f);
            break;
        default:
            description = "（未知道具）";
            break;
    }
}

// 结算：属性类直接写进 m_baseStats（和属性卡同样的线性叠加），行为类（复活 / 荆棘 / 金币）落成标志位
void SurvivorsLayer::ApplyItemEffect(ItemId id)
{
    const float* g = ItemDefOf(id).gain;   // 数值全在目录里，见 ITEM_DEFS 各行注释
    switch (id) {
        case ItemId::BloodBag:
            m_playerHp = std::min(m_playerMaxHp, m_playerHp + m_playerMaxHp * std::min(1.0f, g[0]));
            break;
        case ItemId::Whetstone:
            m_baseStats.penFlat += g[0];
            break;
        case ItemId::IronPlate:
        case ItemId::DragonScale:
            m_baseStats.defense += g[0];
            m_baseStats.maxHp += g[1];
            break;
        case ItemId::HunterMark:
            m_baseStats.critChance = std::min(CRIT_CHANCE_CAP, m_baseStats.critChance + g[0]);
            break;
        case ItemId::StarBoots:
            m_baseStats.moveSpeed += g[0];   // 移速不设上限
            break;
        case ItemId::SoulLamp:
            m_baseStats.pickupRadius += g[0];
            break;
        case ItemId::BloodSigil:
            m_baseStats.damageMul += g[0];
            break;
        case ItemId::GaleFeather:
            m_baseStats.attackSpeedMul += g[0];
            break;
        case ItemId::UndyingHeart:
            ++m_baseStats.reviveCharges;
            break;
        case ItemId::ThornMail:
            m_baseStats.thornRatio += g[0];
            break;
        case ItemId::GreedCoffin:
            m_baseStats.coinGainMul += g[0];
            break;
        case ItemId::MoonshotPupil:
            m_baseStats.critChance = std::min(CRIT_CHANCE_CAP, m_baseStats.critChance + g[0]);
            m_baseStats.critDamageMul += g[1];
            break;
        // 事件类：只把数值写进 BaseStats，真正的效果在 SurvivorsEvents.cpp 的 RaiseEvent 里读
        case ItemId::WailingBell:
            m_baseStats.killBurstDamage += g[0];
            break;
        case ItemId::CoinHammer:
            m_baseStats.coinEventBonus += static_cast<int>(std::lround(g[0]));
            break;
        case ItemId::ScavengerCharm:
            m_baseStats.coinHeal += g[0];
            break;
        case ItemId::GreedChain:
            m_baseStats.coinBurstRatio += g[0];
            break;
        case ItemId::MartyrStigma:
            m_baseStats.hurtBurstDamage += g[0];
            m_baseStats.hurtBurstHeal += g[1];
            break;
        case ItemId::DawnPact:
            m_baseStats.stageDamageGain += g[0];
            m_baseStats.stageMaxHpGain += DAWN_PACT_MAXHP;
            m_baseStats.stageHealRatio += g[1];
            break;
        case ItemId::IronNail:
            m_baseStats.penFlat += g[0];
            m_baseStats.critChance = std::min(CRIT_CHANCE_CAP, m_baseStats.critChance + g[1]);
            break;
        case ItemId::HunterCloak:
            m_baseStats.moveSpeed += g[0];
            m_baseStats.critChance = std::min(CRIT_CHANCE_CAP, m_baseStats.critChance + g[1]);
            break;
        case ItemId::MistLantern:
            m_baseStats.pickupRadius += g[0];
            m_baseStats.coinGainMul += g[1];
            break;
        case ItemId::CrimsonPendant:
            m_baseStats.critChance = std::min(CRIT_CHANCE_CAP, m_baseStats.critChance + g[0]);
            m_baseStats.critDamageMul += g[1];
            break;
        case ItemId::MithrilRing:
            m_baseStats.defense += g[0];
            m_baseStats.regenPerSecond += g[1];
            break;
        case ItemId::LumenCrown:
            m_baseStats.attackSpeedMul += g[0];
            m_baseStats.critDamageMul += g[1];
            break;
        case ItemId::ColdBracer:
            m_baseStats.defense += g[0];
            m_baseStats.attackSpeedMul += g[1];
            break;
        case ItemId::BloodRose:
            m_baseStats.maxHp += g[0];
            m_baseStats.regenPerSecond += g[1];
            break;
        case ItemId::BrokenBlade:
            m_baseStats.damageMul += g[0];
            m_baseStats.attackSpeedMul += g[1];
            break;
        case ItemId::CursedRing:
            m_baseStats.damageMul += g[0];
            m_baseStats.maxHp = std::max(1.0f, m_baseStats.maxHp + g[1]);   // 生命上限不能被压到 0
            break;
        case ItemId::StarShard:
            m_baseStats.critChance = std::min(CRIT_CHANCE_CAP, m_baseStats.critChance + g[0]);
            m_baseStats.critDamageMul += g[1];
            break;
        case ItemId::DawnBlade:
            m_baseStats.critBurstDamage += g[0];
            break;
        case ItemId::BloodDagger:
            m_baseStats.critHeal += g[0];
            break;
        case ItemId::Hourglass:
            m_baseStats.tickHeal += g[0];
            break;
        case ItemId::Metronome:
            m_baseStats.tickBoltDamage += g[0];
            break;
        case ItemId::GreedPact:
            m_baseStats.spendHeal += g[0];
            break;
        case ItemId::MerchantEye:
            m_baseStats.spendDamageGain += g[0];
            break;
        case ItemId::TomeOfGrowth:
            m_baseStats.levelHealRatio += g[0];
            break;
        case ItemId::StarlightPage:
            m_baseStats.levelPenFlat += g[0];
            break;
        case ItemId::BerserkSigil:
            m_baseStats.attackSpeedMul += g[0];
            m_baseStats.defense = std::max(0.0f, m_baseStats.defense + g[1]);   // 防御不穿成负数
            break;
        case ItemId::BloodPact:
            m_baseStats.maxHp += g[0];
            m_baseStats.killHeal += g[1];
            break;
        case ItemId::CharWatchman:
            m_baseStats.maxHp += g[0];
            m_baseStats.defense += g[1];
            break;
        case ItemId::CharHunter:
            m_baseStats.moveSpeed += g[0];   // 移速不设上限
            m_baseStats.critChance = std::min(CRIT_CHANCE_CAP, m_baseStats.critChance + g[1]);
            break;
        case ItemId::CharSister:
            m_baseStats.regenPerSecond += g[0];
            m_baseStats.maxHp += g[1];
            break;
        case ItemId::CharWarlock:
            m_baseStats.damageMul += g[0];
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
    // (6 + 4 × 本次已刷新次数) × 关卡系数，系数 = 1 + 0.4 × (已过关数 − 1)：
    // 第一次开商店（刚过第 1 关，n = 1）系数正好 1.0，之后每过一关更贵一档。
    // n < 1 只在没进商店时出现（第 1 关不开商店），兜底钳到 1.0 免得出现打折
    const float clears = static_cast<float>(m_stage - 1);
    const float scale = std::max(1.0f, 1.0f + SHOP_REROLL_STAGE_STEP * (clears - 1.0f));
    const int base = SHOP_REROLL_BASE + SHOP_REROLL_STEP * m_shopRerolls;
    return static_cast<int>(std::lround(static_cast<float>(base) * scale));
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
    ApplyItemEffect(character);   // 角色固定史诗，数值就是 ITEM_DEFS 里 gain 那组
}

// ============================ 图标 ============================

// 三组图标按对应枚举的**声明顺序**排列。文件在 assets/textures/icons/，由
// Survivors/tools/prepare_sprites.py 从生图的图集切出来；缺文件的位置留 nullptr
// （角色类道具没有独立图标，UI 用立绘代替或不画）
void SurvivorsLayer::LoadItemIcons()
{
    static const char* ITEM_ICONS[] = {
        "assets/textures/icons/item_blood_bag.png",        // BloodBag
        "assets/textures/icons/item_whetstone.png",        // Whetstone
        "assets/textures/icons/item_iron_plate.png",       // IronPlate
        "assets/textures/icons/item_hunter_mark.png",      // HunterMark
        "assets/textures/icons/item_star_boots.png",       // StarBoots
        "assets/textures/icons/item_soul_lamp.png",        // SoulLamp
        "assets/textures/icons/item_iron_nail.png",        // IronNail
        "assets/textures/icons/item_hunter_cloak.png",     // HunterCloak
        "assets/textures/icons/item_crimson_pendant.png",  // CrimsonPendant
        "assets/textures/icons/item_mithril_ring.png",     // MithrilRing
        "assets/textures/icons/item_lumen_crown.png",      // LumenCrown
        "assets/textures/icons/item_coin_hammer.png",      // CoinHammer
        "assets/textures/icons/item_blood_sigil.png",      // BloodSigil
        "assets/textures/icons/item_gale_feather.png",     // GaleFeather
        "assets/textures/icons/item_dragon_scale.png",     // DragonScale
        "assets/textures/icons/item_mist_lantern.png",     // MistLantern
        "assets/textures/icons/item_wailing_bell.png",     // WailingBell
        "assets/textures/icons/item_scavenger_charm.png",  // ScavengerCharm
        "assets/textures/icons/item_martyr_stigma.png",    // MartyrStigma
        "assets/textures/icons/item_cold_bracer.png",      // ColdBracer
        "assets/textures/icons/item_blood_rose.png",       // BloodRose
        "assets/textures/icons/item_broken_blade.png",     // BrokenBlade
        "assets/textures/icons/item_cursed_ring.png",      // CursedRing
        "assets/textures/icons/item_star_shard.png",       // StarShard
        "assets/textures/icons/item_dawn_blade.png",       // DawnBlade
        "assets/textures/icons/item_blood_dagger.png",     // BloodDagger
        "assets/textures/icons/item_hourglass.png",        // Hourglass
        "assets/textures/icons/item_metronome.png",        // Metronome
        "assets/textures/icons/item_greed_pact.png",       // GreedPact
        "assets/textures/icons/item_merchant_eye.png",     // MerchantEye
        "assets/textures/icons/item_tome_of_growth.png",   // TomeOfGrowth
        "assets/textures/icons/item_starlight_page.png",   // StarlightPage
        "assets/textures/icons/item_undying_heart.png",    // UndyingHeart
        "assets/textures/icons/item_thorn_mail.png",       // ThornMail
        "assets/textures/icons/item_greed_coffin.png",     // GreedCoffin
        "assets/textures/icons/item_moonshot_pupil.png",   // MoonshotPupil
        "assets/textures/icons/item_greed_chain.png",      // GreedChain
        "assets/textures/icons/item_dawn_pact.png",        // DawnPact
        "assets/textures/icons/item_berserk_sigil.png",    // BerserkSigil
        "assets/textures/icons/item_blood_pact.png",       // BloodPact
        nullptr,                                            // CharWatchman（用角色立绘）
        nullptr,                                            // CharHunter
        nullptr,                                            // CharSister
        nullptr,                                            // CharWarlock
    };
    static const char* WEAPON_ICONS[] = {
        "assets/textures/icons/weapon_moon_blade.png",   // MoonBlade
        "assets/textures/icons/weapon_aura.png",         // Aura
        "assets/textures/icons/weapon_scythe.png",       // Scythe
        "assets/textures/icons/weapon_storm.png",        // Storm
    };
    static const char* PASSIVE_ICONS[] = {
        "assets/textures/icons/passive_might.png",       // Might
        "assets/textures/icons/passive_haste.png",       // Haste
        "assets/textures/icons/passive_swift.png",       // Swift
        "assets/textures/icons/passive_vitality.png",    // Vitality
        "assets/textures/icons/passive_crit.png",        // Crit
        "assets/textures/icons/passive_critdamage.png",  // CritDamage
        "assets/textures/icons/passive_regen.png",       // Regen
        "assets/textures/icons/passive_bulwark.png",     // Bulwark
        "assets/textures/icons/passive_sunder.png",      // Sunder
    };

    static_assert(sizeof(ITEM_ICONS) / sizeof(ITEM_ICONS[0]) == static_cast<size_t>(ItemId::Count),
                  "图标表必须和 ItemId 枚举一一对应");
    static_assert(sizeof(WEAPON_ICONS) / sizeof(WEAPON_ICONS[0]) == static_cast<size_t>(WeaponId::Count),
                  "图标表必须和 WeaponId 枚举一一对应");
    static_assert(sizeof(PASSIVE_ICONS) / sizeof(PASSIVE_ICONS[0]) == static_cast<size_t>(PassiveId::Count),
                  "图标表必须和 PassiveId 枚举一一对应");

    const std::vector<const char*> itemFiles(ITEM_ICONS, ITEM_ICONS + std::size(ITEM_ICONS));
    const std::vector<const char*> weaponFiles(WEAPON_ICONS, WEAPON_ICONS + std::size(WEAPON_ICONS));
    const std::vector<const char*> passiveFiles(PASSIVE_ICONS, PASSIVE_ICONS + std::size(PASSIVE_ICONS));
    m_art.LoadIcons(itemFiles, weaponFiles, passiveFiles);
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
    m_hurtCooldown = 2.0f;   // 开局短暂无敌，避免刚开局就被贴脸的敌人打一下

    // 开局不再预放蝙蝠：第一波敌人交给关卡刷怪节奏（UpdateSpawning），进场时屏幕是空的

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
