#pragma once

#include <cstdint>
#include <vector>

#include "OiEngine.h"

// 飞机大战：在正交投影的 X-Y 平面上运行的纵向卷轴射击游戏。
// 所有机体/子弹/爆炸特效均为程序化生成的几何体（顶点色），不依赖图片资源。
namespace OiEngine::SandBox {

    // 正交相机：world 单位与屏幕像素 1:1 对应，1 世界单位 = 1 像素
    class PlaneGameCamera : public Camera {
    public:
        Math::Vector3 GetPosition() const override;
        Math::Matrix4 GetViewMatrix() const override;
        Math::Matrix4 GetProjectionMatrix() const override;
    };

    class PlaneGameLayer : public Layer {
    public:
        PlaneGameLayer() : Layer("PlaneGame") {}

        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate() override;
        void OnImGuiRender() override;
        void OnEvent(Event& event) override;

        // 场景切换：未激活时跳过更新与渲染
        void SetActive(bool active) { m_active = active; }
        bool IsActive() const { return m_active; }

    private:
        // 实体类型决定更新行为；子弹/粒子只做匀速移动
        enum class EntityKind {
            PlayerBullet,
            EnemyA,
            EnemyB,
            EnemyC,
            EnemyBullet,
            Particle
        };

        // 爆炸动画随时间放大或缩小（Ring 为线框、Burst 为十字闪光、Flash 为圆形闪光）
        enum class EffectKind {
            Ring,
            Burst,
            Flash
        };

        enum class GameState {
            Playing,
            Paused,
            GameOver
        };

        struct Entity {
            EntityKind kind = EntityKind::Particle;
            Math::Vector2 position;
            Math::Vector2 velocity;
            float rotation = 0.0f;      // 弧度
            float scale = 1.0f;
            float radius = 0.0f;        // 碰撞半径（世界单位）
            float lifetime = 0.0f;      // 总寿命，用于特效缩放/淡出
            float age = 0.0f;
            float spin = 0.0f;          // 角速度（粒子旋转）
            float wave = 0.0f;          // 摆动相位（敌机）
            float fireCooldown = 0.0f;
            Math::Vector4 color = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
            const Ref<VertexArray>* mesh = nullptr;
        };

        struct Effect {
            EffectKind kind = EffectKind::Ring;
            Math::Vector2 position;
            float radius = 0.0f;
            float lifetime = 0.3f;
            float age = 0.0f;
            Math::Vector4 color = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
            float thickness = 3.0f;
        };

        struct Star {
            Math::Vector2 position;
            float speed = 0.0f;
            float size = 0.0f;
            Math::Vector4 color = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        };

        struct Nebula {
            Math::Vector2 position;
            float speed = 0.0f;
            float scale = 1.0f;
            Math::Vector4 color = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        };

        // ---- 资源创建 ----
        void CreateShaders();
        void CreateMeshes();
        void CreateStars();
        Ref<VertexArray> CreateDynamicVertexArray(uint32_t maxVertices, uint32_t indexCount);

        // ---- 游戏流程 ----
        void ResetGame();
        void UpdatePlayer(float deltaTime);
        void UpdateEntities(float deltaTime);
        void UpdateEffects(float deltaTime);
        void UpdateBackground(float deltaTime);
        void UpdateSpawning(float deltaTime);
        void RebuildStarVertices();
        void SpawnEnemy(EntityKind kind, const Math::Vector2& position);
        void SpawnFighterShot();                 // 玩家机翼双发
        void SpawnEnemyShot(const Entity& enemy); // 敌机根据类型开火
        void SpawnExplosion(const Math::Vector2& position, const Math::Vector4& color,
                            float radius, bool large);
        void SpawnPlayerDeath(const Math::Vector2& position);
        void KillEnemy(uint32_t index);
        void HitPlayer();
        void AddScore(int points);

        // ---- 渲染 ----
        // 注意 tint 默认值必须写全分量：Vector4(1.0f) 是 (1,0,0,0)，alpha 为 0 会导致整批绘制被 discard
        void DrawMesh(const Ref<VertexArray>& mesh, const Math::Matrix4& transform,
                      const Math::Vector4& tint = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f), float lineWidth = 0.0f);
        void RenderWorld(const Math::Matrix4& viewOffset);
        void RenderHud();  // ImGui 界面（得分 / 生命 / 暂停 / 结束）

        // ---- 状态 ----
        bool m_active = true;
        GameState m_state = GameState::Playing;

        // 相机与视图偏移
        PlaneGameCamera m_camera;
        float m_shake = 0.0f;       // 受击震屏强度，随时间衰减

        // 玩家
        Math::Vector2 m_playerPosition;
        Math::Vector2 m_playerVelocity;
        float m_playerRotation = 0.0f;
        int m_lives = 3;
        float m_invulnerable = 0.0f;   // 无敌剩余时间（秒）
        float m_fireCooldown = 0.0f;
        float m_hitFlash = 0.0f;       // 受击红屏，随时间衰减

        // 全局
        float m_time = 0.0f;           // 本局游戏时间（暂停时不增长）
        float m_spawnTimer = 0.0f;
        float m_spawnInterval = 1.2f;
        float m_difficulty = 1.0f;
        int m_score = 0;
        int m_highScore = 0;
        int m_combo = 1;               // 连击倍率，击杀累积、超时重置
        float m_comboTimer = 0.0f;

        // 世界边界（由窗口宽高决定，居中于原点；正上方为 +Y）
        float m_halfHeight = 540.0f;
        float m_halfWidth = 960.0f;

        // 网格资源
        Ref<Shader> m_shader;
        Ref<VertexArray> m_playerMesh;
        Ref<VertexArray> m_enemyAMesh;
        Ref<VertexArray> m_enemyBMesh;
        Ref<VertexArray> m_enemyCMesh;
        Ref<VertexArray> m_playerBulletMesh;
        Ref<VertexArray> m_enemyBulletMesh;
        Ref<VertexArray> m_enemyBulletHeavyMesh;
        Ref<VertexArray> m_ringMesh;
        Ref<VertexArray> m_burstMesh;
        Ref<VertexArray> m_glowMesh;
        Ref<VertexArray> m_vignetteMesh;
        Ref<VertexArray> m_fullscreenMesh;

        // 动态星空（每帧重建顶点）
        Ref<VertexArray> m_starMesh;
        Ref<VertexBuffer> m_starVertexBuffer;
        std::vector<float> m_starVertices;
        std::vector<Star> m_stars;
        std::vector<Nebula> m_nebulae;

        std::vector<Entity> m_entities;
        std::vector<Effect> m_effects;

        // 调试
        bool m_showDebug = false;
    };
}
