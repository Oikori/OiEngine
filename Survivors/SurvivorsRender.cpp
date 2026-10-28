// 《永夜幸存者》世界渲染：全部程序化顶点色网格的绘制顺序与变换。
//
// 深度是 GL_LESS，绘制顺序按 z 递增；屏幕暗角必须最后画（见 DrawScreenOverlay）。
#include "SurvivorsGame.h"
#include "SurvivorsInternal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <format>
#include <string>

using namespace OiEngine;
using namespace OiEngine::Survivors;

// ============================ 渲染 ============================

void SurvivorsLayer::DrawMesh(const Ref<VertexArray>& mesh, const glm::mat4& transform,
                              const glm::vec4& tint, float falloff)
{
    m_shader->SetMat4("u_Transform", transform);
    m_shader->SetFloat4("u_Tint", tint);
    m_shader->SetFloat("u_Falloff", falloff);
    RenderCommand::DrawIndexed(mesh);
}

void SurvivorsLayer::RenderWorld()
{
    m_shader->Bind();
    m_shader->SetFloat("u_Falloff", 0.0f);

    DrawBackground();
    DrawGround();
    DrawCoins();
    DrawEnemies();
    DrawEffectsUnder();
    DrawPlayer();
    DrawProjectiles();
    DrawParticles();
    DrawEffectsOver();
    DrawScreenOverlay();
}

void SurvivorsLayer::DrawBackground()
{
    // 月光：以玩家上方为中心的大范围冷色光
    DrawMesh(m_art.disc, Translate(glm::vec3(m_cameraFocus.x, m_cameraFocus.y + 120.0f, -9.2f)) *
                         Scale(glm::vec3(1650.0f, 1250.0f, 1.0f)),
             glm::vec4(0.34f, 0.30f, 0.62f, 0.26f), 0.9f);
    DrawMesh(m_art.disc, Translate(glm::vec3(m_cameraFocus.x, m_cameraFocus.y + 330.0f, -9.1f)) *
                         Scale(glm::vec3(250.0f, 250.0f, 1.0f)),
             glm::vec4(0.80f, 0.84f, 1.00f, 0.30f), 0.6f);

    // 雾团
    for (const Wisp& wisp : m_wisps) {
        DrawMesh(m_art.disc, Translate(glm::vec3(wisp.position.x, wisp.position.y, -8.8f)) *
                             Scale(glm::vec3(wisp.radius, wisp.radius * 0.72f, 1.0f)),
                 wisp.color, 1.0f);
    }

    // 上方紫色薄雾
    DrawMesh(m_art.vignette, Translate(glm::vec3(m_cameraFocus.x, m_cameraFocus.y + m_halfHeight - 210.0f, -8.6f)) *
                             Scale(glm::vec3(m_halfWidth * 2.0f, 420.0f, 1.0f)),
             glm::vec4(0.34f, 0.27f, 0.62f, 0.22f));
}

void SurvivorsLayer::DrawGround()
{
    constexpr float GRID_STEP = 120.0f;
    const float gridX = std::floor(m_cameraFocus.x / GRID_STEP) * GRID_STEP;
    const float gridY = std::floor(m_cameraFocus.y / GRID_STEP) * GRID_STEP;
    m_shader->SetMat4("u_Transform", Translate(glm::vec3(gridX, gridY, -7.55f)));
    m_shader->SetFloat4("u_Tint", WHITE);
    m_shader->SetFloat("u_Falloff", 0.0f);
    RenderCommand::DrawLines(m_art.lineGrid, m_art.lineGridVertexCount);

    // 按格子哈希放置的墓碑 / 草丛：无需存储，走多远都有稳定装饰
    constexpr float CELL = 190.0f;
    const int cx0 = static_cast<int>(std::floor((m_cameraFocus.x - m_halfWidth - CELL) / CELL));
    const int cx1 = static_cast<int>(std::floor((m_cameraFocus.x + m_halfWidth + CELL) / CELL));
    const int cy0 = static_cast<int>(std::floor((m_cameraFocus.y - m_halfHeight - CELL) / CELL));
    const int cy1 = static_cast<int>(std::floor((m_cameraFocus.y + m_halfHeight + CELL) / CELL));

    for (int cy = cy0; cy <= cy1; ++cy) {
        for (int cx = cx0; cx <= cx1; ++cx) {
            const uint32_t hash = HashCell(cx, cy);
            const uint32_t kind = hash & 15u;
            if (kind > 4) {
                continue;
            }
            const glm::vec2 position(
                (static_cast<float>(cx) + HashUnit(hash, 4)) * CELL,
                (static_cast<float>(cy) + HashUnit(hash, 14)) * CELL);
            if (kind <= 1) {
                const float scale = 52.0f + HashUnit(hash, 20) * 34.0f;
                const float rotation = (HashUnit(hash, 24) - 0.5f) * 0.6f;
                DrawMesh(m_art.tombstone, Translate(glm::vec3(position.x, position.y, -7.3f)) *
                                          RotateZ(rotation) * Scale(glm::vec3(scale, scale, 1.0f)),
                         glm::vec4(1.0f, 1.0f, 1.0f, 0.9f));
            } else {
                const float scale = 24.0f + HashUnit(hash, 20) * 22.0f;
                const float rotation = (HashUnit(hash, 24) - 0.5f) * 1.2f;
                DrawMesh(m_art.grass, Translate(glm::vec3(position.x, position.y, -7.4f)) *
                                      RotateZ(rotation) * Scale(glm::vec3(scale, scale, 1.0f)),
                         glm::vec4(1.0f, 1.0f, 1.0f, 0.7f));
            }
        }
    }
}

void SurvivorsLayer::DrawCoins()
{
    for (const Coin& coin : m_coinDrops) {
        if (!coin.alive) {
            continue;
        }
        const float size = coin.tier == 0 ? 16.0f : (coin.tier == 1 ? 22.0f : 30.0f);
        const float pulse = 0.9f + 0.1f * std::sin(coin.age * 4.0f + coin.phase);
        const float bob = std::sin(coin.age * 3.0f + coin.phase) * 2.2f;
        // 快过期时淡出，给玩家「该捡钱了」的提示
        const float fade = std::min(1.0f, (coin.lifetime - coin.age) / 2.0f);

        DrawMesh(m_art.disc, Translate(glm::vec3(coin.position.x, coin.position.y, -2.05f)) *
                             Scale(glm::vec3(size * 1.9f * pulse, size * 1.9f * pulse, 1.0f)),
                 glm::vec4(1.0f, 0.80f, 0.34f, 0.18f * fade), 1.0f);
        DrawMesh(m_art.coin, Translate(glm::vec3(coin.position.x, coin.position.y + bob, -2.0f)) *
                        RotateZ(coin.age * 1.6f + coin.phase) * Scale(glm::vec3(size, size, 1.0f)),
                 glm::vec4(1.0f, 1.0f, 1.0f, fade));
    }
}

void SurvivorsLayer::DrawEnemies()
{
    for (const Enemy& enemy : m_enemies) {
        if (!enemy.alive) {
            continue;
        }
        const Ref<VertexArray>* mesh = &m_art.bat;
        float bob = 0.0f;
        float scaleX = 1.0f;
        switch (enemy.type) {
            case EnemyType::Bat:
                mesh = &m_art.bat;
                bob = std::sin(enemy.age * 13.0f + enemy.phase) * 3.0f;
                scaleX = 0.78f + 0.30f * std::abs(std::sin(enemy.age * 13.0f + enemy.phase));
                break;
            case EnemyType::Ghoul:
                mesh = &m_art.ghoul;
                bob = std::sin(enemy.age * 4.5f + enemy.phase) * 2.0f;
                break;
            case EnemyType::Wraith:
                mesh = &m_art.wraith;
                bob = std::sin(enemy.age * 2.6f + enemy.phase) * 4.0f;
                break;
            case EnemyType::Brute:
                mesh = &m_art.brute;
                bob = std::sin(enemy.age * 3.1f + enemy.phase) * 1.6f;
                break;
            case EnemyType::Boss:
                mesh = &m_art.boss;
                bob = std::sin(enemy.age * 1.7f) * 3.0f;
                break;
            default:
                break;
        }

        const float scale = enemy.radius * 2.0f;

        if (enemy.type != EnemyType::Bat) {
            DrawMesh(m_art.disc, Translate(glm::vec3(enemy.position.x, enemy.position.y - 4.0f, -1.5f)) *
                                 Scale(glm::vec3(scale * 0.58f, scale * 0.26f, 1.0f)),
                     glm::vec4(0.0f, 0.0f, 0.0f, 0.38f), 1.0f);
        }

        glm::vec4 tint = WHITE;
        if (enemy.hitFlash > 0.0f) {
            const float flash = enemy.hitFlash / 0.18f;
            tint = glm::vec4(1.0f + flash * 1.4f, 1.0f + flash * 0.6f, 1.0f + flash * 0.6f, 1.0f);
        }

        if (enemy.type == EnemyType::Boss) {
            DrawMesh(m_art.ring, Translate(glm::vec3(enemy.position.x, enemy.position.y - 6.0f, -1.2f)) *
                                 Scale(glm::vec3(scale * 1.9f, scale * 1.9f, 1.0f)),
                     glm::vec4(0.90f, 0.18f, 0.22f, 0.32f));
            DrawMesh(m_art.disc, Translate(glm::vec3(enemy.position.x, enemy.position.y, -1.3f)) *
                                 Scale(glm::vec3(scale * 1.2f, scale * 1.2f, 1.0f)),
                     glm::vec4(0.70f, 0.10f, 0.14f, 0.18f), 1.0f);
        }

        DrawMesh(*mesh, Translate(glm::vec3(enemy.position.x, enemy.position.y + bob, 0.0f)) *
                        Scale(glm::vec3(scale * scaleX, scale, 1.0f)), tint);
    }
}

void SurvivorsLayer::DrawPlayer()
{
    const float bob = std::sin(m_walkPhase * 2.1f) * 2.2f;
    const int auraLevel = m_weaponLevel[static_cast<int>(WeaponId::Aura)];

    if (!m_playerAlive) {
        DrawMesh(m_art.disc, Translate(glm::vec3(m_playerPosition.x, m_playerPosition.y - 4.0f, -1.0f)) *
                             Scale(glm::vec3(70.0f, 26.0f, 1.0f)),
                 glm::vec4(0.35f, 0.03f, 0.06f, 0.55f), 1.0f);
        return;
    }

    // 圣光环法阵与光晕
    if (auraLevel > 0) {
        const float radius = AURA.radius[auraLevel];
        DrawMesh(m_art.disc, Translate(glm::vec3(m_playerPosition.x, m_playerPosition.y, -1.1f)) *
                             Scale(glm::vec3(radius, radius, 1.0f)),
                 glm::vec4(1.0f, 0.82f, 0.42f, 0.075f), 1.0f);
        DrawMesh(m_art.sigil, Translate(glm::vec3(m_playerPosition.x, m_playerPosition.y, -1.05f)) *
                              RotateZ(m_time * 0.35f) * Scale(glm::vec3(radius * 0.92f, radius * 0.92f, 1.0f)),
                 glm::vec4(1.0f, 0.86f, 0.50f, 0.34f));
    }

    DrawMesh(m_art.disc, Translate(glm::vec3(m_playerPosition.x, m_playerPosition.y - 3.0f, -1.4f)) *
                         Scale(glm::vec3(44.0f, 18.0f, 1.0f)),
             glm::vec4(0.0f, 0.0f, 0.0f, 0.45f), 1.0f);

    // 身周微光；没有圣光环时额外补一圈小型法阵，避免角色淹没在黑暗里
    DrawMesh(m_art.disc, Translate(glm::vec3(m_playerPosition.x, m_playerPosition.y, -1.3f)) *
                         Scale(glm::vec3(130.0f, 130.0f, 1.0f)),
             glm::vec4(0.36f, 0.62f, 1.0f, 0.10f), 1.0f);
    if (auraLevel == 0) {
        DrawMesh(m_art.sigil, Translate(glm::vec3(m_playerPosition.x, m_playerPosition.y, -1.15f)) *
                              RotateZ(-m_time * 0.22f) * Scale(glm::vec3(62.0f, 62.0f, 1.0f)),
                 glm::vec4(0.55f, 0.85f, 1.0f, 0.18f));
    }

    // 法杖宝石的光
    DrawMesh(m_art.disc, Translate(glm::vec3(m_playerPosition.x + 16.0f, m_playerPosition.y + 22.0f + bob, 0.5f)) *
                         Scale(glm::vec3(30.0f, 30.0f, 1.0f)),
             glm::vec4(0.55f, 0.95f, 1.0f, 0.22f), 1.0f);

    glm::vec4 tint = WHITE;
    if (m_hurtCooldown > 0.0f && std::fmod(m_hurtCooldown, 0.22f) < 0.12f) {
        tint = glm::vec4(1.6f, 0.7f, 0.7f, 1.0f);
    }
    DrawMesh(m_art.player, Translate(glm::vec3(m_playerPosition.x, m_playerPosition.y + bob, 0.6f)) *
                           Scale(glm::vec3(64.0f, 64.0f, 1.0f)), tint);
}

void SurvivorsLayer::DrawProjectiles()
{
    for (const Projectile& projectile : m_projectiles) {
        if (!projectile.alive) {
            continue;
        }
        const bool blade = projectile.kind == ShotKind::Blade;
        const glm::vec4 color = blade ? C_BLADE : C_SCYTHE;
        const float size = blade ? 34.0f : 50.0f;

        DrawMesh(m_art.disc, Translate(glm::vec3(projectile.position.x, projectile.position.y, 0.72f)) *
                             Scale(glm::vec3(size * 0.9f, size * 0.9f, 1.0f)),
                 glm::vec4(color.x, color.y, color.z, blade ? 0.22f : 0.30f), 1.0f);
        DrawMesh(blade ? m_art.blade : m_art.scythe,
                 Translate(glm::vec3(projectile.position.x, projectile.position.y, 0.80f)) *
                     RotateZ(projectile.rotation) * Scale(glm::vec3(size, size, 1.0f)),
                 WHITE);
    }
}

void SurvivorsLayer::DrawParticles()
{
    for (const Particle& particle : m_particles) {
        const float t = std::min(particle.age / particle.lifetime, 1.0f);
        const float alpha = particle.color.w * (1.0f - t) * (1.0f - t);
        const float size = particle.size * (1.0f - t * 0.45f);
        DrawMesh(m_art.disc, Translate(glm::vec3(particle.position.x, particle.position.y, 1.25f)) *
                             Scale(glm::vec3(size, size, 1.0f)),
                 glm::vec4(particle.color.x, particle.color.y, particle.color.z, alpha), 1.0f);
    }
}

void SurvivorsLayer::DrawEffectsUnder()
{
    for (const Fx& effect : m_effects) {
        if (effect.kind != FxKind::Ring) {
            continue;
        }
        const float t = std::min(effect.age / effect.lifetime, 1.0f);
        const float expand = 0.30f + 0.70f * (t * t * (3.0f - 2.0f * t));
        const float radius = effect.radius * expand;
        const float alpha = (1.0f - t) * effect.color.w;
        DrawMesh(m_art.ring, Translate(glm::vec3(effect.position.x, effect.position.y, 0.22f)) *
                             Scale(glm::vec3(radius, radius, 1.0f)),
                 glm::vec4(effect.color.x, effect.color.y, effect.color.z, alpha));
    }
}

void SurvivorsLayer::DrawEffectsOver()
{
    for (const Fx& effect : m_effects) {
        const float t = std::min(effect.age / effect.lifetime, 1.0f);
        if (effect.kind == FxKind::Flash) {
            const float alpha = (1.0f - t) * (1.0f - t) * effect.color.w;
            const float radius = effect.radius * (0.6f + 0.5f * t);
            DrawMesh(m_art.disc, Translate(glm::vec3(effect.position.x, effect.position.y, 1.40f)) *
                                 Scale(glm::vec3(radius, radius, 1.0f)),
                 glm::vec4(effect.color.x, effect.color.y, effect.color.z, alpha), 1.0f);
        } else if (effect.kind == FxKind::Bolt) {
            const float alpha = (1.0f - t) * effect.color.w;
            DrawMesh(m_art.bolt, Translate(glm::vec3(effect.position.x, effect.position.y + 40.0f, 1.48f)) *
                                  Scale(glm::vec3(effect.radius * 0.55f, effect.radius * 2.0f, 1.0f)),
                 glm::vec4(1.0f, 1.0f, 1.0f, alpha));
            DrawMesh(m_art.disc, Translate(glm::vec3(effect.position.x, effect.position.y, 1.45f)) *
                                 Scale(glm::vec3(effect.radius * 0.55f, effect.radius * 0.55f, 1.0f)),
                 glm::vec4(effect.color.x, effect.color.y, effect.color.z, alpha * 0.7f), 1.0f);
        }
    }
}

void SurvivorsLayer::DrawScreenOverlay()
{
    // 径向暗角最后画：无接缝地压暗画面四周（深度也在这里写入，所以全屏闪光必须放在更高的 z）
    DrawMesh(m_art.vignetteDisc, Translate(glm::vec3(m_cameraFocus.x, m_cameraFocus.y, 3.0f)) *
                                 Scale(glm::vec3(m_halfWidth * 1.50f, m_halfHeight * 1.50f, 1.0f)),
             glm::vec4(0.0f, 0.0f, 0.0f, 0.62f));

    const glm::vec3 origin(m_cameraFocus.x, m_cameraFocus.y, 0.0f);
    const glm::mat4 fullScreen = Scale(glm::vec3(m_halfWidth * 2.0f, m_halfHeight * 2.0f, 1.0f));

    if (m_hurtFlash > 0.01f) {
        DrawMesh(m_art.quad, Translate(origin + glm::vec3(0.0f, 0.0f, 3.60f)) * fullScreen,
                 glm::vec4(0.75f, 0.06f, 0.10f, 0.30f * m_hurtFlash));
    }
    if (m_levelFlash > 0.01f) {
        DrawMesh(m_art.quad, Translate(origin + glm::vec3(0.0f, 0.0f, 3.55f)) * fullScreen,
                 glm::vec4(0.55f, 0.80f, 1.00f, 0.22f * m_levelFlash));
    }
    if (m_playerAlive && m_playerHp > 0.0f && m_playerHp / m_playerMaxHp < 0.30f) {
        const float pulse = 0.5f + 0.5f * std::sin(m_time * 7.0f);
        const float alpha = (0.30f - m_playerHp / m_playerMaxHp) / 0.30f * (0.10f + 0.10f * pulse);
        DrawMesh(m_art.quad, Translate(origin + glm::vec3(0.0f, 0.0f, 3.50f)) * fullScreen,
                 glm::vec4(0.55f, 0.02f, 0.06f, alpha));
    }
    if (m_stageBannerBoss && m_stageBanner > 0.0f) {
        // Boss 关进关时压一层血光，随横幅一起淡出
        const float pulse = 0.5f + 0.5f * std::sin(m_time * 9.0f);
        const float intensity = std::min(1.0f, m_stageBanner / 2.2f);
        DrawMesh(m_art.quad, Translate(origin + glm::vec3(0.0f, 0.0f, 3.45f)) * fullScreen,
                 glm::vec4(0.40f, 0.02f, 0.05f, 0.10f * intensity * pulse));
    }
}

