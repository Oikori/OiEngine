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
// 关卡元素（横向卷轴，从起点营地一路向右）：
//   - 尖刺：朝上 / 朝下（吊顶）/ 贴墙，碰到即死；判定盒比外观略小（宽松）
//   - 伸缩尖刺：周期「缩回 → 预警露尖（不致命）→ 快速伸出（致命）→ 收回」，用于卡时机通过的闸门
//   - 移动平台：水平 / 垂直往返、端点停顿，站立时随平台一起位移
//   - 存档点：触碰点亮；坠落 / 扎刺回到最近的存档点（金币与用时保留），R 从头重开
//
// 操作：A/D 或 ←/→ 移动，空格/W/↑ 跳跃（按住跳更高），↓/S 快速下坠，R 重开，F4 手感调参
namespace OiEngine::Platformer {

    // 水平跟随（带朝向前瞻与平滑）、垂直方向固定的正交相机
    class PlatformerCamera : public Camera {
    public:
        void SetCenter(const glm::vec2& center) { m_center = center; }
        const glm::vec2& GetCenter() const { return m_center; }

        // 震屏：偏移叠加在 GetViewMatrix 里，随相机 UBO 一起上传
        void SetShakeOffset(const glm::vec3& offset) { m_shakeOffset = offset; }

        glm::vec3 GetPosition() const override;
        glm::mat4 GetViewMatrix() const override;
        glm::mat4 GetProjectionMatrix() const override;

    private:
        glm::vec2 m_center{0.0f, 0.0f};
        glm::vec3 m_shakeOffset{0.0f, 0.0f, 0.0f};
    };

    class PlatformerLayer : public Layer {
    public:
        PlatformerLayer() : Layer("Platformer") {}

        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate() override;
        void OnImGuiRender() override;
        void OnEvent(Event& event) override;

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

            // 4) 陷阱宽容度
            float spikeDeadlyExtend = 0.45f;  // 伸缩尖刺伸出比例超过该值才致命（预警期安全通过）
            float spikeHitboxShrink = 0.78f;  // 尖刺判定盒相对外观的宽度收缩系数（越大越宽容）
        };

        // 平台：静态，或沿 pathStart → pathEnd 往返的移动平台（speed > 0）
        struct Platform {
            glm::vec2 position;   // 中心（移动平台每帧更新）
            glm::vec2 halfSize;
            glm::vec4 bodyColor;
            glm::vec4 topColor;

            // 移动平台参数
            glm::vec2 pathStart{0.0f, 0.0f};   // 路径两端（中心点）
            glm::vec2 pathEnd{0.0f, 0.0f};
            float speed = 0.0f;                // 0 = 静态平台
            float pauseTime = 0.0f;            // 端点停顿（秒），便于上下车
            float t = 0.0f;                    // 路径参数 [0, 1]
            int pathDir = 1;
            float pauseTimer = 0.0f;
            glm::vec2 velocity{0.0f, 0.0f};    // 本帧位移速度（载人用）

            bool IsMoving() const { return speed > 0.0f; }
        };

        // 尖刺：position 是底面（附着面）中心，齿沿 direction 突出
        struct Spike {
            enum class Direction { Up, Down, Left, Right };

            glm::vec2 position{0.0f, 0.0f};
            float width = 90.0f;               // 覆盖宽度（沿附着面）
            float height = 44.0f;              // 完全伸出时的高度
            Direction direction = Direction::Up;
            bool cyclic = false;               // true = 周期伸缩
            float period = 3.0f;
            float phase = 0.0f;                // 周期内相位 [0, 1)
            float extend = 1.0f;               // 当前伸出比例 [0, 1]（静态恒为 1）
        };

        // 纯装饰（无碰撞，画在平台之后）：闸门支柱、背景构件
        struct Decor {
            glm::vec2 position;
            glm::vec2 halfSize;
            glm::vec4 color;
        };

        struct Checkpoint {
            glm::vec2 position;        // 底座中心（站立表面）
            bool activated = false;
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

        struct Hill {
            float x = 0.0f;            // 视差层内的循环基准位置
            float y = 0.0f;            // 底边高度
            float width = 0.0f;
            float height = 0.0f;
            float scrollFactor = 0.8f; // 相机位移参与比例（越小越远）
            float z = -3.6f;
            glm::vec4 color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        };

        enum class GameState {
            Playing,
            Victory
        };

        // ---- 资源 ----
        void CreateShaders();
        void CreateMeshes();
        void LoadSounds();
        void StartMusic();

        // ---- 关卡 ----
        void BuildBackdrop();   // 云层 + 远山（只在 OnAttach 生成一次）
        void BuildLevel();      // 平台 / 陷阱 / 存档点 / 金币（ResetLevel 会重建，保证状态归零）
        void AddGround(float centerX, float halfWidth, float top);                              // 草地（厚地面）
        void AddBlock(float centerX, float top, float halfWidth, float halfHeight, bool grass); // 浮空平台
        void AddMovingPlatform(const glm::vec2& from, const glm::vec2& to, const glm::vec2& halfSize,
                               float speed, float pauseTime, float startT = 0.0f);
        void AddDecor(float centerX, float centerY, float halfWidth, float halfHeight, const glm::vec4& color);
        void AddSpikes(const glm::vec2& basePosition, float width, Spike::Direction direction, float height = 44.0f);
        void AddCyclicSpikes(const glm::vec2& basePosition, float width, Spike::Direction direction,
                             float height, float period, float startPhase = 0.0f);
        void AddCheckpoint(float x, float surfaceY);
        void AddCoin(float x, float y);
        void ResetLevel();

        // ---- 更新 ----
        void UpdatePlatforms(float deltaTime);   // 移动平台推进 + 记录本帧速度（载人）
        void UpdateSpikes(float deltaTime);      // 伸缩尖刺相位推进
        void UpdatePlayer(float deltaTime);
        void UpdateCamera(float deltaTime);
        float CameraTargetX(float playerX) const;   // 相机跟随目标（含前瞻与世界范围钳制）
        void UpdateParticles(float deltaTime);
        void MoveAndCollide(const glm::vec2& delta);       // 分轴 + 子步进的 AABB 碰撞
        bool OverlapsPlatform(const Platform& platform) const;
        bool CheckHazards();                               // 尖刺判定；命中即死亡
        void KillPlayer(const char* cause, const glm::vec2& effectPosition);   // 死亡 → 回到最近存档点
        void RespawnAtCheckpoint();
        void SpawnPuff(const glm::vec2& position, int count, const glm::vec4& color,
                       float speed, float size, float gravity);

        // ---- 渲染 ----
        // 注意 tint 默认值必须写全分量：glm::vec4(1.0f) 是 (1,0,0,0)，alpha 为 0 会导致整批绘制被 discard
        void DrawMesh(const Ref<VertexArray>& mesh, const glm::mat4& transform,
                      const glm::vec4& tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        void RenderWorld();
        void RenderHitboxes();   // F4 开关：判定盒可视化
        void RenderHud();        // ImGui：信息栏 + 手感调试

        // ---- 辅助 ----
        static float SpikeRotation(Spike::Direction direction);   // 单位齿（指向 +Y）到实际朝向的旋转弧度
        bool SpikeHitbox(const Spike& spike, glm::vec2& center, glm::vec2& halfSize) const;

        // ---- 状态 ----
        GameState m_state = GameState::Playing;
        Tuning m_tuning;

        PlatformerCamera m_camera;
        float m_time = 0.0f;        // 本局用时（Victory 后停止增长）
        int m_deaths = 0;           // 死亡次数（尖刺 / 坠落）
        float m_shake = 0.0f;       // 震屏强度，随时间衰减
        float m_deathFlash = 0.0f;  // 死亡闪红强度 [0, 1]
        const char* m_deathCause = nullptr;   // 最近一次死因（指向字符串字面量）
        float m_deathCauseTimer = 0.0f;
        float m_inputLockTimer = 0.0f;        // 复活后的短暂输入锁，避免误操作

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
        int m_groundPlatformIndex = -1;   // 当前踩着的平台（移动平台载人用，-1 表示没有）

        // 关卡数据
        std::vector<Platform> m_platforms;      // 静态 + 移动平台统一存这里，碰撞逻辑一致
        std::vector<Spike> m_spikes;
        std::vector<Decor> m_decors;
        std::vector<Checkpoint> m_checkpoints;
        std::vector<Coin> m_coins;
        glm::vec2 m_spawnPoint{0.0f, 0.0f};
        glm::vec2 m_respawnPoint{0.0f, 0.0f};   // 最近激活的存档点（中心坐标）
        glm::vec2 m_flagPosition{0.0f, 0.0f};   // 终点旗杆底部
        int m_coinsCollected = 0;

        // 粒子与背景
        std::vector<Puff> m_puffs;
        std::vector<Cloud> m_clouds;
        std::vector<Hill> m_hills;
        float m_cloudDrift = 0.0f;     // 云层自身的缓慢漂移

        // 网格资源
        Ref<Shader> m_shader;
        Ref<VertexArray> m_solidMesh;   // 单位矩形（宽 1 高 1，中心原点，白色）
        Ref<VertexArray> m_skyMesh;     // 天空渐变（顶点色固定）
        Ref<VertexArray> m_glowMesh;    // 径向光晕（半径 1）
        Ref<VertexArray> m_coinMesh;    // 金币（菱形，外径 0.5）
        Ref<VertexArray> m_playerMesh;  // 玩家（宽 1 高 1.4，局部坐标）
        Ref<VertexArray> m_flagMesh;    // 旗面（附着点在左侧）
        Ref<VertexArray> m_pennantMesh; // 存档点小旗（白色顶点色，靠 tint 上色）
        Ref<VertexArray> m_spikeMesh;   // 尖刺单齿（底边 y=0、齿尖 y=1、宽 1）
        Ref<VertexArray> m_hillMesh;    // 远山（单位半圆，底边 y=0）

        bool m_showDebug = false;
        bool m_showHitboxes = false;    // F4 面板开关：绘制判定盒

        // 音频资源（Platformer/assets/audio，构建后复制到输出目录）
        Ref<AudioClip> m_bgmClip;
        Ref<AudioClip> m_jumpSound;
        Ref<AudioClip> m_landSound;
        Ref<AudioClip> m_coinSound;
        Ref<AudioClip> m_checkpointSound;
        Ref<AudioClip> m_deathSound;
        Ref<AudioClip> m_victorySound;
        Ref<SoundInstance> m_bgm;
        float m_audioVolume = 0.8f;
        bool m_audioMuted = false;
    };
}
