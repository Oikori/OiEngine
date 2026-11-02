// 《永夜幸存者》武器：索敌、四把武器（月刃 / 圣光环 / 回旋镰 / 雷暴）的开火与冷却、弹丸推进。
//
// 数值表见 SurvivorsInternal.h（BLADE / AURA / SCYTHE / STORM），下标是武器等级 1~5。
#include "SurvivorsGame.h"
#include "SurvivorsInternal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <format>
#include <limits>
#include <string>

using namespace OiEngine;
using namespace OiEngine::Survivors;

// ============================ 武器 ============================

SurvivorsLayer::Enemy* SurvivorsLayer::FindNearestEnemy(const glm::vec2& origin, float range)
{
    Enemy* best = nullptr;
    float bestDistanceSq = range * range;
    m_grid.ForEachNear(origin, range, [&](uint32_t index) {
        if (index >= m_enemies.size()) {
            return;
        }
        Enemy& enemy = m_enemies[index];
        if (!enemy.alive || InShadow(enemy.position)) {
            return;   // 阴影里的敌人打不到，索敌也跳过它，别把输出浪费在无敌目标上
        }
        const glm::vec2 offset = enemy.position - origin;
        const float distanceSq = offset.x * offset.x + offset.y * offset.y;
        if (distanceSq < bestDistanceSq) {
            bestDistanceSq = distanceSq;
            best = &enemy;
        }
    });
    return best;
}

bool SurvivorsLayer::FireMoonBlade()
{
    const int level = m_weaponLevel[static_cast<int>(WeaponId::MoonBlade)];
    // 索敌范围略小于画面短半轴：只打看得见的目标，敌人能真正围到身边来
    Enemy* target = FindNearestEnemy(m_playerPosition, m_halfHeight - 45.0f);
    if (target == nullptr) {
        return false;
    }

    const glm::vec2 direction = Normalize(target->position - m_playerPosition);
    const float baseAngle = std::atan2(direction.y, direction.x);
    const int count = BLADE.count[level];
    for (int i = 0; i < count; ++i) {
        const float offset = count > 1
            ? (static_cast<float>(i) - static_cast<float>(count - 1) * 0.5f) * 0.14f
            : 0.0f;
        const float angle = baseAngle + offset;
        const glm::vec2 shotDirection(std::cos(angle), std::sin(angle));

        Projectile projectile;
        projectile.kind = ShotKind::Blade;
        projectile.position = m_playerPosition + shotDirection * 26.0f;
        projectile.velocity = shotDirection * 880.0f;
        projectile.damage = BLADE.damage[level] * DamageMul() *
                            WeaponDamageMul(static_cast<int>(WeaponId::MoonBlade));   // 猎手：月刃 +20%
        projectile.radius = 16.0f;
        projectile.rotation = std::atan2(-shotDirection.x, shotDirection.y);
        projectile.spin = 14.0f;
        projectile.lifetime = 1.15f;
        projectile.pierce = BLADE.pierce[level];
        m_projectiles.push_back(projectile);
    }

    SpawnParticles(m_playerPosition + direction * 28.0f, 4, C_BLADE, 140.0f, 5.0f, 0.25f);
    m_shake = std::max(m_shake, 1.2f);
    PlaySfx(SfxId::Blade, 1.0f, Rand(0.96f, 1.06f));
    return true;
}

void SurvivorsLayer::PulseAura()
{
    const int level = m_weaponLevel[static_cast<int>(WeaponId::Aura)];
    const float radius = AURA.radius[level];
    const float damage = AURA.damage[level] * DamageMul() *
                         WeaponDamageMul(static_cast<int>(WeaponId::Aura));   // 修女：圣光环 +50%

    m_grid.ForEachNear(m_playerPosition, radius, [&](uint32_t index) {
        if (index >= m_enemies.size()) {
            return;
        }
        Enemy& enemy = m_enemies[index];
        if (!enemy.alive || !CircleHit(enemy.position, enemy.radius, m_playerPosition, radius)) {
            return;
        }
        const bool crit = Rand(0.0f, 1.0f) < CritChance();
        DamageEnemy(enemy, damage * (crit ? CritMultiplier() : 1.0f), crit);
    });

    SpawnRing(m_playerPosition, radius, 0.34f, C_AURA);
    SpawnParticles(m_playerPosition, 3, C_AURA, 70.0f, 9.0f, 0.3f);
}

bool SurvivorsLayer::ThrowScythes()
{
    const int level = m_weaponLevel[static_cast<int>(WeaponId::Scythe)];
    const int count = SCYTHE.count[level];

    // 投掷方向：第一把**必定朝最近的敌人**掷出（索敌范围与月刃一致），其余几把以它为起点
    // 按 360°/count 等分圆抛出 —— 一把贴脸、其余覆盖四周，不会全砸在同一个方向上。
    // 场上一个目标都没有时（开局 / 清场）退化成随机朝向，照样满圆抛出，回旋镰不会因为没目标而停手。
    float baseAngle = Rand(0.0f, TAU);
    const Enemy* target = FindNearestEnemy(m_playerPosition, m_halfHeight - 45.0f);
    if (target != nullptr) {
        const glm::vec2 aimed = Normalize(target->position - m_playerPosition);
        baseAngle = std::atan2(aimed.y, aimed.x);
    }

    for (int i = 0; i < count; ++i) {
        const float angle = baseAngle + TAU * static_cast<float>(i) / static_cast<float>(count);
        const glm::vec2 direction(std::cos(angle), std::sin(angle));

        Projectile projectile;
        projectile.kind = ShotKind::Scythe;
        projectile.position = m_playerPosition + direction * 34.0f;
        projectile.velocity = direction * 520.0f;
        projectile.damage = SCYTHE.damage[level] * DamageMul() *
                            WeaponDamageMul(static_cast<int>(WeaponId::Scythe));   // 猎手：回旋镰 +20%
        projectile.radius = 30.0f;
        projectile.rotation = std::atan2(-direction.x, direction.y);
        projectile.lifetime = 4.0f;
        projectile.pierce = 99;
        m_projectiles.push_back(projectile);
    }
    PlaySfx(SfxId::Blade, 0.75f, 0.66f);   // 回旋镰复用切割声，压低变慢做成厚重的风啸
    return true;
}

bool SurvivorsLayer::CastStorm()
{
    const int level = m_weaponLevel[static_cast<int>(WeaponId::Storm)];
    // 落点候选：玩家周围 620 内的敌人，每道雷随机挑一只作为**落点**
    std::vector<uint32_t> targets;
    m_grid.ForEachNear(m_playerPosition, 620.0f, [&](uint32_t index) {
        if (index >= m_enemies.size()) {
            return;
        }
        const Enemy& enemy = m_enemies[index];
        // 阴影里的敌人不能当落点：否则整道雷砸在无敌目标上，旁边的怪也白吃不到伤害
        if (!enemy.alive || InShadow(enemy.position) || Length(enemy.position - m_playerPosition) > 620.0f) {
            return;
        }
        targets.push_back(index);
    });
    if (targets.empty()) {
        return false;
    }

    const int strikes = STORM.count[level];
    const float radius = STORM.radius[level];
    const float damage = STORM.damage[level] * DamageMul() *
                         WeaponDamageMul(static_cast<int>(WeaponId::Storm));
    // 默认**单体**：每一道只劈落点那一只（只有术士的雷暴带范围伤害）
    const bool areaDamage = m_character == ItemId::CharWarlock;

    for (int strike = 0; strike < strikes && !targets.empty(); ++strike) {
        size_t pick = 0;
        if (strike == 0) {
            // 第一道**必定**劈最近的敌人
            float bestDistanceSq = std::numeric_limits<float>::max();
            for (size_t i = 0; i < targets.size(); ++i) {
                const glm::vec2 offset = m_enemies[targets[i]].position - m_playerPosition;
                const float distanceSq = offset.x * offset.x + offset.y * offset.y;
                if (distanceSq < bestDistanceSq) {
                    bestDistanceSq = distanceSq;
                    pick = i;
                }
            }
        } else if (strike == 1) {
            // 第二道**优先劈 Boss**（血月使徒）：场上有使徒就锁最近的那只，没有就退回随机
            pick = static_cast<size_t>(RandInt(0, static_cast<int>(targets.size()) - 1));
            float bestDistanceSq = std::numeric_limits<float>::max();
            for (size_t i = 0; i < targets.size(); ++i) {
                const Enemy& enemy = m_enemies[targets[i]];
                if (enemy.type != EnemyType::Boss) {
                    continue;
                }
                const glm::vec2 offset = enemy.position - m_playerPosition;
                const float distanceSq = offset.x * offset.x + offset.y * offset.y;
                if (distanceSq < bestDistanceSq) {
                    bestDistanceSq = distanceSq;
                    pick = i;
                }
            }
        } else {
            // 其余几道仍然随机
            pick = static_cast<size_t>(RandInt(0, static_cast<int>(targets.size()) - 1));
        }
        const uint32_t targetIndex = targets[pick];
        targets.erase(targets.begin() + static_cast<std::ptrdiff_t>(pick));
        if (targetIndex >= m_enemies.size() || !m_enemies[targetIndex].alive) {
            continue;
        }
        // 落点取一份拷贝：接下来会改敌人状态（命中、死亡），位置本身不该被遮住
        const glm::vec2 impact = m_enemies[targetIndex].position;

        if (areaDamage) {
            // 术士：以落点为中心，半径内所有敌人各自结算一次（逐只掷暴击）
            m_grid.ForEachNear(impact, radius, [&](uint32_t index) {
                if (index >= m_enemies.size()) {
                    return;
                }
                Enemy& enemy = m_enemies[index];
                if (!enemy.alive || !CircleHit(enemy.position, enemy.radius, impact, radius)) {
                    return;
                }
                const bool crit = Rand(0.0f, 1.0f) < CritChance();
                DamageEnemy(enemy, damage * (crit ? CritMultiplier() : 1.0f), crit);
            });
        } else {
            // 其他角色：单体，只结算落点这一只
            Enemy& enemy = m_enemies[targetIndex];
            const bool crit = Rand(0.0f, 1.0f) < CritChance();
            DamageEnemy(enemy, damage * (crit ? CritMultiplier() : 1.0f), crit);
        }

        Fx bolt;
        bolt.kind = FxKind::Bolt;
        bolt.position = impact;
        bolt.radius = 260.0f;
        bolt.lifetime = 0.30f;
        bolt.color = C_STORM;
        m_effects.push_back(bolt);

        // 冲击环的半径就是真实伤害范围：单体时给一个小环，别让人以为范围雷还在
        SpawnRing(impact, areaDamage ? radius : 64.0f, 0.32f, C_STORM);
        SpawnParticles(impact, 6, C_STORM, 220.0f, 8.0f, 0.4f);
        m_shake = std::max(m_shake, 4.0f);
    }
    PlaySfx(SfxId::Storm);
    return true;
}

void SurvivorsLayer::UpdateWeapons(float deltaTime)
{
    m_bladeCooldown = std::max(0.0f, m_bladeCooldown - deltaTime);
    m_auraCooldown = std::max(0.0f, m_auraCooldown - deltaTime);
    m_scytheCooldown = std::max(0.0f, m_scytheCooldown - deltaTime);
    m_stormCooldown = std::max(0.0f, m_stormCooldown - deltaTime);

    const int bladeLevel = m_weaponLevel[static_cast<int>(WeaponId::MoonBlade)];
    if (bladeLevel > 0 && m_bladeCooldown <= 0.0f) {
        if (FireMoonBlade()) {
            m_bladeCooldown = BLADE.cooldown[bladeLevel] / AttackSpeedMul();
        }
    }

    const int auraLevel = m_weaponLevel[static_cast<int>(WeaponId::Aura)];
    if (auraLevel > 0 && m_auraCooldown <= 0.0f) {
        PulseAura();
        m_auraCooldown = AURA.cooldown[auraLevel] / AttackSpeedMul();
    }

    const int scytheLevel = m_weaponLevel[static_cast<int>(WeaponId::Scythe)];
    if (scytheLevel > 0 && m_scytheCooldown <= 0.0f) {
        if (ThrowScythes()) {
            m_scytheCooldown = SCYTHE.cooldown[scytheLevel] / AttackSpeedMul();
        }
    }

    const int stormLevel = m_weaponLevel[static_cast<int>(WeaponId::Storm)];
    if (stormLevel > 0 && m_stormCooldown <= 0.0f) {
        if (CastStorm()) {
            // 术士的雷暴基础间隔 -0.75 秒（StormInterval 里算，含下限）
            m_stormCooldown = StormInterval(stormLevel) / AttackSpeedMul();
        }
    }
}

void SurvivorsLayer::UpdateProjectiles(float deltaTime)
{
    for (Projectile& projectile : m_projectiles) {
        if (!projectile.alive) {
            continue;
        }
        projectile.age += deltaTime;

        if (projectile.kind == ShotKind::Scythe) {
            if (!projectile.returning && projectile.age > 0.55f) {
                projectile.returning = true;
                // 抛出与收回算两段独立的攻击：换段时清空命中表，同一只怪在回程还能再吃一次伤害
                // （去重只在**同一段**内生效，避免贴脸时一帧一次伤害）
                projectile.hitIds.clear();
            }
            if (projectile.returning) {
                const glm::vec2 toPlayer = m_playerPosition - projectile.position;
                const float distance = Length(toPlayer);
                if (distance < 46.0f) {
                    projectile.alive = false;
                    continue;
                }
                projectile.position += (toPlayer / distance) * 780.0f * deltaTime;
            } else {
                projectile.position += projectile.velocity * deltaTime;
                projectile.velocity *= std::exp(-deltaTime * 1.5f);
            }
            projectile.rotation += 15.0f * deltaTime;
        } else {
            projectile.position += projectile.velocity * deltaTime;
            projectile.rotation += projectile.spin * deltaTime;
        }

        bool consumed = false;
        bool retarget = false;
        m_grid.ForEachNear(projectile.position, 72.0f, [&](uint32_t index) {
            if (consumed || index >= m_enemies.size()) {
                return;
            }
            Enemy& enemy = m_enemies[index];
            if (!enemy.alive) {
                return;
            }
            if (!CircleHit(projectile.position, projectile.radius, enemy.position, enemy.radius)) {
                return;
            }

            // 同一枚弹丸在同一段飞行里对同一敌人只结算一次：命中后弹丸还会继续叠在这只怪身上飞几帧，
            // 不去重的话一帧一次伤害。回旋镰换段（抛出 → 收回）时会清表，所以目标去程回程各吃一次
            if (std::find(projectile.hitIds.begin(), projectile.hitIds.end(), enemy.id) != projectile.hitIds.end()) {
                return;
            }
            projectile.hitIds.push_back(enemy.id);

            const bool crit = Rand(0.0f, 1.0f) < CritChance();
            DamageEnemy(enemy, projectile.damage * (crit ? CritMultiplier() : 1.0f), crit);
            SpawnParticles(enemy.position, crit ? 6 : 3,
                           projectile.kind == ShotKind::Scythe ? C_SCYTHE : C_BLADE,
                           180.0f, 6.0f, 0.35f);

            if (projectile.kind == ShotKind::Blade) {
                --projectile.pierce;
                if (projectile.pierce <= 0) {
                    consumed = true;
                } else {
                    retarget = true;   // 猎手：还有穿透次数 → 命中后弹射向下一个目标
                }
            }
        });

        // 猎手：月刃的「穿透」改成弹射 —— 从弹丸**当前位置**找最近的、没被这枚飞刃打过的敌人，
        // 把速度直接转向它（找不到目标就保持原方向继续飞，靠寿命自然结束）
        if (retarget && !consumed && m_character == ItemId::CharHunter) {
            const Enemy* next = nullptr;
            float bestDistanceSq = HUNTER_BLADE_BOUNCE_RANGE * HUNTER_BLADE_BOUNCE_RANGE;
            m_grid.ForEachNear(projectile.position, HUNTER_BLADE_BOUNCE_RANGE, [&](uint32_t index) {
                if (index >= m_enemies.size()) {
                    return;
                }
                const Enemy& enemy = m_enemies[index];
                if (!enemy.alive || InShadow(enemy.position)) {
                    return;
                }
                if (std::find(projectile.hitIds.begin(), projectile.hitIds.end(), enemy.id) != projectile.hitIds.end()) {
                    return;   // 打过的不再回来（和「同一段只结算一次」共用这张表）
                }
                const glm::vec2 offset = enemy.position - projectile.position;
                const float distanceSq = offset.x * offset.x + offset.y * offset.y;
                if (distanceSq < bestDistanceSq) {
                    bestDistanceSq = distanceSq;
                    next = &enemy;
                }
            });
            if (next != nullptr) {
                const glm::vec2 direction = Normalize(next->position - projectile.position);
                projectile.velocity = direction * 880.0f;   // 弹射不减速
                projectile.rotation = std::atan2(-direction.x, direction.y);
                // 每次弹射给 0.5 秒续命，免得刚接上目标就因为寿命到点消失
                projectile.lifetime = std::max(projectile.lifetime, projectile.age + 0.5f);
                SpawnParticles(projectile.position, 3, C_BLADE, 120.0f, 4.0f, 0.2f);
            }
        }

        if (consumed || projectile.age > projectile.lifetime ||
            Length(projectile.position - m_playerPosition) > 1600.0f) {
            projectile.alive = false;
        }
    }
}

