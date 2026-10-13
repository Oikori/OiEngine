#pragma once

#include <vector>

#include "OiEngine.h"

// 平台跳跃（Platformer）：单屏高度视野、横向卷轴的 2D 平台跳跃小游戏，几何全部程序化生成。
//
// 手感三要素（F4 面板可实时调参）：
//   1. 加速下落：下落重力约为上升重力的 1.9 倍，落地干脆；按住 ↓/S 会再叠加一份下坠加速度
//   2. 顶点滞空：跳跃接近最高点（|vy| 进入滞空带）时重力临时减弱，形成短暂停顿感
//   3. 可变跳跃高度：按住跳跃键的时长决定跳跃高度（松开即削减上升速度），最大高度由起跳初速度封顶
//
// 操作：A/D 或 ←/→ 移动，空格/W/↑ 跳跃（按住跳更高），↓/S 快速下坠，R 重开，F4 手感调参
namespace OiEngine::SandBox {

    // 水平跟随（带朝向前瞻与平滑）、垂直方向固定的正交相机
    class PlatformerCamera : public Camera {
    public:
        void SetCenter(const glm::vec2& center) { m_center = center; }
        const glm::vec2& GetCenter() const { return m_center; }

        glm::vec3 GetPosition() const override;
        glm::mat4 GetViewMatrix() const override;
        glm::mat4 GetProjectionMatrix() const override;

    private:
        glm::vec2 m_center{0.0f, 0.0f};
    };

    class PlatformerLayer : public Layer {
    public:
        PlatformerLayer() : Layer("Platformer") {}

        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate() override;
        void OnImGuiRender() override;
        void OnEvent(Event& event) override;

        // 场景切换：未激活时跳过更新与渲染
        void SetActive(bool active) { m_active = active; }
        bool IsActive() const { return m_active; }

    private:
        // 手感参数集中管理，F4 面板直接调；单位：像素、像素/秒、像素/秒²
        struct Tuning {
            // 水平移动
            float maxRunSpeed = 340.0f;
            float groundAccel = 2900.0f;
            float airAccel = 2200.0f;
            float groundFriction = 3200.0f;
            float airDrag = 650.0f;

            // 1) 加速下落：上升与下落使用不同重力
            float riseGravity = 2100.0f;      // 上升重力（较小，给足滞空用于调整落点）
            float fallGravity = 3900.0f;      // 下落重力（较大，落地干脆）
            float fastFallExtra = 2400.0f;    // 按住 ↓/S 时的额外下坠加速度
            float maxFallSpeed = 1500.0f;     // 终端速度

            // 2) 顶点滞空：靠近最高点时进入低重力带
            float apexBand = 120.0f;          // 进入滞空带的速度阈值 |vy|
            float apexGravityScale = 0.45f;   // 滞空带内的重力系数
            float apexExitScale = 1.7f;       // 速度超过 band*该系数时退出（迟滞，避免反复横跳）

            // 3) 可变跳跃高度：起跳后按住越快松手跳得越低
            float jumpVelocity = 830.0f;      // 起跳初速度（对应最大跳跃高度）
            float jumpCutScale = 0.55f;       // 松开跳跃键时上升速度的削减系数
            float jumpCutMinVelocity = 220.0f;// 低于该速度不再削减（接近顶点时松手无影响）

            // 宽容时间
            float coyoteTime = 0.10f;         // 离开平台后仍允许起跳的宽限时间
            float jumpBufferTime = 0.12f;     // 落地前提前按下也算数
        };

        struct Platform {
            glm::vec2 position;   // 中心
            glm::vec2 halfSize;
            glm::vec4 bodyColor;
            glm::vec4 topColor;   // 顶面高光条颜色
        };

        struct Coin {
            glm::vec2 position;
            float phase = 0.0f;   // 自转相位（错开每枚金币的动画）
            bool collected = false;
        };

        struct Puff {
            glm::vec2 position;
            glm::vec2 velocity;
            float gravity = 0.0f;
            float age = 0.0f;
            float lifetime = 0.5f;
            float size = 16.0f;
            glm::vec4 color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        };

        struct Cloud {
            float x = 0.0f;       // 视差层内的循环基准位置
            float y = 0.0f;
            float scale = 1.0f;
            float z = -2.7f;      // 每朵云错开 z，避免同 z 重叠部分被深度测试剔除
            glm::vec4 color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        };

        enum class GameState {
            Playing,
            Victory
        };

        // ---- 资源 ----
        void CreateShaders();
        void CreateMeshes();

        // ---- 关卡 ----
        void BuildLevel();
        void AddGround(float centerX, float halfWidth, float top);                              // 草地（厚地面）
        void AddBlock(float centerX, float top, float halfWidth, float halfHeight, bool grass); // 浮空平台
        void AddCoin(float x, float y);
        void ResetLevel();

        // ---- 更新 ----
        void UpdatePlayer(float deltaTime);
        void UpdateCamera(float deltaTime);
        float CameraTargetX(float playerX) const;   // 相机跟随目标（含前瞻与世界范围钳制）
        void UpdateParticles(float deltaTime);
        void MoveAndCollide(const glm::vec2& delta);       // 分轴 + 子步进的 AABB 碰撞
        bool OverlapsPlatform(const Platform& platform) const;
        void SpawnPuff(const glm::vec2& position, int count, const glm::vec4& color,
                       float speed, float size, float gravity);

        // ---- 渲染 ----
        // 注意 tint 默认值必须写全分量：glm::vec4(1.0f) 是 (1,0,0,0)，alpha 为 0 会导致整批绘制被 discard
        void DrawMesh(const Ref<VertexArray>& mesh, const glm::mat4& transform,
                      const glm::vec4& tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        void RenderWorld();
        void RenderHud();   // ImGui：信息栏 + 手感调试

        // ---- 状态 ----
        bool m_active = true;
        GameState m_state = GameState::Playing;
        Tuning m_tuning;

        PlatformerCamera m_camera;
        float m_time = 0.0f;        // 本局用时（Victory 后停止增长）
        int m_falls = 0;            // 坠落次数

        // 玩家（位置为中心，速度 y 向上为正）
        glm::vec2 m_playerPosition;
        glm::vec2 m_velocity;
        bool m_grounded = false;
        bool m_wasGrounded = false;
        float m_coyoteTimer = 0.0f;
        float m_jumpBufferTimer = 0.0f;
        bool m_jumpDownPrev = false;   // 用于检测"刚刚按下"（轮询式输入）
        bool m_jumpHeld = false;       // 本次跳跃是否仍按住（可变跳跃高度）
        float m_jumpHoldTime = 0.0f;   // 本次跳跃按住时长（调试显示）
        bool m_roseInAir = false;      // 本次离地后是否明显上升过（决定顶点滞空是否生效）
        bool m_apexActive = false;     // 当前是否处于滞空带
        bool m_fastFalling = false;    // 当前是否处于快速下坠（调试显示）
        int m_facing = 1;              // 1 右 / -1 左（绘制朝向）
        float m_squash = 1.0f;         // 挤压拉伸：落地 < 1、起跳 > 1，随时间回弹到 1
        float m_landSpeed = 0.0f;      // 最近一次落地速度（调试显示）

        // 关卡数据
        std::vector<Platform> m_platforms;
        std::vector<Coin> m_coins;
        glm::vec2 m_spawnPoint{0.0f, 0.0f};
        glm::vec2 m_flagPosition{0.0f, 0.0f};   // 终点旗杆底部
        int m_coinsCollected = 0;

        // 粒子与背景
        std::vector<Puff> m_puffs;
        std::vector<Cloud> m_clouds;
        float m_cloudDrift = 0.0f;     // 云层自身的缓慢漂移

        // 网格资源
        Ref<Shader> m_shader;
        Ref<VertexArray> m_solidMesh;   // 单位矩形（宽 1 高 1，中心原点，白色）
        Ref<VertexArray> m_skyMesh;     // 天空渐变（顶点色固定）
        Ref<VertexArray> m_glowMesh;    // 径向光晕（半径 1）
        Ref<VertexArray> m_coinMesh;    // 金币（菱形，外径 0.5）
        Ref<VertexArray> m_playerMesh;  // 玩家（宽 1 高 1.4，局部坐标）
        Ref<VertexArray> m_flagMesh;    // 旗面（附着点在左侧）

        bool m_showDebug = false;
    };
}
