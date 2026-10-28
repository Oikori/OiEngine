#pragma once

#include <string>

#include "Engine/Core/Base.h"
#include "Engine/Math.h"

namespace OiEngine {
    // 距离衰减模型（映射到 miniaudio 的 ma_attenuation_model）
    enum class AudioAttenuation {
        None = 0,
        Inverse,
        Linear,
        Exponential
    };

    // 一次播放的参数。2D 音效忽略 position / attenuation / distance 字段。
    struct AudioPlaybackSettings {
        float volume = 1.0f;
        float pitch = 1.0f;             // 必须 > 0
        bool looping = false;
        bool spatialized = false;       // true：按 listener 与 position 做 3D 声像与距离衰减
        glm::vec3 position = glm::vec3(0.0f);
        AudioAttenuation attenuation = AudioAttenuation::Inverse;
        float minDistance = 1.0f;
        float maxDistance = 100.0f;
        float rolloff = 1.0f;
    };

    class AudioClip;

    // 一个正在播放的声部（voice）。同一个 AudioClip 每次 Play 都会创建独立声部，可叠加播放。
    // Play 的返回值不用保存：AudioEngine::Update 会保留到播放结束再回收。
    // 只能在主线程使用；AudioEngine::Shutdown 之后播放 / 修改类方法均为空操作。
    class OI_ENGINE_API SoundInstance {
    public:
        ~SoundInstance();

        // 停止、回到开头并标记结束；下一次 AudioEngine::Update 将其移出活动声部
        void Stop();
        // 暂停并保留当前位置
        void Pause();
        // 从暂停位置继续；已 Stop 的声部会从头开始
        void Resume();

        bool IsPlaying() const;
        bool IsPaused() const;
        // 已停止、自然播放结束，或引擎已关闭
        bool IsFinished() const;

        void SetVolume(float volume);
        float GetVolume() const;
        void SetPitch(float pitch);
        void SetLooping(bool looping);
        void SetSpatializationEnabled(bool enabled);
        void SetPosition(const glm::vec3& position);
        void SetAttenuation(AudioAttenuation model, float minDistance, float maxDistance, float rolloff = 1.0f);
        // 跳转到指定秒数（超出范围时截断）
        void SeekTo(float seconds);
        // 当前播放位置（秒）；未初始化 / 已释放时返回 0
        float GetPlaybackPosition() const;
        float GetDuration() const;

    private:
        friend class AudioEngine;
        SoundInstance();

        // 立即停止并释放 miniaudio 声部；可重复调用
        void Release();

        struct Impl;
        Scope<Impl> m_impl;
    };

    // 音频文件资源：加载一次，可多次 Play。非流式时内部共享预解码的 PCM 数据，每次 Play 只分配一个独立声部。
    // 支持 miniaudio 内置解码格式（wav / mp3 / flac / ogg 等），路径相对当前工作目录。
    class OI_ENGINE_API AudioClip {
    public:
        ~AudioClip();

        // 编辑器 / 引擎未初始化时返回 nullptr
        Ref<SoundInstance> Play(const AudioPlaybackSettings& settings = {});

        const std::string& GetPath() const;
        float GetDuration() const;
        bool IsValid() const;
        bool IsStreaming() const;

    private:
        friend class AudioEngine;
        AudioClip();

        // 卸载预解码数据；可重复调用
        void Release();

        struct Impl;
        Scope<Impl> m_impl;
    };

    // 全局音频设备与资源管理。Init / Shutdown / Update 由 Application 自动调用，
    // 业务层只需要 LoadClip + Play。初始化失败不会阻塞引擎运行（音频静默）。
    class OI_ENGINE_API AudioEngine {
    public:
        // 启动音频设备；重复调用无副作用。失败返回 false（可用 IsInitialized 查询）
        static bool Init();
        static void Shutdown();
        static bool IsInitialized();

        // 主线程每帧调用：回收播放结束 / 已停止且不再被外部持有的声部
        static void Update();

        // 加载音频文件并按规范化路径缓存；同一文件重复加载返回同一个 AudioClip。
        // streaming = true 时按流式播放（适合长 BGM，不预解码整段 PCM）；默认整段解码（适合短音效，可低开销叠加）。
        static Ref<AudioClip> LoadClip(const std::string& filePath, bool streaming = false);
        static void StopAll();

        static void SetMasterVolume(float volume);
        static float GetMasterVolume();
        static void SetMuted(bool muted);
        static bool IsMuted();

        // 3D 听者：forward 为世界空间朝向，up 为世界空间上方
        static void SetListenerPosition(const glm::vec3& position);
        static void SetListenerDirection(const glm::vec3& forward, const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f));
        static glm::vec3 GetListenerPosition();
        static glm::vec3 GetListenerDirection();

    private:
        friend class AudioClip;

        // AudioClip::Play 的实现：创建一个独立声部并注册到活动列表
        static Ref<SoundInstance> PlayClip(const AudioClip& clip, const AudioPlaybackSettings& settings);
    };
}
