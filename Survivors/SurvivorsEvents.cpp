// 《永夜幸存者》事件系统：击杀 / 拾取金币 / 受伤 / 过关四类「事件」，以及挂在事件上的道具响应。
//
// 设计：事件的发生处只调用一次 RaiseEvent()——
//   KillEnemy()        → EventId::Kill
//   UpdateCoins()      → EventId::CoinPickup（金币进钱包那一刻，金额 = 实际到手面额）
//   DamagePlayer()     → EventId::Hurt
//   AdvanceStage()     → EventId::StageClear
// 道具对这些事件的响应全部集中在本文件的 RaiseEvent 里。于是：
//   新增一种事件   = EventId 加一项 + 在发生处发一次 + 在 switch 里写响应；
//   新增一件事件道具 = ItemId / ITEM_DEFS 登记 + ItemGainFor / ApplyItemEffect 把数值写进
//                     BaseStats（见 SurvivorsItems.cpp）+ 在这里读它。
//
// 阈值类道具（每 N 次触发）共用一份「按事件归类」的进度计数器，且**只在持有关联道具时累积**：
// 买道具之前的击杀 / 金币不会被白攒，暂停面板里的「N / 阈值」就是真实进度。
#include "SurvivorsGame.h"
#include "SurvivorsInternal.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <string>
#include <vector>

using namespace OiEngine;
using namespace OiEngine::Survivors;

void SurvivorsLayer::ResetEvents()
{
    m_eventStats = {};
    m_eventKillProgress = 0;
    m_eventCoinProgress = 0;
    m_eventHurtProgress = 0;
    m_eventDepth = 0;
}

// ============================ 事件分发 ============================

void SurvivorsLayer::RaiseEvent(EventId id, const glm::vec2& position, int amount)
{
    ++m_eventDepth;
    // 事件可以互相引发（哀嚎钟的音波又击杀敌人 → 又一次击杀事件）：
    // 超过连锁上限后只记账（计数不能丢），不再引爆新的范围伤害，避免递归清屏
    const bool chained = m_eventDepth > MAX_EVENT_CHAIN;

    switch (id) {
        case EventId::Kill: {
            ++m_eventStats.kills;

            // 噬魂匕：每次击杀回血（没有阈值，任何深度都生效）
            if (m_baseStats.killHeal > 0.0f) {
                m_playerHp = std::min(m_playerMaxHp, m_playerHp + m_baseStats.killHeal);
            }

            // 哀嚎钟：每 KILL_EVENT_STEP 次击杀响一次（while 兜住一次事件跨多个阈值的情况）
            if (m_baseStats.killBurstDamage > 0.0f) {
                m_eventKillProgress += std::max(1, amount);
                while (m_eventKillProgress >= KILL_EVENT_STEP) {
                    m_eventKillProgress -= KILL_EVENT_STEP;
                    if (!chained) {
                        SpawnKillBurst(position);
                    }
                }
            }
            break;
        }
        case EventId::CoinPickup: {
            const int value = std::max(1, amount);
            ++m_eventStats.coinPickups;
            m_eventStats.coinValue += value;

            // 贪欲之链：每次拾取都向四周震出金币碎片（没有阈值，伤害按金币面额算）
            if (m_baseStats.coinBurstRatio > 0.0f) {
                if (!chained) {
                    SpawnCoinBurst(position, value);
                }
            }

            // 铸币者之锤 / 拾荒护符：按面额累计，每 COIN_EVENT_STEP 枚触发一次
            if (m_baseStats.coinEventBonus > 0 || m_baseStats.coinHeal > 0.0f) {
                m_eventCoinProgress += value;
                while (m_eventCoinProgress >= COIN_EVENT_STEP) {
                    m_eventCoinProgress -= COIN_EVENT_STEP;
                    if (chained) {
                        continue;
                    }
                    // 铸币者之锤：额外铸出的金币**直接进钱包**（不再生成掉落物，
                    // 否则「铸币 → 又被拾取 → 再铸币」会无限自我触发）
                    if (m_baseStats.coinEventBonus > 0) {
                        const int bonus = CoinGain(m_baseStats.coinEventBonus);
                        m_coins += bonus;
                        m_coinsEarned += bonus;
                        SpawnCoinNumber(position, bonus);
                        SpawnParticles(position, 3, C_GOLD, 150.0f, 5.0f, 0.32f);
                    }
                    // 拾荒护符：回血
                    if (m_baseStats.coinHeal > 0.0f) {
                        m_playerHp = std::min(m_playerMaxHp, m_playerHp + m_baseStats.coinHeal);
                        SpawnParticles(position, 3, glm::vec4(0.60f, 0.96f, 0.72f, 1.0f),
                                       120.0f, 4.0f, 0.30f);
                    }
                }
            }
            break;
        }
        case EventId::Hurt: {
            ++m_eventStats.hurts;

            // 受难者圣痕：每 HURT_EVENT_STEP 次受伤释放一次暗焰（伤害 + 回复）
            if (m_baseStats.hurtBurstDamage > 0.0f || m_baseStats.hurtBurstHeal > 0.0f) {
                m_eventHurtProgress += std::max(1, amount);
                while (m_eventHurtProgress >= HURT_EVENT_STEP) {
                    m_eventHurtProgress -= HURT_EVENT_STEP;
                    if (!chained && m_baseStats.hurtBurstDamage > 0.0f) {
                        SpawnHurtBurst(position);
                    }
                }
            }
            break;
        }
        case EventId::StageClear: {
            ++m_eventStats.stageClears;

            // 黎明誓约：每过一关永久成长，并回复一次生命
            if (m_baseStats.stageDamageGain > 0.0f) {
                m_baseStats.damageMul += m_baseStats.stageDamageGain;
            }
            if (m_baseStats.stageMaxHpGain > 0.0f) {
                m_baseStats.maxHp += m_baseStats.stageMaxHpGain;
                RecalculateStats();
            }
            if (m_baseStats.stageHealRatio > 0.0f) {
                m_playerHp = std::min(m_playerMaxHp,
                                      m_playerHp + m_playerMaxHp * m_baseStats.stageHealRatio);
                SpawnRing(position, 200.0f, 0.55f, C_GOLD);
                SpawnParticles(position, 16, C_GOLD, 260.0f, 7.0f, 0.55f);
            }
            break;
        }
        default:
            break;
    }

    --m_eventDepth;
}

// ============================ 事件爆发 ============================

// 事件造成的伤害统一在这里算：乘玩家的伤害加成（和武器同一条成长线），不吃暴击
void SurvivorsLayer::SpawnKillBurst(const glm::vec2& position)
{
    const float damage = m_baseStats.killBurstDamage * DamageMul();

    SpawnRing(position, BELL_RADIUS, 0.45f, C_BELL);
    SpawnParticles(position, 10, C_BELL, 300.0f, 7.0f, 0.45f);
    PlaySfx(SfxId::Storm, 0.45f, 1.62f);

    m_grid.ForEachNear(position, BELL_RADIUS, [&](uint32_t index) {
        if (index >= m_enemies.size()) {
            return;
        }
        Enemy& enemy = m_enemies[index];
        if (!enemy.alive || !CircleHit(enemy.position, enemy.radius, position, BELL_RADIUS)) {
            return;
        }
        DamageEnemy(enemy, damage, false);
    });
}

void SurvivorsLayer::SpawnCoinBurst(const glm::vec2& position, int coinValue)
{
    const float damage = std::max(1.0f, static_cast<float>(coinValue) * m_baseStats.coinBurstRatio) *
                         DamageMul();

    SpawnRing(position, GREED_CHAIN_RADIUS, 0.32f, C_GOLD);
    SpawnParticles(position, 6, COIN_COLORS[std::clamp(coinValue <= 2 ? 0 : (coinValue <= 8 ? 1 : 2),
                                                        0, 2)],
                   260.0f, 6.0f, 0.35f);
    m_shake = std::max(m_shake, 2.0f);

    m_grid.ForEachNear(position, GREED_CHAIN_RADIUS, [&](uint32_t index) {
        if (index >= m_enemies.size()) {
            return;
        }
        Enemy& enemy = m_enemies[index];
        if (!enemy.alive || !CircleHit(enemy.position, enemy.radius, position, GREED_CHAIN_RADIUS)) {
            return;
        }
        DamageEnemy(enemy, damage, false);
    });
}

void SurvivorsLayer::SpawnHurtBurst(const glm::vec2& position)
{
    const float damage = m_baseStats.hurtBurstDamage * DamageMul();

    SpawnRing(position, STIGMA_RADIUS, 0.40f, C_STIGMA);
    SpawnParticles(position, 12, C_STIGMA, 320.0f, 8.0f, 0.5f);
    m_shake = std::max(m_shake, 6.0f);

    m_grid.ForEachNear(position, STIGMA_RADIUS, [&](uint32_t index) {
        if (index >= m_enemies.size()) {
            return;
        }
        Enemy& enemy = m_enemies[index];
        if (!enemy.alive || !CircleHit(enemy.position, enemy.radius, position, STIGMA_RADIUS)) {
            return;
        }
        DamageEnemy(enemy, damage, false);
    });

    // 暗焰同时把伤口烧合：按比例回复最大生命
    if (m_baseStats.hurtBurstHeal > 0.0f) {
        m_playerHp = std::min(m_playerMaxHp, m_playerHp + m_playerMaxHp * m_baseStats.hurtBurstHeal);
        SpawnParticles(position, 6, glm::vec4(0.62f, 0.96f, 0.74f, 1.0f), 140.0f, 5.0f, 0.35f);
    }
}

// ============================ 暂停面板：事件一览 ============================

std::vector<std::string> SurvivorsLayer::EventSummary() const
{
    std::vector<std::string> lines;
    lines.reserve(4);

    std::string killLine = std::format("击杀 ×{}", m_eventStats.kills);
    if (m_baseStats.killBurstDamage > 0.0f) {
        killLine += std::format(" · 哀嚎钟 {}/{}", m_eventKillProgress, KILL_EVENT_STEP);
    }
    lines.push_back(std::move(killLine));

    std::string coinLine = std::format("拾取金币 {} 枚（{} 次）", m_eventStats.coinValue,
                                       m_eventStats.coinPickups);
    std::string coinTag;
    if (m_baseStats.coinEventBonus > 0) {
        coinTag = "铸币者之锤";
    }
    if (m_baseStats.coinHeal > 0.0f) {
        coinTag += coinTag.empty() ? "拾荒护符" : " / 拾荒护符";
    }
    if (!coinTag.empty()) {
        coinLine += std::format(" · {} {}/{}", coinTag, m_eventCoinProgress, COIN_EVENT_STEP);
    }
    lines.push_back(std::move(coinLine));

    std::string hurtLine = std::format("受伤 ×{}", m_eventStats.hurts);
    if (m_baseStats.hurtBurstDamage > 0.0f) {
        hurtLine += std::format(" · 受难者圣痕 {}/{}", m_eventHurtProgress, HURT_EVENT_STEP);
    }
    lines.push_back(std::move(hurtLine));

    lines.push_back(std::format("过关 ×{}", m_eventStats.stageClears));
    return lines;
}
