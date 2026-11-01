// 《永夜幸存者》敌人：空间哈希、刷怪节奏、移动与分离、接触伤害、Boss 登场。
//
// 敌人上限与数值表见 SurvivorsInternal.h（MAX_ENEMIES / ENEMY_TUNING）。
#include "SurvivorsGame.h"
#include "SurvivorsInternal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <format>
#include <string>

using namespace OiEngine;
using namespace OiEngine::Survivors;

// ============================ 空间哈希 ============================

// ============================ 空间哈希 ============================

void SurvivorsLayer::EnemyGrid::Build(const std::vector<Enemy>& enemies)
{
    cells.clear();
    for (uint32_t i = 0; i < static_cast<uint32_t>(enemies.size()); ++i) {
        const Enemy& enemy = enemies[i];
        if (!enemy.alive) {
            continue;
        }
        const int cx = static_cast<int>(std::floor(enemy.position.x / CELL_SIZE));
        const int cy = static_cast<int>(std::floor(enemy.position.y / CELL_SIZE));
        cells[Key(cx, cy)].push_back(i);
    }
}

void SurvivorsLayer::UpdateEnemies(float deltaTime)
{
    for (Enemy& enemy : m_enemies) {
        if (!enemy.alive) {
            continue;
        }
        enemy.age += deltaTime;
        enemy.hitFlash = std::max(0.0f, enemy.hitFlash - deltaTime * 6.5f);

        glm::vec2 toPlayer = m_playerPosition - enemy.position;
        const float distance = Length(toPlayer);
        glm::vec2 direction = distance > 0.001f ? toPlayer / distance : glm::vec2(0.0f, 1.0f);

        // 血月使徒的冲刺：蓄力 / 冲刺阶段由技能接管移动，常规转向与加速整段跳过
        if (enemy.type == EnemyType::Boss && UpdateBossDash(enemy, distance, deltaTime)) {
            enemy.impulse *= std::exp(-deltaTime * 5.5f);
            continue;
        }

        if (enemy.type == EnemyType::Bat || enemy.type == EnemyType::Wraith) {
            const glm::vec2 tangent(-direction.y, direction.x);
            const float amplitude = enemy.type == EnemyType::Bat ? 0.30f : 0.45f;
            const float frequency = enemy.type == EnemyType::Bat ? 7.0f : 2.1f;
            direction = Normalize(direction + tangent * (std::sin(enemy.age * frequency + enemy.phase) * amplitude));
        }

        const float blend = 1.0f - std::exp(-deltaTime * 5.0f);
        enemy.velocity = glm::mix(enemy.velocity, direction * enemy.speed, blend);
        enemy.position += (enemy.velocity + enemy.impulse) * deltaTime;
        enemy.impulse *= std::exp(-deltaTime * 5.5f);
    }

    // 群体分离：用空间哈希限制邻近查询规模
    m_grid.Build(m_enemies);
    for (uint32_t i = 0; i < static_cast<uint32_t>(m_enemies.size()); ++i) {
        Enemy& enemy = m_enemies[i];
        if (!enemy.alive) {
            continue;
        }
        // 冲刺中的使徒不吃软推挤：冲出去的轨迹得是它自己锁定的那条直线
        const bool bossSkillLocked = enemy.type == EnemyType::Boss && enemy.dashPhase != Enemy::DashPhase::None;
        if (!bossSkillLocked) {
            glm::vec2 push(0.0f, 0.0f);
            m_grid.ForEachNear(enemy.position, enemy.radius + 96.0f, [&](uint32_t otherIndex) {
                if (otherIndex == i || otherIndex >= m_enemies.size()) {
                    return;
                }
                const Enemy& other = m_enemies[otherIndex];
                if (!other.alive) {
                    return;
                }
                const glm::vec2 offset = enemy.position - other.position;
                const float distance = Length(offset);
                const float minimum = enemy.radius + other.radius;
                if (distance > 0.001f && distance < minimum) {
                    push += offset / distance * ((minimum - distance) / minimum);
                }
            });
            const float weight = enemy.type == EnemyType::Boss ? 24.0f : 90.0f;
            enemy.position += push * weight * deltaTime;

            // 玩家也是碰撞体：把贴在身上的敌人挤开一点（Boss 体型大，只要求少量间隙）
            const glm::vec2 away = enemy.position - m_playerPosition;
            const float distanceToPlayer = Length(away);
            const float minimumDistance = PLAYER_RADIUS + enemy.radius * (enemy.type == EnemyType::Boss ? 0.35f : 0.85f);
            if (distanceToPlayer > 0.001f && distanceToPlayer < minimumDistance) {
                enemy.position += away / distanceToPlayer * (minimumDistance - distanceToPlayer);
            }
        }
    }

    // 跑出很远的敌人从视野外重新入场：避免玩家直线狂奔时屏幕里空无一人
    for (Enemy& enemy : m_enemies) {
        if (!enemy.alive) {
            continue;
        }
        const float distance = Length(enemy.position - m_playerPosition);
        if (enemy.type == EnemyType::Boss) {
            // 冲刺途中不重定位：锁定的轨迹要被玩家看清楚，只收招之后再判断
            if (distance > 1100.0f && enemy.dashPhase == Enemy::DashPhase::None) {
                const float angle = Rand(0.0f, TAU);
                enemy.position = m_playerPosition + glm::vec2(std::cos(angle), std::sin(angle)) * 560.0f;
                enemy.velocity = glm::vec2(0.0f, 0.0f);
            }
        } else if (distance > 1400.0f) {
            enemy.position = EdgeSpawnPoint(60.0f);
            enemy.velocity = glm::vec2(0.0f, 0.0f);
            enemy.impulse = glm::vec2(0.0f, 0.0f);
        }
    }

    // 供武器索敌 / 弹丸碰撞使用的最新网格
    m_grid.Build(m_enemies);
}

// 血月使徒的冲刺：None（常规追击）→ Charge（站定蓄力，方向已锁）→ Dash（沿锁定的直线冲）→ None。
// 返回 true = 这一帧的移动由技能接管，调用方跳过常规转向。
bool SurvivorsLayer::UpdateBossDash(Enemy& boss, float distanceToPlayer, float deltaTime)
{
    boss.dashCooldown = std::max(0.0f, boss.dashCooldown - deltaTime);

    if (boss.dashPhase == Enemy::DashPhase::Charge) {
        // 蓄力：方向在开始时锁死，身子只带着惯性再滑一点点就停住（给玩家 1 秒横移躲开的机会）
        boss.dashTimer -= deltaTime;
        boss.velocity *= std::exp(-deltaTime * 14.0f);
        boss.impulse = glm::vec2(0.0f, 0.0f);
        boss.position += boss.velocity * deltaTime;
        if (boss.dashTimer <= 0.0f) {
            boss.dashPhase = Enemy::DashPhase::Dash;
            boss.dashTimer = BOSS_DASH_DISTANCE / BOSS_DASH_SPEED;
            boss.velocity = boss.dashDirection * BOSS_DASH_SPEED;
            SpawnRing(boss.position, boss.radius * 3.4f, 0.32f, C_BLOOD);
            m_shake = std::max(m_shake, 10.0f);
        }
        return true;
    }

    if (boss.dashPhase == Enemy::DashPhase::Dash) {
        boss.dashTimer -= deltaTime;
        boss.velocity = boss.dashDirection * BOSS_DASH_SPEED;
        const glm::vec2 next = boss.position + boss.velocity * deltaTime;

        // 冲到围栏就停在场地边上：阴影区里的敌人无敌，不能让它一路冲进黑处躲起来
        const float margin = boss.radius * 0.9f;
        const glm::vec2 inside(std::clamp(next.x, -ARENA_HALF + margin, ARENA_HALF - margin),
                               std::clamp(next.y, -ARENA_HALF + margin, ARENA_HALF - margin));
        const bool hitFence = Length(inside - next) > 0.001f;
        boss.position = inside;

        if (hitFence || boss.dashTimer <= 0.0f) {
            boss.dashPhase = Enemy::DashPhase::None;
            boss.dashCooldown = BOSS_DASH_COOLDOWN;
            // 收招只留一点点前冲惯性（常规转向按 5/s 收敛，全留着会再多滑出 BOSS_DASH_SPEED/5 ≈ 320）
            boss.velocity = boss.dashDirection * (BOSS_DASH_SPEED * 0.25f);
            SpawnRing(boss.position, boss.radius * 2.4f, 0.28f, glm::vec4(0.72f, 0.16f, 0.24f, 1.0f));
            if (hitFence) {
                m_shake = std::max(m_shake, 7.0f);
            }
        }
        return true;
    }

    // 常规阶段：进入触发距离（太近了不冲，免得贴着玩家原地站桩）且冷却结束
    // → 锁定当前朝向玩家的方向，开始蓄力
    if (boss.dashCooldown <= 0.0f && distanceToPlayer >= BOSS_DASH_MIN_RANGE &&
        distanceToPlayer <= BOSS_DASH_TRIGGER_RANGE) {
        boss.dashPhase = Enemy::DashPhase::Charge;
        boss.dashTimer = BOSS_DASH_CHARGE_TIME;
        boss.dashDirection = (m_playerPosition - boss.position) / distanceToPlayer;
        SpawnRing(boss.position, boss.radius * 2.2f, 0.45f, glm::vec4(1.0f, 0.32f, 0.30f, 1.0f));
        return true;
    }
    return false;
}

void SurvivorsLayer::UpdateEnemySpawning(float deltaTime)
{
    m_spawnTimer -= deltaTime;
    if (m_spawnTimer > 0.0f) {
        return;
    }
    if (static_cast<int>(m_enemies.size()) >= MAX_ENEMIES) {
        m_spawnTimer = 0.3f;
        return;
    }

    // 刷怪节奏与兵种全部按关卡推进（第 1 关只有蝙蝠，之后逐关放开）
    const float stage = static_cast<float>(m_stage);
    const float interval = std::max(0.22f, 0.85f - 0.055f * (stage - 1.0f));
    m_spawnTimer = interval * Rand(0.80f, 1.20f);
    const int wave = 2 + std::min(6, (m_stage - 1) / 2 + ((m_stage >= 2) ? 1 : 0));

    for (int i = 0; i < wave; ++i) {
        if (static_cast<int>(m_enemies.size()) >= MAX_ENEMIES) {
            break;
        }
        const glm::vec2 position = EdgeSpawnPoint(40.0f);

        const float weightBat = 1.0f;
        const float weightGhoul = m_stage >= 2 ? std::min(0.90f, 0.18f * stage) : 0.0f;
        const float weightWraith = m_stage >= 4 ? std::min(0.75f, 0.12f * (stage - 3.0f)) : 0.0f;
        const float weightBrute = m_stage >= 7 ? std::min(0.50f, 0.16f * (stage - 6.0f)) : 0.0f;
        const float total = weightBat + weightGhoul + weightWraith + weightBrute;
        float cursor = Rand(0.0f, total);
        EnemyType type = EnemyType::Bat;
        if (cursor >= weightBat) {
            cursor -= weightBat;
            if (cursor < weightGhoul) {
                type = EnemyType::Ghoul;
            } else if (cursor < weightGhoul + weightWraith) {
                type = EnemyType::Wraith;
            } else {
                type = EnemyType::Brute;
            }
        }

        SpawnEnemy(type, position);
    }
}

void SurvivorsLayer::UpdateContactDamage(float deltaTime)
{
    (void)deltaTime;
    if (m_hurtCooldown > 0.0f || !m_playerAlive) {
        return;
    }

    for (const Enemy& enemy : m_enemies) {
        if (!enemy.alive) {
            continue;
        }
        if (!CircleHit(enemy.position, enemy.radius, m_playerPosition, PLAYER_RADIUS)) {
            continue;
        }

        DamagePlayer(enemy.damage);

        // 受击后把周围的敌人推开，避免连续贴脸
        for (Enemy& other : m_enemies) {
            if (!other.alive) {
                continue;
            }
            const glm::vec2 offset = other.position - m_playerPosition;
            const float distance = Length(offset);
            if (distance > 0.001f && distance < 180.0f) {
                const float strength = 520.0f * (1.0f - distance / 180.0f);
                other.impulse += (offset / distance) * strength;
            }
        }
        break;
    }
}

// ============================ 战斗 ============================

void SurvivorsLayer::SpawnEnemy(EnemyType type, const glm::vec2& position)
{
    const int index = std::min(static_cast<int>(type), 4);
    const EnemyTuning& tuning = ENEMY_TUNING[index];

    Enemy enemy;
    enemy.type = type;
    enemy.id = m_nextEnemyId++;
    enemy.position = position;
    enemy.radius = tuning.radius;
    enemy.speed = tuning.speed;
    enemy.phase = Rand(0.0f, TAU);

    // 数值随关卡递增（曲线见 SurvivorsInternal.h 的 Enemy*Scale / BossHpScale）：
    // 小怪血量线性（+30% 每关），伤害二次（1 + 0.05·(n−1)²，第 10 关约 5 倍），防御线性（+8% 每关）；
    // Boss 的血量**单独一条二次曲线**（第 5 关 2 倍、第 10 关 10 倍），伤害 / 防御与小怪共用同一套。
    // 经验与金币**不随关卡放大**，直接用表里的固定值（掉落节奏靠刷怪量收紧来提升）
    const bool isBoss = type == EnemyType::Boss;
    enemy.damage = tuning.damage * EnemyDamageScale(m_stage);
    enemy.defense = tuning.defense * EnemyDefenseScale(m_stage);
    enemy.xp = tuning.xp;
    enemy.coins = tuning.coins;

    const float hp = tuning.hp * (isBoss ? BossHpScale(m_stage) : EnemyHpScale(m_stage));
    enemy.hp = hp;
    enemy.maxHp = hp;

    m_enemies.push_back(enemy);
}

glm::vec2 SurvivorsLayer::EdgeSpawnPoint(float margin)
{
    // 沿随机方向投射到「画面矩形 + 边距」的外侧：无论从哪边来，都刚好在屏幕外一点
    const float angle = Rand(0.0f, TAU);
    const glm::vec2 direction(std::cos(angle), std::sin(angle));
    const float horizontal = std::abs(direction.x) > 0.0001f
        ? (m_halfWidth + margin) / std::abs(direction.x) : 100000.0f;
    const float vertical = std::abs(direction.y) > 0.0001f
        ? (m_halfHeight + margin) / std::abs(direction.y) : 100000.0f;
    const float radius = std::min(horizontal, vertical) * Rand(1.0f, 1.08f);
    return m_cameraFocus + direction * radius;
}

void SurvivorsLayer::SpawnBoss()
{
    const float angle = Rand(0.0f, TAU);
    const glm::vec2 position = m_playerPosition + glm::vec2(std::cos(angle), std::sin(angle)) * 560.0f;
    SpawnEnemy(EnemyType::Boss, position);

    m_shake = 16.0f;
    SpawnRing(position, 220.0f, 0.9f, C_BLOOD);
    OI_INFO("Survivors: 血月使徒降临（第 {} 关）", m_stage);
}

