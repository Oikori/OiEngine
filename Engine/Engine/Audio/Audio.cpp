#include "Audio.h"

#include <algorithm>
#include <filesystem>
#include <unordered_map>
#include <utility>
#include <vector>

#include "miniaudio.h"

namespace OiEngine {
    namespace {
        ma_engine s_engine;

        bool s_initialized = false;
        float s_masterVolume = 1.0f;
        bool s_muted = false;

        glm::vec3 s_listenerPosition = glm::vec3(0.0f);
        glm::vec3 s_listenerForward = glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 s_listenerUp = glm::vec3(0.0f, 1.0f, 0.0f);

        // 路径 -> 资源（强引用，引擎关闭时统一卸载）
        std::unordered_map<std::string, Ref<AudioClip>> s_clips;
        // 正在播放的声部（强引用，保证 fire-and-forget 的 Play 返回值也能播完）
        std::vector<Ref<SoundInstance>> s_activeSounds;
        // 所有仍存活的声部（弱引用，用于 Shutdown 时释放调用方还持有的句柄）
        std::vector<std::weak_ptr<SoundInstance>> s_liveSounds;

        std::string PathToUtf8(const std::filesystem::path& path)
        {
            const std::u8string u8 = path.u8string();
            std::string result;
            result.reserve(u8.size());
            for (const char8_t c : u8) {
                result.push_back(static_cast<char>(c));
            }
            return result;
        }

        std::filesystem::path PathFromUtf8(const std::string& utf8)
        {
            std::u8string u8;
            u8.reserve(utf8.size());
            for (const char c : utf8) {
                u8.push_back(static_cast<char8_t>(c));
            }
            return std::filesystem::path(u8);
        }

        // 规范化绝对路径作为缓存键（同一文件的不同相对写法只加载一次）
        std::filesystem::path NormalizePath(const std::string& filePath)
        {
            std::error_code ec;
            const std::filesystem::path absolutePath = std::filesystem::absolute(PathFromUtf8(filePath), ec);
            if (ec) {
                return PathFromUtf8(filePath);
            }
            return absolutePath.lexically_normal();
        }

        ma_attenuation_model ToMiniaudioAttenuation(AudioAttenuation model)
        {
            switch (model) {
                case AudioAttenuation::None:        return ma_attenuation_model_none;
                case AudioAttenuation::Inverse:     return ma_attenuation_model_inverse;
                case AudioAttenuation::Linear:      return ma_attenuation_model_linear;
                case AudioAttenuation::Exponential: return ma_attenuation_model_exponential;
            }
            return ma_attenuation_model_inverse;
        }

        void ApplyMasterVolume()
        {
            if (!s_initialized) {
                return;
            }
            ma_engine_set_volume(&s_engine, s_muted ? 0.0f : s_masterVolume);
        }

        // 回收已销毁声部的弱引用
        void PruneDeadSoundHandles()
        {
            s_liveSounds.erase(
                std::remove_if(s_liveSounds.begin(), s_liveSounds.end(),
                               [](const std::weak_ptr<SoundInstance>& handle) { return handle.expired(); }),
                s_liveSounds.end());
        }
    }

    struct SoundInstance::Impl {
        ma_sound sound{};
        bool initialized = false;
        bool stopped = false;
        bool paused = false;
        float volume = 1.0f;
        float duration = 0.0f;
    };

    struct AudioClip::Impl {
        ma_sound sound{};
        std::string path;
        float duration = 0.0f;
        bool initialized = false;
        bool streaming = false;
    };

    SoundInstance::SoundInstance()
        : m_impl(CreateScope<Impl>())
    {
    }

    SoundInstance::~SoundInstance()
    {
        Release();
    }

    void SoundInstance::Release()
    {
        if (!m_impl || !m_impl->initialized) {
            return;
        }

        ma_sound_stop(&m_impl->sound);
        ma_sound_uninit(&m_impl->sound);
        m_impl->initialized = false;
        m_impl->stopped = true;
    }

    void SoundInstance::Stop()
    {
        if (!m_impl || !m_impl->initialized || m_impl->stopped) {
            return;
        }

        ma_sound_stop(&m_impl->sound);
        ma_sound_seek_to_pcm_frame(&m_impl->sound, 0);
        m_impl->stopped = true;
        m_impl->paused = false;
    }

    void SoundInstance::Pause()
    {
        if (!m_impl || !m_impl->initialized || m_impl->stopped || m_impl->paused) {
            return;
        }

        ma_sound_stop(&m_impl->sound);
        m_impl->paused = true;
    }

    void SoundInstance::Resume()
    {
        if (!m_impl || !m_impl->initialized) {
            return;
        }

        // 已 Stop 的声部语义上是“结束”，再次 Resume 时从头播放
        if (m_impl->stopped) {
            ma_sound_seek_to_pcm_frame(&m_impl->sound, 0);
            m_impl->stopped = false;
        }

        m_impl->paused = false;
        ma_sound_start(&m_impl->sound);
    }

    bool SoundInstance::IsPlaying() const
    {
        if (!m_impl || !m_impl->initialized || m_impl->stopped || m_impl->paused) {
            return false;
        }
        return ma_sound_is_playing(&m_impl->sound) == MA_TRUE;
    }

    bool SoundInstance::IsPaused() const
    {
        return m_impl && m_impl->initialized && m_impl->paused;
    }

    bool SoundInstance::IsFinished() const
    {
        if (!m_impl || !m_impl->initialized || m_impl->stopped) {
            return true;
        }
        if (m_impl->paused) {
            return false;
        }
        return ma_sound_at_end(&m_impl->sound) == MA_TRUE;
    }

    void SoundInstance::SetVolume(float volume)
    {
        if (!m_impl || !m_impl->initialized) {
            return;
        }

        m_impl->volume = std::max(volume, 0.0f);
        ma_sound_set_volume(&m_impl->sound, m_impl->volume);
    }

    float SoundInstance::GetVolume() const
    {
        return m_impl ? m_impl->volume : 0.0f;
    }

    void SoundInstance::SetPitch(float pitch)
    {
        if (!m_impl || !m_impl->initialized || pitch <= 0.0f) {
            return;
        }

        ma_sound_set_pitch(&m_impl->sound, pitch);
    }

    void SoundInstance::SetLooping(bool looping)
    {
        if (!m_impl || !m_impl->initialized) {
            return;
        }

        ma_sound_set_looping(&m_impl->sound, looping ? MA_TRUE : MA_FALSE);
    }

    void SoundInstance::SetSpatializationEnabled(bool enabled)
    {
        if (!m_impl || !m_impl->initialized) {
            return;
        }

        ma_sound_set_spatialization_enabled(&m_impl->sound, enabled ? MA_TRUE : MA_FALSE);
    }

    void SoundInstance::SetPosition(const glm::vec3& position)
    {
        if (!m_impl || !m_impl->initialized) {
            return;
        }

        ma_sound_set_position(&m_impl->sound, position.x, position.y, position.z);
    }

    void SoundInstance::SetAttenuation(AudioAttenuation model, float minDistance, float maxDistance, float rolloff)
    {
        if (!m_impl || !m_impl->initialized) {
            return;
        }

        const float minDist = std::max(minDistance, 0.0f);
        const float maxDist = std::max(maxDistance, minDist);

        ma_sound_set_attenuation_model(&m_impl->sound, ToMiniaudioAttenuation(model));
        ma_sound_set_min_distance(&m_impl->sound, minDist);
        ma_sound_set_max_distance(&m_impl->sound, maxDist);
        ma_sound_set_rolloff(&m_impl->sound, std::max(rolloff, 0.0f));
    }

    void SoundInstance::SeekTo(float seconds)
    {
        if (!m_impl || !m_impl->initialized) {
            return;
        }

        const float clamped = std::clamp(seconds, 0.0f, std::max(m_impl->duration, 0.0f));
        ma_sound_seek_to_second(&m_impl->sound, clamped);
    }

    float SoundInstance::GetPlaybackPosition() const
    {
        if (!m_impl || !m_impl->initialized) {
            return 0.0f;
        }

        float cursor = 0.0f;
        if (ma_sound_get_cursor_in_seconds(&m_impl->sound, &cursor) != MA_SUCCESS) {
            return 0.0f;
        }
        return cursor;
    }

    float SoundInstance::GetDuration() const
    {
        return m_impl ? m_impl->duration : 0.0f;
    }

    AudioClip::AudioClip()
        : m_impl(CreateScope<Impl>())
    {
    }

    AudioClip::~AudioClip()
    {
        Release();
    }

    void AudioClip::Release()
    {
        if (!m_impl || !m_impl->initialized) {
            return;
        }

        ma_sound_uninit(&m_impl->sound);
        m_impl->initialized = false;
    }

    Ref<SoundInstance> AudioClip::Play(const AudioPlaybackSettings& settings)
    {
        return AudioEngine::PlayClip(*this, settings);
    }

    const std::string& AudioClip::GetPath() const
    {
        static const std::string empty;
        return m_impl ? m_impl->path : empty;
    }

    float AudioClip::GetDuration() const
    {
        return m_impl ? m_impl->duration : 0.0f;
    }

    bool AudioClip::IsValid() const
    {
        return m_impl && m_impl->initialized;
    }

    bool AudioClip::IsStreaming() const
    {
        return m_impl && m_impl->streaming;
    }

    bool AudioEngine::Init()
    {
        if (s_initialized) {
            return true;
        }

        ma_engine_config config = ma_engine_config_init();
        config.listenerCount = 1;

        // 上一次 Init 失败可能留下部分状态；miniaudio 要求在未初始化的对象上重新初始化
        s_engine = {};
        const ma_result result = ma_engine_init(&config, &s_engine);
        if (result != MA_SUCCESS) {
            OI_ERROR("Audio: ma_engine_init failed: {} ({})",
                     static_cast<int>(result), ma_result_description(result));
            return false;
        }

        s_initialized = true;
        s_masterVolume = 1.0f;
        s_muted = false;
        s_listenerPosition = glm::vec3(0.0f);
        s_listenerForward = glm::vec3(0.0f, 0.0f, -1.0f);
        s_listenerUp = glm::vec3(0.0f, 1.0f, 0.0f);

        ma_engine_listener_set_position(&s_engine, 0,
            s_listenerPosition.x, s_listenerPosition.y, s_listenerPosition.z);
        ma_engine_listener_set_direction(&s_engine, 0,
            s_listenerForward.x, s_listenerForward.y, s_listenerForward.z);
        ma_engine_listener_set_world_up(&s_engine, 0,
            s_listenerUp.x, s_listenerUp.y, s_listenerUp.z);
        ma_engine_set_volume(&s_engine, s_masterVolume);

        char deviceName[256] = {};
        ma_device* device = ma_engine_get_device(&s_engine);
        if (device != nullptr) {
            ma_device_get_name(device, ma_device_type_playback, deviceName, sizeof(deviceName), nullptr);
        }

        OI_INFO("Audio: miniaudio {} initialized (device '{}', {} Hz, {} ch)",
                ma_version_string(),
                deviceName[0] != '\0' ? deviceName : "default",
                ma_engine_get_sample_rate(&s_engine),
                ma_engine_get_channels(&s_engine));
        return true;
    }

    void AudioEngine::Shutdown()
    {
        if (!s_initialized) {
            return;
        }

        // 1. 先释放声部（调用方可能还持有 Ref，Release 后其句柄变为空操作）
        for (const std::weak_ptr<SoundInstance>& handle : s_liveSounds) {
            if (const Ref<SoundInstance> sound = handle.lock()) {
                sound->Release();
            }
        }
        s_liveSounds.clear();
        s_activeSounds.clear();

        // 2. 再释放预解码模板（调用方持有的 AudioClip 同样变为无效）
        for (const auto& entry : s_clips) {
            entry.second->Release();
        }
        s_clips.clear();

        // 3. 最后销毁 miniaudio 引擎
        ma_engine_uninit(&s_engine);
        s_initialized = false;

        OI_INFO("Audio: shut down");
    }

    bool AudioEngine::IsInitialized()
    {
        return s_initialized;
    }

    void AudioEngine::Update()
    {
        if (!s_initialized) {
            return;
        }

        for (auto it = s_activeSounds.begin(); it != s_activeSounds.end();) {
            const Ref<SoundInstance>& sound = *it;
            if (!sound) {
                it = s_activeSounds.erase(it);
                continue;
            }

            if (sound->IsFinished()) {
                // 只有引擎还持有时才释放；调用方仍持有句柄则保留，允许其 Resume / SeekTo
                if (sound.use_count() == 1) {
                    sound->Release();
                }
                it = s_activeSounds.erase(it);
            } else if (sound->IsPaused() && sound.use_count() == 1) {
                // 暂停后丢弃句柄的声部永远不会再播放，及时回收
                sound->Release();
                it = s_activeSounds.erase(it);
            } else {
                ++it;
            }
        }

        PruneDeadSoundHandles();
    }

    Ref<AudioClip> AudioEngine::LoadClip(const std::string& filePath, bool streaming)
    {
        if (!s_initialized) {
            OI_ERROR("Audio: LoadClip('{}') called before AudioEngine::Init()", filePath);
            return nullptr;
        }

        const std::filesystem::path normalizedPath = NormalizePath(filePath);
        const std::string pathUtf8 = PathToUtf8(normalizedPath);
        const std::string cacheKey = (streaming ? "stream:" : "decode:") + pathUtf8;
        const auto cached = s_clips.find(cacheKey);
        if (cached != s_clips.end()) {
            return cached->second;
        }

        const Ref<AudioClip> clip = Ref<AudioClip>(new AudioClip());
        AudioClip::Impl& impl = *clip->m_impl;
        impl.path = pathUtf8;
        impl.streaming = streaming;

        // DECODE：启动时完整解码到 PCM，之后每次 Play 用 ma_sound_init_copy 共享数据，避免重复解码。
        // STREAM：不预解码，适合长 BGM；Play 时为每个声部单独打开解码流。
        // NO_DEFAULT_ATTACHMENT：模板声部本身不接入混音图，只作为克隆源 / 元数据载体。
        // Windows 下 miniaudio 的窄字符文件接口使用 ANSI fopen，非 ASCII 路径必须走宽字符版本。
        const ma_uint32 flags = (streaming ? MA_SOUND_FLAG_STREAM : MA_SOUND_FLAG_DECODE) |
                                MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT;
#if defined(OI_PLATFORM_WINDOWS)
        const ma_result result = ma_sound_init_from_file_w(
            &s_engine, normalizedPath.c_str(), flags, nullptr, nullptr, &impl.sound);
#else
        const ma_result result = ma_sound_init_from_file(
            &s_engine, pathUtf8.c_str(), flags, nullptr, nullptr, &impl.sound);
#endif

        if (result != MA_SUCCESS) {
            OI_ERROR("Audio: failed to load '{}': {} ({})",
                     pathUtf8, static_cast<int>(result), ma_result_description(result));
            return nullptr;
        }

        impl.initialized = true;
        ma_sound_get_length_in_seconds(&impl.sound, &impl.duration);

        ma_format format = ma_format_unknown;
        ma_uint32 channels = 0;
        ma_uint32 sampleRate = 0;
        ma_sound_get_data_format(&impl.sound, &format, &channels, &sampleRate, nullptr, 0);

        OI_INFO("Audio: loaded '{}' ({:.2f}s, {} Hz, {} ch, {})",
                pathUtf8, impl.duration, sampleRate, channels, streaming ? "stream" : "decode");

        s_clips.emplace(cacheKey, clip);
        return clip;
    }

    void AudioEngine::StopAll()
    {
        for (const Ref<SoundInstance>& sound : s_activeSounds) {
            if (sound) {
                sound->Stop();
            }
        }
    }

    void AudioEngine::SetMasterVolume(float volume)
    {
        s_masterVolume = std::clamp(volume, 0.0f, 1.0f);
        ApplyMasterVolume();
    }

    float AudioEngine::GetMasterVolume()
    {
        return s_masterVolume;
    }

    void AudioEngine::SetMuted(bool muted)
    {
        s_muted = muted;
        ApplyMasterVolume();
    }

    bool AudioEngine::IsMuted()
    {
        return s_muted;
    }

    void AudioEngine::SetListenerPosition(const glm::vec3& position)
    {
        s_listenerPosition = position;
        if (s_initialized) {
            ma_engine_listener_set_position(&s_engine, 0, position.x, position.y, position.z);
        }
    }

    void AudioEngine::SetListenerDirection(const glm::vec3& forward, const glm::vec3& up)
    {
        if (glm::dot(forward, forward) < 1e-8f) {
            OI_WARN("Audio: ignored zero listener forward vector");
            return;
        }
        if (glm::dot(up, up) < 1e-8f) {
            OI_WARN("Audio: ignored zero listener up vector");
            return;
        }

        s_listenerForward = glm::normalize(forward);
        s_listenerUp = glm::normalize(up);

        if (s_initialized) {
            ma_engine_listener_set_direction(&s_engine, 0,
                s_listenerForward.x, s_listenerForward.y, s_listenerForward.z);
            ma_engine_listener_set_world_up(&s_engine, 0,
                s_listenerUp.x, s_listenerUp.y, s_listenerUp.z);
        }
    }

    glm::vec3 AudioEngine::GetListenerPosition()
    {
        return s_listenerPosition;
    }

    glm::vec3 AudioEngine::GetListenerDirection()
    {
        return s_listenerForward;
    }

    Ref<SoundInstance> AudioEngine::PlayClip(const AudioClip& clip, const AudioPlaybackSettings& settings)
    {
        if (!s_initialized || !clip.m_impl || !clip.m_impl->initialized) {
            OI_WARN("Audio: cannot play an invalid clip (engine initialized: {})", s_initialized);
            return nullptr;
        }
        if (settings.pitch <= 0.0f) {
            OI_WARN("Audio: invalid pitch {} for '{}'", settings.pitch, clip.m_impl->path);
            return nullptr;
        }

        const Ref<SoundInstance> instance = Ref<SoundInstance>(new SoundInstance());
        SoundInstance::Impl& impl = *instance->m_impl;

        ma_result result = MA_SUCCESS;
        if (clip.m_impl->streaming) {
            // 流式 clip 不能 init_copy：每个声部独立打开一条解码流
            const std::filesystem::path nativePath = PathFromUtf8(clip.m_impl->path);
#if defined(OI_PLATFORM_WINDOWS)
            result = ma_sound_init_from_file_w(
                &s_engine, nativePath.c_str(), MA_SOUND_FLAG_STREAM, nullptr, nullptr, &impl.sound);
#else
            result = ma_sound_init_from_file(
                &s_engine, clip.m_impl->path.c_str(), MA_SOUND_FLAG_STREAM, nullptr, nullptr, &impl.sound);
#endif
        } else {
            // 从预解码模板克隆：数据源共享 PCM，声部拥有独立游标 / 音量 / 空间参数
            result = ma_sound_init_copy(&s_engine, &clip.m_impl->sound, 0, nullptr, &impl.sound);
        }

        if (result != MA_SUCCESS) {
            OI_ERROR("Audio: failed to create playback for '{}': {} ({})",
                     clip.m_impl->path, static_cast<int>(result), ma_result_description(result));
            return nullptr;
        }

        impl.initialized = true;
        impl.duration = clip.m_impl->duration;
        impl.volume = std::max(settings.volume, 0.0f);

        const float pitch = settings.pitch;
        const bool looping = settings.looping;
        const bool spatialized = settings.spatialized;

        ma_sound_set_volume(&impl.sound, impl.volume);
        ma_sound_set_pitch(&impl.sound, pitch);
        ma_sound_set_looping(&impl.sound, looping ? MA_TRUE : MA_FALSE);
        ma_sound_set_spatialization_enabled(&impl.sound, spatialized ? MA_TRUE : MA_FALSE);

        if (spatialized) {
            const float minDistance = std::max(settings.minDistance, 0.0f);
            const float maxDistance = std::max(settings.maxDistance, minDistance);

            ma_sound_set_position(&impl.sound,
                settings.position.x, settings.position.y, settings.position.z);
            ma_sound_set_attenuation_model(&impl.sound, ToMiniaudioAttenuation(settings.attenuation));
            ma_sound_set_min_distance(&impl.sound, minDistance);
            ma_sound_set_max_distance(&impl.sound, maxDistance);
            ma_sound_set_rolloff(&impl.sound, std::max(settings.rolloff, 0.0f));
        }

        if (ma_sound_start(&impl.sound) != MA_SUCCESS) {
            OI_ERROR("Audio: failed to start '{}'", clip.m_impl->path);
            instance->Release();
            return nullptr;
        }

        s_activeSounds.push_back(instance);
        s_liveSounds.push_back(instance);
        return instance;
    }
}
