// 《永夜幸存者》战斗结算与掉落：伤害 / 击杀 / 玩家受伤、金币掉落与拾取、
// 粒子 / 光环 / 闪光 / 伤害数字等世界反馈，以及它们的生命周期更新。
#include "SurvivorsGame.h"
#include "SurvivorsInternal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <format>
#include <string>

using namespace OiEngine;
using namespace OiEngine::Survivors;

// ============================ 伤害与击杀 ============================

void SurvivorsLayer::DamageEnemy(Enemy& enemy, float damage, bool crit)
{
    if (!enemy.alive) {
        return;
    }
    // 玩家进攻：自己的固定穿甲扣目标防御（怪物只有防御，没有穿甲）
    const float dealt = ResolveDamage(damage, m_baseStats.penFlat, enemy.defense);
    enemy.hp -= dealt;
    enemy.hitFlash = 0.18f;
    m_damageDealt += dealt;
    // 命中音随机变调，避免同频堆在一起像报错声
    PlaySfx(SfxId::Hit, 1.0f, Rand(0.92f, 1.10f));
    SpawnDamageNumber(enemy.position + glm::vec2(0.0f, enemy.radius * 0.75f), dealt, crit);
    if (enemy.hp <= 0.0f) {
        KillEnemy(enemy);
    }
}
void SurvivorsLayer::KillEnemy(Enemy& enemy)
{
    enemy.alive = false;
    ++m_kills;
    PlaySfx(SfxId::Kill, 1.0f, Rand(0.90f, 1.12f));

    const bool isBoss = enemy.type == EnemyType::Boss;
    const glm::vec4 color = [&]() {
        switch (enemy.type) {
            case EnemyType::Bat: return glm::vec4(0.60f, 0.34f, 0.72f, 1.0f);
            case EnemyType::Ghoul: return glm::vec4(0.38f, 0.58f, 0.38f, 1.0f);
            case EnemyType::Wraith: return glm::vec4(0.46f, 0.84f, 0.92f, 1.0f);
            case EnemyType::Brute: return glm::vec4(0.72f, 0.38f, 0.24f, 1.0f);
            case EnemyType::Boss: return glm::vec4(0.90f, 0.16f, 0.20f, 1.0f);
            default: return glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        }
    }();

    SpawnParticles(enemy.position, isBoss ? 46 : (enemy.type == EnemyType::Brute ? 18 : 9),
                   color, isBoss ? 520.0f : 280.0f, isBoss ? 20.0f : 10.0f, 0.7f);
    SpawnRing(enemy.position, enemy.radius * (isBoss ? 6.0f : 2.2f), isBoss ? 0.9f : 0.35f, color);

    // 经验击杀当场结算（不再等拾取），金币作为掉落物撒在地上
    GainExp(enemy.xp);

    // 事件：击杀（噬魂匕回血 / 哀嚎钟的「每 N 杀」音波）。放在经验之后：
    // 事件里的范围伤害可能连锁击杀，经验照常逐只结算
    RaiseEvent(EventId::Kill, enemy.position);

    if (isBoss) {
        m_shake = 26.0f;
        // 使徒的钱袋：拆成 12 枚散落，余数单独补一枚（总额不掉）
        const int perCoin = std::max(1, enemy.coins / 12);
        for (int i = 0; i < 12; ++i) {
            SpawnCoin(enemy.position + glm::vec2(Rand(-60.0f, 60.0f), Rand(-60.0f, 60.0f)), perCoin, true);
        }
        const int remainder = enemy.coins - perCoin * 12;
        if (remainder > 0) {
            SpawnCoin(enemy.position, remainder, true);
        }
        m_playerHp = std::min(m_playerMaxHp, m_playerHp + 30.0f);
        SpawnRing(enemy.position, 420.0f, 1.2f, C_GOLD);
        OI_INFO("Survivors: 血月使徒被击败");
        return;
    }

    if (enemy.type == EnemyType::Brute) {
        // 石魔的钱袋摔开：拆成两枚带弧线弹出去
        const int half = std::max(1, enemy.coins / 2);
        SpawnCoin(enemy.position, half, true);
        SpawnCoin(enemy.position + glm::vec2(Rand(-20.0f, 20.0f), Rand(-20.0f, 20.0f)),
                  enemy.coins - half, true);
        m_shake = std::max(m_shake, 5.0f);
    } else {
        SpawnCoin(enemy.position, enemy.coins, false);
    }
}

void SurvivorsLayer::DamagePlayer(float amount)
{
    if (!m_playerAlive) {
        return;
    }
    // 玩家挨打：怪物没有穿甲，防御全额生效
    const float taken = ResolveDamage(amount, 0.0f, m_baseStats.defense);
    m_playerHp -= taken;
    m_hurtCooldown = 0.95f;
    m_hurtFlash = 1.0f;
    m_shake = std::max(m_shake, 15.0f);
    PlaySfx(SfxId::Hurt);
    SpawnParticles(m_playerPosition, 14, C_BLOOD, 300.0f, 8.0f, 0.55f);

    // 事件：受伤（受难者圣痕的「每 N 次受伤」暗焰反击）。
    // 放在死亡判定之前：反击的回复可以救命，反击的伤害也照常结算
    RaiseEvent(EventId::Hurt, m_playerPosition);

    // 荆棘之甲：把刚吃到的伤害按比例反弹给贴身的敌人（反弹的是结算后的伤害）
    if (m_baseStats.thornRatio > 0.0f && taken > 0.0f) {
        const float reflected = taken * m_baseStats.thornRatio;
        m_grid.ForEachNear(m_playerPosition, THORN_RADIUS, [&](uint32_t index) {
            if (index >= m_enemies.size()) {
                return;
            }
            Enemy& enemy = m_enemies[index];
            if (!enemy.alive || !CircleHit(enemy.position, enemy.radius, m_playerPosition, THORN_RADIUS)) {
                return;
            }
            DamageEnemy(enemy, reflected, false);
        });
        SpawnRing(m_playerPosition, THORN_RADIUS, 0.32f, C_THORN);
    }

    if (m_playerHp <= 0.0f) {
        // 不朽之心：先把命续回来（每件只能拿一次，所以复活次数不会无限叠）
        if (m_baseStats.reviveCharges > 0) {
            --m_baseStats.reviveCharges;
            m_playerHp = m_playerMaxHp * REVIVE_HP_FRACTION;
            m_hurtCooldown = 1.8f;
            m_shake = std::max(m_shake, 26.0f);
            SpawnRing(m_playerPosition, 420.0f, 1.0f, C_GOLD);
            SpawnFlash(m_playerPosition, 260.0f, 0.7f, glm::vec4(1.0f, 0.86f, 0.52f, 0.9f));
            SpawnParticles(m_playerPosition, 36, C_GOLD, 420.0f, 12.0f, 0.8f);
            PlaySfx(SfxId::LevelUp, 1.0f, 0.72f);
            OI_INFO("Survivors: 不朽之心发动，以 {:.0f}% 生命复活（剩余次数 {}）",
                    REVIVE_HP_FRACTION * 100.0f, m_baseStats.reviveCharges);
            return;
        }

        m_playerHp = 0.0f;
        m_playerAlive = false;
        m_state = State::GameOver;
        m_shake = 34.0f;
        if (m_bgm) {
            m_bgm->Stop();
            m_bgm.reset();
        }
        PlaySfx(SfxId::GameOver);
        SpawnParticles(m_playerPosition, 46, C_BLOOD, 460.0f, 14.0f, 0.9f);
        SpawnRing(m_playerPosition, 360.0f, 1.1f, C_BLOOD);
        OI_INFO("Survivors: 玩家阵亡，存活 {} 秒，击杀 {}", static_cast<int>(m_time), m_kills);
    }
}
// ============================ 金币 / 粒子 / 特效 ============================

void SurvivorsLayer::SpawnCoin(const glm::vec2& position, int value, bool burst)
{
    if (static_cast<int>(m_coinDrops.size()) >= MAX_COINS) {
        // 掉落表满了：把最老的一枚直接折成钱，避免击杀掉落的金币被静默吞掉
        if (!m_coinDrops.empty()) {
            const int value = CoinGain(m_coinDrops.front().value);
            m_coins += value;
            m_coinsEarned += value;
            m_coinDrops.erase(m_coinDrops.begin());
        }
    }
    Coin coin;
    coin.position = position;
    coin.value = std::max(1, value);
    coin.tier = value <= 2 ? 0 : (value <= 8 ? 1 : 2);
    coin.lifetime = COIN_LIFETIME;
    coin.phase = Rand(0.0f, TAU);
    if (burst) {
        const float angle = Rand(0.0f, TAU);
        coin.velocity = glm::vec2(std::cos(angle), std::sin(angle)) * Rand(60.0f, 150.0f);
    }
    m_coinDrops.push_back(coin);
}

void SurvivorsLayer::SpawnParticles(const glm::vec2& position, int count, const glm::vec4& color,
                                    float speed, float size, float lifetime, float zBias)
{
    (void)zBias;
    if (static_cast<int>(m_particles.size()) >= MAX_PARTICLES) {
        return;
    }
    for (int i = 0; i < count; ++i) {
        if (static_cast<int>(m_particles.size()) >= MAX_PARTICLES) {
            break;
        }
        Particle particle;
        particle.position = position;
        const float angle = Rand(0.0f, TAU);
        const float velocity = speed * Rand(0.25f, 1.0f);
        particle.velocity = glm::vec2(std::cos(angle), std::sin(angle)) * velocity;
        particle.size = size * Rand(0.6f, 1.4f);
        particle.lifetime = lifetime * Rand(0.6f, 1.2f);
        particle.drag = Rand(2.0f, 4.5f);
        particle.color = color;
        m_particles.push_back(particle);
    }
}

void SurvivorsLayer::SpawnRing(const glm::vec2& position, float radius, float lifetime, const glm::vec4& color)
{
    Fx fx;
    fx.kind = FxKind::Ring;
    fx.position = position;
    fx.radius = radius;
    fx.lifetime = lifetime;
    fx.color = color;
    m_effects.push_back(fx);
}

void SurvivorsLayer::SpawnFlash(const glm::vec2& position, float radius, float lifetime, const glm::vec4& color)
{
    Fx fx;
    fx.kind = FxKind::Flash;
    fx.position = position;
    fx.radius = radius;
    fx.lifetime = lifetime;
    fx.color = color;
    m_effects.push_back(fx);
}

void SurvivorsLayer::SpawnDamageNumber(const glm::vec2& position, float value, bool crit)
{
    if (static_cast<int>(m_damageNumbers.size()) >= MAX_DAMAGE_NUMBERS) {
        m_damageNumbers.erase(m_damageNumbers.begin());
    }
    DamageNumber number;
    number.position = position;
    number.value = value;
    number.crit = crit;
    number.lifetime = crit ? 0.85f : 0.6f;
    number.drift = glm::vec2(Rand(-18.0f, 18.0f), crit ? 70.0f : 46.0f);
    m_damageNumbers.push_back(number);
}
void SurvivorsLayer::SpawnCoinNumber(const glm::vec2& position, int value)
{
    if (static_cast<int>(m_damageNumbers.size()) >= MAX_DAMAGE_NUMBERS) {
        m_damageNumbers.erase(m_damageNumbers.begin());
    }
    DamageNumber number;
    number.position = position;
    number.value = static_cast<float>(value);
    number.gold = true;
    number.lifetime = 0.7f;
    number.drift = glm::vec2(Rand(-14.0f, 14.0f), 56.0f);
    m_damageNumbers.push_back(number);
}

// ============================ 场上反馈的更新 ============================

void SurvivorsLayer::UpdateCoins(float deltaTime)
{
    const float pickupRadius = PickupRadius();
    // 本轮拾到手的金币先记下来，事件放到遍历之后统一发：
    // 拾币事件里的范围伤害会连锁击杀 → SpawnCoin 会 push / erase m_coinDrops，
    // 在 range-for 里边遍历边改这个容器会失效（含 vector 扩容导致的引用悬空）
    struct Pickup {
        glm::vec2 position{ 0.0f, 0.0f };
        int value = 1;
    };
    std::vector<Pickup> pickups;

    for (Coin& coin : m_coinDrops) {
        if (!coin.alive) {
            continue;
        }
        coin.age += deltaTime;
        if (coin.age >= coin.lifetime) {
            coin.alive = false;   // 太久没捡就散在夜里（经验已在击杀时给过，不会亏成长）
            continue;
        }

        const glm::vec2 toPlayer = m_playerPosition - coin.position;
        const float distance = Length(toPlayer);
        if (distance < pickupRadius) {
            const glm::vec2 direction = distance > 0.001f ? toPlayer / distance : glm::vec2(0.0f, 1.0f);
            const float speed = 300.0f + (pickupRadius - distance) * 4.0f;
            coin.velocity = glm::mix(coin.velocity, direction * speed, 1.0f - std::exp(-deltaTime * 11.0f));
        } else {
            coin.velocity *= std::exp(-deltaTime * 3.0f);
        }
        coin.position += coin.velocity * deltaTime;

        if (distance < 26.0f) {
            coin.alive = false;
            // 金币收益倍率（贪婪之匣）在这里统一折算，拾取提示展示的就是实际到手的数额
            const int value = CoinGain(coin.value);
            m_coins += value;
            m_coinsEarned += value;
            PlaySfx(SfxId::Pickup, 1.0f, Rand(0.94f, 1.10f));
            SpawnParticles(coin.position, 4, COIN_COLORS[coin.tier], 130.0f, 5.0f, 0.3f);
            SpawnCoinNumber(coin.position, value);
            pickups.push_back({ coin.position, value });
        }
    }

    // 事件：拾取金币（铸币者之锤 / 拾荒护符 / 贪欲之链）。
    // 金额用折算后的 value，和钱包里真正到手的数额一致
    for (const Pickup& pickup : pickups) {
        RaiseEvent(EventId::CoinPickup, pickup.position, pickup.value);
    }
}

void SurvivorsLayer::UpdateParticles(float deltaTime)
{
    for (Particle& particle : m_particles) {
        particle.age += deltaTime;
        particle.position += particle.velocity * deltaTime;
        particle.velocity *= std::exp(-particle.drag * deltaTime);
    }
    std::erase_if(m_particles, [](const Particle& particle) { return particle.age >= particle.lifetime; });
}

void SurvivorsLayer::UpdateEffects(float deltaTime)
{
    for (Fx& effect : m_effects) {
        effect.age += deltaTime;
    }
    std::erase_if(m_effects, [](const Fx& effect) { return effect.age >= effect.lifetime; });

    for (DamageNumber& number : m_damageNumbers) {
        number.age += deltaTime;
        number.position += number.drift * deltaTime;
    }
    std::erase_if(m_damageNumbers, [](const DamageNumber& number) { return number.age >= number.lifetime; });
}

