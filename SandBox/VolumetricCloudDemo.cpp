#include "VolumetricCloudDemo.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <thread>
#include <vector>

#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>

#include "Engine/Core/Input.h"
#include "Engine/Events/KeyEvent.h"

using namespace OiEngine;

namespace {
    constexpr float PI = 3.14159265358979323846f;
    constexpr float TERRAIN_SIZE = 70000.0f;    // 地形边长（米）
    constexpr int TERRAIN_SEGMENTS = 900;       // 网格分段（约 78 米一格）
    constexpr int NOISE_SIZE = 64;              // 3D 形状噪声分辨率
    constexpr int WEATHER_SIZE = 512;           // 2D 天气图分辨率

    // ---------- 基础噪声（全部可平铺，供 CPU 生成纹理 / 地形使用） ----------

    float SmoothStep(float edge0, float edge1, float x)
    {
        const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    float Lerp(float a, float b, float t) { return a + (b - a) * t; }

    uint32_t HashUInt(uint32_t x)
    {
        x ^= x >> 16;
        x *= 0x7feb352du;
        x ^= x >> 15;
        x *= 0x846ca68bu;
        x ^= x >> 16;
        return x;
    }

    uint32_t Hash3(int x, int y, int z, uint32_t seed)
    {
        uint32_t h = seed ^ HashUInt(static_cast<uint32_t>(x) + 0x9e3779b9u);
        h = HashUInt(h ^ (static_cast<uint32_t>(y) + 0x85ebca6bu));
        h = HashUInt(h ^ (static_cast<uint32_t>(z) + 0xc2b2ae35u));
        return h;
    }

    float HashToUnit(uint32_t h)
    {
        return static_cast<float>(h & 0x00ffffffu) / 16777215.0f;
    }

    int WrapInt(int value, int period)
    {
        const int r = value % period;
        return r < 0 ? r + period : r;
    }

    float Fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

    glm::vec2 Gradient2(uint32_t h)
    {
        switch (h % 8u) {
            case 0: return { 1.0f, 0.0f };
            case 1: return { -1.0f, 0.0f };
            case 2: return { 0.0f, 1.0f };
            case 3: return { 0.0f, -1.0f };
            case 4: return { 0.70710678f, 0.70710678f };
            case 5: return { -0.70710678f, 0.70710678f };
            case 6: return { 0.70710678f, -0.70710678f };
            default: return { -0.70710678f, -0.70710678f };
        }
    }

    // 可平铺的 2D 梯度噪声（period 控制循环周期）
    float Perlin2(float x, float y, int period, uint32_t seed)
    {
        const int x0 = static_cast<int>(std::floor(x));
        const int y0 = static_cast<int>(std::floor(y));
        const float fx = x - static_cast<float>(x0);
        const float fy = y - static_cast<float>(y0);

        const auto corner = [&](int ix, int iy, float dx, float dy) {
            const uint32_t h = Hash3(WrapInt(ix, period), WrapInt(iy, period), 0, seed);
            return glm::dot(Gradient2(h), glm::vec2(dx, dy));
        };

        const float n00 = corner(x0, y0, fx, fy);
        const float n10 = corner(x0 + 1, y0, fx - 1.0f, fy);
        const float n01 = corner(x0, y0 + 1, fx, fy - 1.0f);
        const float n11 = corner(x0 + 1, y0 + 1, fx - 1.0f, fy - 1.0f);

        const float u = Fade(fx);
        const float v = Fade(fy);
        return Lerp(Lerp(n00, n10, u), Lerp(n01, n11, u), v);
    }

    float FbmPerlin2(const glm::vec2& p, int basePeriod, int octaves, uint32_t seed)
    {
        float sum = 0.0f;
        float amplitude = 0.5f;
        float norm = 0.0f;
        int period = basePeriod;

        for (int i = 0; i < octaves; ++i) {
            sum += amplitude * Perlin2(p.x * static_cast<float>(period), p.y * static_cast<float>(period),
                                       period, seed + static_cast<uint32_t>(i) * 131u);
            norm += amplitude;
            amplitude *= 0.5f;
            period *= 2;
        }

        return norm > 0.0f ? sum / norm : 0.0f;
    }

    glm::vec3 Gradient3(uint32_t h)
    {
        switch (h % 12u) {
            case 0: return { 1.0f, 1.0f, 0.0f };
            case 1: return { -1.0f, 1.0f, 0.0f };
            case 2: return { 1.0f, -1.0f, 0.0f };
            case 3: return { -1.0f, -1.0f, 0.0f };
            case 4: return { 1.0f, 0.0f, 1.0f };
            case 5: return { -1.0f, 0.0f, 1.0f };
            case 6: return { 1.0f, 0.0f, -1.0f };
            case 7: return { -1.0f, 0.0f, -1.0f };
            case 8: return { 0.0f, 1.0f, 1.0f };
            case 9: return { 0.0f, -1.0f, 1.0f };
            case 10: return { 0.0f, 1.0f, -1.0f };
            default: return { 0.0f, -1.0f, -1.0f };
        }
    }

    float Perlin3(float x, float y, float z, int period, uint32_t seed)
    {
        const int x0 = static_cast<int>(std::floor(x));
        const int y0 = static_cast<int>(std::floor(y));
        const int z0 = static_cast<int>(std::floor(z));
        const float fx = x - static_cast<float>(x0);
        const float fy = y - static_cast<float>(y0);
        const float fz = z - static_cast<float>(z0);

        const auto corner = [&](int ix, int iy, int iz, float dx, float dy, float dz) {
            const uint32_t h = Hash3(WrapInt(ix, period), WrapInt(iy, period), WrapInt(iz, period), seed);
            return glm::dot(Gradient3(h), glm::vec3(dx, dy, dz));
        };

        const float n000 = corner(x0, y0, z0, fx, fy, fz);
        const float n100 = corner(x0 + 1, y0, z0, fx - 1.0f, fy, fz);
        const float n010 = corner(x0, y0 + 1, z0, fx, fy - 1.0f, fz);
        const float n110 = corner(x0 + 1, y0 + 1, z0, fx - 1.0f, fy - 1.0f, fz);
        const float n001 = corner(x0, y0, z0 + 1, fx, fy, fz - 1.0f);
        const float n101 = corner(x0 + 1, y0, z0 + 1, fx - 1.0f, fy, fz - 1.0f);
        const float n011 = corner(x0, y0 + 1, z0 + 1, fx, fy - 1.0f, fz - 1.0f);
        const float n111 = corner(x0 + 1, y0 + 1, z0 + 1, fx - 1.0f, fy - 1.0f, fz - 1.0f);

        const float u = Fade(fx);
        const float v = Fade(fy);
        const float w = Fade(fz);
        const float nx00 = Lerp(n000, n100, u);
        const float nx10 = Lerp(n010, n110, u);
        const float nx01 = Lerp(n001, n101, u);
        const float nx11 = Lerp(n011, n111, u);
        return Lerp(Lerp(nx00, nx10, v), Lerp(nx01, nx11, v), w);
    }

    float FbmPerlin3(const glm::vec3& p, int basePeriod, int octaves, uint32_t seed)
    {
        float sum = 0.0f;
        float amplitude = 0.5f;
        float norm = 0.0f;
        int period = basePeriod;

        for (int i = 0; i < octaves; ++i) {
            sum += amplitude * Perlin3(p.x * static_cast<float>(period), p.y * static_cast<float>(period),
                                       p.z * static_cast<float>(period), period,
                                       seed + static_cast<uint32_t>(i) * 197u);
            norm += amplitude;
            amplitude *= 0.5f;
            period *= 2;
        }

        return norm > 0.0f ? sum / norm : 0.0f;
    }

    // 可平铺 Worley（F1），返回最近特征点距离（以 cell 为单位）
    float Worley3(const glm::vec3& p, int cells, uint32_t seed)
    {
        const glm::vec3 pc = p * static_cast<float>(cells);
        const int cx = static_cast<int>(std::floor(pc.x));
        const int cy = static_cast<int>(std::floor(pc.y));
        const int cz = static_cast<int>(std::floor(pc.z));

        float minDistance = 1e9f;
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    const uint32_t h = Hash3(WrapInt(cx + dx, cells), WrapInt(cy + dy, cells),
                                             WrapInt(cz + dz, cells), seed);
                    const glm::vec3 jitter(HashToUnit(h),
                                           HashToUnit(HashUInt(h + 0x9e3779b9u)),
                                           HashToUnit(HashUInt(h + 0x85ebca6bu)));
                    const glm::vec3 feature = glm::vec3(static_cast<float>(cx + dx),
                                                        static_cast<float>(cy + dy),
                                                        static_cast<float>(cz + dz)) + jitter;
                    minDistance = std::min(minDistance, glm::length(pc - feature));
                }
            }
        }

        return minDistance;
    }

    float InvertedWorley3(const glm::vec3& p, int cells, uint32_t seed)
    {
        return 1.0f - std::clamp(Worley3(p, cells, seed), 0.0f, 1.0f);
    }

    // ---------- 地形高度场 ----------

    float RidgedFbm2(const glm::vec2& p, int basePeriod, int octaves, uint32_t seed)
    {
        float sum = 0.0f;
        float amplitude = 0.5f;
        float norm = 0.0f;
        float weight = 1.0f;
        int period = basePeriod;

        for (int i = 0; i < octaves; ++i) {
            float n = 1.0f - std::fabs(Perlin2(p.x * static_cast<float>(period), p.y * static_cast<float>(period),
                                                period, seed + static_cast<uint32_t>(i) * 73u));
            n *= n;
            n *= weight;
            weight = std::clamp(n * 2.6f, 0.0f, 1.0f);
            sum += n * amplitude;
            norm += amplitude;
            amplitude *= 0.55f;
            period *= 2;
        }

        return norm > 0.0f ? sum / norm : 0.0f;
    }

    float TerrainHeight(float x, float z)
    {
        const glm::vec2 p(x, z);

        // 低频域扭曲：让山脉走向不规则，而不是噪声网格的方块感
        const glm::vec2 warp(
            FbmPerlin2(p * 0.00007f, 4, 4, 17u),
            FbmPerlin2(p * 0.00007f + glm::vec2(19.3f, 7.1f), 4, 4, 29u));
        const glm::vec2 q = p + warp * 6500.0f;

        const float continental = 0.5f + 0.5f * FbmPerlin2(q * 0.000045f, 3, 4, 41u);
        const float mask = SmoothStep(0.44f, 0.76f, continental);
        const float ridge = RidgedFbm2(q * 0.00024f, 4, 4, 53u);
        const float detail = 0.5f + 0.5f * FbmPerlin2(q * 0.0009f, 4, 4, 67u);

        return 40.0f + mask * (520.0f + 2350.0f * ridge) + mask * 140.0f * detail;
    }

    // ---------- 着色器 ----------

    const char* TERRAIN_VERTEX_SHADER = R"(
        #version 450 core

        layout(location = 0) in vec3 a_Position;
        layout(location = 1) in vec3 a_Normal;
        layout(location = 2) in vec4 a_Color;

        layout(std140, binding = 0) uniform Camera {
            mat4 u_Projection;
            mat4 u_View;
            vec4 u_CameraPosition;
        };

        out vec3 v_WorldPosition;
        out vec3 v_Normal;
        out vec4 v_Color;

        void main()
        {
            v_WorldPosition = a_Position;
            v_Normal = a_Normal;
            v_Color = a_Color;
            gl_Position = u_Projection * u_View * vec4(a_Position, 1.0);
        }
    )";

    const char* TERRAIN_FRAGMENT_SHADER = R"(
        #version 450 core

        layout(location = 0) out vec4 o_Color;

        in vec3 v_WorldPosition;
        in vec3 v_Normal;
        in vec4 v_Color;

        layout(std140, binding = 0) uniform Camera {
            mat4 u_Projection;
            mat4 u_View;
            vec4 u_CameraPosition;
        };

        uniform vec3 u_SunDirection;     // 指向太阳
        uniform vec3 u_SunColor;
        uniform float u_SunIntensity;
        uniform vec3 u_SkyAmbient;
        uniform vec3 u_HazeColor;
        uniform vec3 u_SunHazeColor;
        uniform float u_FogDensity;
        uniform float u_CloudBase;
        uniform float u_CloudTop;
        uniform vec2 u_WeatherOffset;
        uniform sampler2D u_WeatherMap;

        void main()
        {
            vec3 N = normalize(v_Normal);
            vec3 toSun = normalize(u_SunDirection);
            float ndl = max(dot(N, toSun), 0.0);

            // 云影：用天气图的覆盖率粗略代替云阴影贴图（低于云顶的地形被云遮挡）
            float cloudShadow = 1.0;
            if (v_WorldPosition.y < u_CloudTop) {
                float coverage = texture(u_WeatherMap, v_WorldPosition.xz / 26000.0 + u_WeatherOffset).r;
                float below = 1.0 - smoothstep(u_CloudTop - 350.0, u_CloudTop + 100.0, v_WorldPosition.y);
                cloudShadow = 1.0 - 0.48 * smoothstep(0.45, 0.85, coverage) * below;
            }

            vec3 ambient = u_SkyAmbient * (0.55 + 0.45 * clamp(N.y, 0.0, 1.0));
            vec3 color = v_Color.rgb * (u_SunColor * u_SunIntensity * ndl * cloudShadow + ambient);

            vec3 toCamera = u_CameraPosition.xyz - v_WorldPosition;
            float distanceToCamera = length(toCamera);
            vec3 viewDir = toCamera / max(distanceToCamera, 0.001);
            float fog = 1.0 - exp(-distanceToCamera * u_FogDensity);
            float sunAmount = pow(max(dot(viewDir, toSun), 0.0), 3.0);
            vec3 haze = mix(u_HazeColor, u_SunHazeColor, sunAmount * 0.65);
            color = mix(color, haze, fog);

            o_Color = vec4(color, 1.0);
        }
    )";

    const char* FULLSCREEN_VERTEX_SHADER = R"(
        #version 450 core

        layout(location = 0) in vec2 a_Position;

        out vec2 v_Ndc;

        void main()
        {
            v_Ndc = a_Position;
            gl_Position = vec4(a_Position, 0.0, 1.0);
        }
    )";

    const char* CLOUD_FRAGMENT_SHADER = R"(
        #version 450 core

        layout(location = 0) out vec4 o_Cloud;   // rgb: 预乘的云散射；a: 透过率

        in vec2 v_Ndc;

        uniform sampler2D u_SceneDepth;
        uniform sampler2D u_WeatherMap;
        uniform sampler3D u_ShapeNoise;

        uniform mat4 u_InvViewProj;
        uniform vec3 u_CameraPosition;
        uniform vec3 u_SunDirection;
        uniform vec3 u_SunColor;
        uniform float u_SunIntensity;
        uniform vec3 u_SkyZenith;
        uniform vec3 u_SkyHorizon;
        uniform vec3 u_HazeColor;
        uniform float u_FogDensity;

        uniform float u_Coverage;
        uniform float u_Density;
        uniform float u_CloudBase;
        uniform float u_CloudTop;
        uniform float u_Extinction;
        uniform float u_DetailStrength;
        uniform float u_Ambient;
        uniform vec2 u_WindOffset;
        uniform float u_Time;
        uniform vec2 u_Resolution;
        uniform float u_Frame;
        uniform int u_Steps;
        uniform int u_LightSteps;

        const float PI = 3.141592653589793;

        float Hg(float cosTheta, float g)
        {
            float g2 = g * g;
            float denom = max(1.0 + g2 - 2.0 * g * cosTheta, 0.0001);
            return (1.0 - g2) / (4.0 * PI * pow(denom, 1.5));
        }

        float SampleDensity(vec3 position, bool detail)
        {
            float heightFraction = (position.y - u_CloudBase) / (u_CloudTop - u_CloudBase);
            if (heightFraction <= 0.0 || heightFraction >= 1.0) {
                return 0.0;
            }

            vec4 weather = texture(u_WeatherMap, position.xz / 26000.0 + u_WindOffset);
            float meso = texture(u_WeatherMap, (position.xz / 26000.0 + u_WindOffset) * 3.1 +
                                vec2(0.41, 0.17)).g;
            float coverageNoise = weather.r * 0.78 + meso * 0.22;
            float cloudMask = smoothstep(1.0 - u_Coverage, 1.0 - u_Coverage + 0.38, coverageNoise);
            if (cloudMask <= 0.005) {
                return 0.0;
            }

            // 云顶/云底各用一张二维高度场（天气图的塔高 + 噪声起伏），
            // 中间是厚实的体积，云顶不会被高度层削平
            float tower = smoothstep(0.25, 0.85, weather.g);
            float topNoise = texture(u_ShapeNoise, vec3(position.x, 0.37, position.z) / 3200.0).a;
            float topErosion = texture(u_ShapeNoise, position / 1300.0 +
                                       vec3(u_Time * 0.004, 0.0, 0.0)).g;
            float topField = mix(u_CloudBase + 550.0, u_CloudTop - 550.0, tower);
            topField += (topNoise - 0.5) * 1100.0;
            topField += (topErosion - 0.5) * 900.0;
            topField = clamp(topField, u_CloudBase + 300.0, u_CloudTop - 120.0);
            float baseField = u_CloudBase + (topNoise - 0.5) * 130.0;

            float vertical = smoothstep(baseField, baseField + 220.0, position.y) *
                            (1.0 - smoothstep(topField - 520.0, topField, position.y));
            if (vertical <= 0.002) {
                return 0.0;
            }

            // 3D 噪声负责体积内部的疏密变化与边缘丝缕
            float shape = texture(u_ShapeNoise, position / 1700.0 +
                                  vec3(u_Time * 0.003, 0.0, 0.0)).r;
            shape = clamp((shape - 0.46) * 2.4, 0.0, 1.0);
            float density = vertical * cloudMask * (0.45 + 0.55 * shape);

            if (detail) {
                // 用低频 Perlin 做柔和的边缘侵蚀；旧版高频 Worley 会在云面留下明显颗粒
                float high = texture(u_ShapeNoise, position / 700.0 +
                                     vec3(u_Time * 0.004, 0.0, u_Time * 0.002)).a;
                float erosion = smoothstep(0.10, 0.62, high);
                density *= mix(1.0, erosion, u_DetailStrength);
            }

            return density * u_Density;
        }

        // 朝太阳的光步进只需要高度场，省掉 3D 细节
        float SampleDensityCheap(vec3 position)
        {
            float heightFraction = (position.y - u_CloudBase) / (u_CloudTop - u_CloudBase);
            if (heightFraction <= 0.0 || heightFraction >= 1.0) {
                return 0.0;
            }

            vec4 weather = texture(u_WeatherMap, position.xz / 26000.0 + u_WindOffset);
            float meso = texture(u_WeatherMap, (position.xz / 26000.0 + u_WindOffset) * 3.1 +
                                vec2(0.41, 0.17)).g;
            float coverageNoise = weather.r * 0.78 + meso * 0.22;
            float cloudMask = smoothstep(1.0 - u_Coverage, 1.0 - u_Coverage + 0.38, coverageNoise);
            if (cloudMask <= 0.005) {
                return 0.0;
            }

            float tower = smoothstep(0.25, 0.85, weather.g);
            float topNoise = texture(u_ShapeNoise, vec3(position.x, 0.37, position.z) / 3200.0).a;
            float topErosion = texture(u_ShapeNoise, position / 1300.0 +
                                       vec3(u_Time * 0.004, 0.0, 0.0)).g;
            float topField = mix(u_CloudBase + 550.0, u_CloudTop - 550.0, tower);
            topField += (topNoise - 0.5) * 1100.0;
            topField += (topErosion - 0.5) * 900.0;
            topField = clamp(topField, u_CloudBase + 300.0, u_CloudTop - 120.0);
            float baseField = u_CloudBase + (topNoise - 0.5) * 130.0;

            float vertical = smoothstep(baseField, baseField + 220.0, position.y) *
                            (1.0 - smoothstep(topField - 520.0, topField, position.y));
            return vertical * cloudMask * 0.75 * u_Density;
        }

        float LightMarch(vec3 position)
        {
            float opticalDepth = 0.0;
            float stepSize = 110.0;
            vec3 p = position;

            for (int i = 0; i < 8; ++i) {
                if (i >= u_LightSteps) {
                    break;
                }
                p += u_SunDirection * stepSize;
                opticalDepth += SampleDensityCheap(p) * stepSize;
                stepSize *= 1.45;
            }

            // 0.42 是多次散射的经验补偿，避免云内部完全死黑
            return exp(-opticalDepth * u_Extinction * 0.42);
        }

        float InterleavedGradientNoise(vec2 pixel)
        {
            return fract(52.9829189 * fract(dot(pixel, vec2(0.06711056, 0.00583715))));
        }

        void main()
        {
            vec2 uv = gl_FragCoord.xy / u_Resolution;
            float depth = texture(u_SceneDepth, uv).r;

            vec4 ndc = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
            vec4 world = u_InvViewProj * ndc;
            vec3 worldPosition = world.xyz / world.w;
            vec3 rayDir = normalize(worldPosition - u_CameraPosition);

            float sceneDistance = depth >= 0.99999 ? 1e9 : length(worldPosition - u_CameraPosition);

            // 与云层平板（slab）求交
            float t0 = 0.0;
            float t1 = 0.0;
            if (abs(rayDir.y) < 1e-5) {
                if (u_CameraPosition.y < u_CloudBase || u_CameraPosition.y > u_CloudTop) {
                    o_Cloud = vec4(0.0, 0.0, 0.0, 1.0);
                    return;
                }
                t0 = 0.0;
                t1 = 60000.0;
            } else {
                float ta = (u_CloudBase - u_CameraPosition.y) / rayDir.y;
                float tb = (u_CloudTop - u_CameraPosition.y) / rayDir.y;
                t0 = max(min(ta, tb), 0.0);
                t1 = max(ta, tb);
            }

            t1 = min(t1, min(sceneDistance, 60000.0));
            if (t1 <= t0 + 1.0) {
                o_Cloud = vec4(0.0, 0.0, 0.0, 1.0);
                return;
            }

            float jitter = fract(InterleavedGradientNoise(gl_FragCoord.xy) + u_Frame * 0.61803398875);
            float transmittance = 1.0;
            vec3 scattered = vec3(0.0);

            float cosTheta = dot(rayDir, u_SunDirection);
            float phase = mix(Hg(cosTheta, 0.72), Hg(cosTheta, -0.22), 0.34);

            float previousT = t0;
            for (int i = 0; i < 256; ++i) {
                if (i >= u_Steps || transmittance < 0.015) {
                    break;
                }

                float s = (float(i) + jitter) / float(u_Steps);
                // 近处采样更密：指数 1.45 让靠近相机的云有足够细节
                float t = t0 + (t1 - t0) * pow(s, 1.70);
                float stepLength = t - previousT;
                previousT = t;
                if (stepLength <= 0.0) {
                    continue;
                }

                vec3 p = u_CameraPosition + rayDir * t;
                float density = SampleDensity(p, t < 9000.0);
                if (density <= 0.002) {
                    continue;
                }

                float lightTransmittance = LightMarch(p);
                lightTransmittance = pow(lightTransmittance, 0.60);

                float powder = 1.0 - exp(-density * 9.0);
                vec3 sunLight = u_SunColor * u_SunIntensity * lightTransmittance *
                                phase * mix(0.35, 1.0, powder);

                float hf = clamp((p.y - u_CloudBase) / (u_CloudTop - u_CloudBase), 0.0, 1.0);
                vec3 ambient = u_SkyZenith * u_Ambient * (0.45 + 0.85 * hf);
                ambient += u_SkyHorizon * u_Ambient * 0.16 * (1.0 - hf);

                float sigma = density * u_Extinction;
                float sampleTransmittance = exp(-sigma * stepLength);
                vec3 radiance = sunLight + ambient;
                // 远处云层融进大气雾，避免地平线出现一条死白的云墙
                float aerial = 1.0 - exp(-t * u_FogDensity);
                radiance = mix(radiance, u_HazeColor * 1.1, aerial * 0.9);
                scattered += transmittance * radiance * (1.0 - sampleTransmittance);
                transmittance *= sampleTransmittance;
            }

            o_Cloud = vec4(scattered, transmittance);
        }
    )";

    const char* COMPOSITE_FRAGMENT_SHADER = R"(
        #version 450 core

        layout(location = 0) out vec4 o_Color;

        in vec2 v_Ndc;

        uniform sampler2D u_SceneColor;
        uniform sampler2D u_CloudTexture;
        uniform sampler2D u_SceneDepth;
        uniform mat4 u_InvViewProj;
        uniform vec3 u_CameraPosition;
        uniform vec3 u_SunDirection;
        uniform vec3 u_SunColor;
        uniform float u_SunIntensity;
        uniform vec3 u_SkyZenith;
        uniform vec3 u_SkyHorizon;
        uniform vec3 u_HazeColor;
        uniform float u_Exposure;
        uniform float u_Denoise;
        uniform int u_DebugView;
        uniform vec2 u_Resolution;

        vec3 AcesFilm(vec3 color)
        {
            const float a = 2.51;
            const float b = 0.03;
            const float c = 2.43;
            const float d = 0.59;
            const float e = 0.14;
            return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
        }

        vec3 SkyColor(vec3 dir)
        {
            float height = max(dir.y, 0.0);
            // 地平线附近保留一条暖色带，向上很快过渡到深蓝天顶
            vec3 sky = mix(u_SkyHorizon, u_SkyZenith, smoothstep(0.0, 0.26, height));

            float mu = max(dot(dir, u_SunDirection), 0.0);
            float horizonBand = exp(-height * 12.0);
            sky += u_SunColor * horizonBand * pow(mu, 2.0) * 0.32;
            sky += u_SunColor * pow(mu, 18.0) * 0.24;
            sky += u_SunColor * (pow(mu, 8.0) * 0.20 + pow(mu, 64.0) * 0.55) * 0.32;

            float sunDisc = smoothstep(0.99965, 0.99990, mu);
            sky += u_SunColor * sunDisc * u_SunIntensity * 2.0;

            if (dir.y < 0.0) {
                sky = mix(u_HazeColor, u_SkyHorizon, exp(-abs(dir.y) * 10.0));
            }

            return sky;
        }

        // 深度感知 5x5 高斯：抹平步进抖动与高频侵蚀的颗粒，同时不让云抹到更近的山脊上
        vec4 SampleCloudDenoised(vec2 uv)
        {
            vec4 center = texture(u_CloudTexture, uv);
            if (u_Denoise <= 0.001) {
                return center;
            }

            vec2 texel = 1.0 / vec2(textureSize(u_CloudTexture, 0));
            float centerDepth = texture(u_SceneDepth, uv).r;
            vec4 sum = vec4(0.0);
            float weightSum = 0.0;

            for (int y = -2; y <= 2; ++y) {
                for (int x = -2; x <= 2; ++x) {
                    vec2 offset = vec2(float(x), float(y)) * texel;
                    float weight = exp(-float(x * x + y * y) * 0.35);

                    // 邻域比中心更近：那是另一个表面（山脊边缘），不要混进来
                    float tapDepth = texture(u_SceneDepth, uv + offset).r;
                    if (tapDepth < centerDepth - 0.0001) {
                        weight *= 0.1;
                    }

                    sum += texture(u_CloudTexture, uv + offset) * weight;
                    weightSum += weight;
                }
            }

            return mix(center, sum / weightSum, u_Denoise);
        }

        void main()
        {
            vec2 uv = gl_FragCoord.xy / u_Resolution;
            float depth = texture(u_SceneDepth, uv).r;

            vec4 farNdc = vec4(uv * 2.0 - 1.0, 1.0, 1.0);
            vec4 farWorld = u_InvViewProj * farNdc;
            vec3 rayDir = normalize(farWorld.xyz / farWorld.w - u_CameraPosition);

            vec3 sceneColor = texture(u_SceneColor, uv).rgb;
            vec3 base = depth >= 0.99999 ? SkyColor(rayDir) : sceneColor;

            vec4 cloud = SampleCloudDenoised(uv);
            vec3 color = base * cloud.a + cloud.rgb;

            if (u_DebugView == 1) {
                color = vec3(1.0 - cloud.a);
            } else if (u_DebugView == 2) {
                color = base;
            } else if (u_DebugView == 3) {
                color = vec3(pow(1.0 - depth, 3.0));
            }

            color = AcesFilm(color * u_Exposure);
            color = pow(color, vec3(1.0 / 2.2));

            float vignette = 1.0 - 0.20 * pow(length(uv - 0.5) * 1.45, 2.0);
            color *= vignette;

            float dither = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453) - 0.5;
            color += dither / 255.0;

            o_Color = vec4(color, 1.0);
        }
    )";
}

// ---------- VolumetricCloudCamera ----------

glm::vec3 VolumetricCloudCamera::Forward() const
{
    const float cosPitch = std::cos(pitch);
    return glm::vec3(cosPitch * std::sin(yaw), std::sin(pitch), -cosPitch * std::cos(yaw));
}

glm::vec3 VolumetricCloudCamera::Right() const
{
    return glm::vec3(std::cos(yaw), 0.0f, std::sin(yaw));
}

glm::vec3 VolumetricCloudCamera::Up() const
{
    return glm::normalize(glm::cross(Right(), Forward()));
}

glm::mat4 VolumetricCloudCamera::GetViewMatrix() const
{
    return glm::lookAt(position, position + Forward(), Up());
}

glm::mat4 VolumetricCloudCamera::GetProjectionMatrix() const
{
    const uint32_t width = Application::Get().GetWindow().GetWidth();
    const uint32_t height = Application::Get().GetWindow().GetHeight();
    const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 16.0f / 9.0f;
    return glm::perspective(fov, aspect, nearClip, farClip);
}

// ---------- VolumetricCloudLayer ----------

void VolumetricCloudLayer::OnAttach() {}

void VolumetricCloudLayer::OnDetach() {}

void VolumetricCloudLayer::EnsureInitialized()
{
    if (m_initialized) {
        return;
    }

    const auto start = std::chrono::high_resolution_clock::now();

    BuildNoiseTextures();
    BuildTerrain();
    CreateFullscreenQuad();

    m_terrainShader = Shader::Create("VolumetricCloudTerrain", TERRAIN_VERTEX_SHADER, TERRAIN_FRAGMENT_SHADER);
    m_cloudShader = Shader::Create("VolumetricCloudRaymarch", FULLSCREEN_VERTEX_SHADER, CLOUD_FRAGMENT_SHADER);
    m_compositeShader = Shader::Create("VolumetricCloudComposite", FULLSCREEN_VERTEX_SHADER, COMPOSITE_FRAGMENT_SHADER);

    m_terrainShader->Bind();
    m_terrainShader->SetInt("u_WeatherMap", 0);

    m_cloudShader->Bind();
    m_cloudShader->SetInt("u_SceneDepth", 0);
    m_cloudShader->SetInt("u_WeatherMap", 1);
    m_cloudShader->SetInt("u_ShapeNoise", 2);

    m_compositeShader->Bind();
    m_compositeShader->SetInt("u_SceneColor", 0);
    m_compositeShader->SetInt("u_CloudTexture", 1);
    m_compositeShader->SetInt("u_SceneDepth", 2);

    const auto end = std::chrono::high_resolution_clock::now();
    const float milliseconds = std::chrono::duration<float, std::milli>(end - start).count();
    OI_INFO("Volumetric cloud scene initialized in {:.0f} ms (terrain height at camera: {:.0f} m)",
            milliseconds, TerrainHeight(m_camera.position.x, m_camera.position.z));

    m_initialized = true;
}

void VolumetricCloudLayer::BuildNoiseTextures()
{
    // ---- 2D 天气图：R = 云量 / G = 云型（扰动云顶高度）/ B = 大尺度细节 ----
    {
        std::vector<uint8_t> pixels(static_cast<size_t>(WEATHER_SIZE) * WEATHER_SIZE * 4);
        for (int y = 0; y < WEATHER_SIZE; ++y) {
            for (int x = 0; x < WEATHER_SIZE; ++x) {
                const glm::vec2 uv((static_cast<float>(x) + 0.5f) / WEATHER_SIZE,
                                   (static_cast<float>(y) + 0.5f) / WEATHER_SIZE);

                const float coverageNoise = FbmPerlin2(uv, 4, 5, 101u);
                float coverage = SmoothStep(0.16f, 0.88f, 0.5f + coverageNoise * 0.95f);
                const float type = 0.5f + 0.65f * FbmPerlin2(uv + glm::vec2(0.31f, 0.77f), 2, 4, 211u);
                const float detail = 0.5f + 0.65f * FbmPerlin2(uv + glm::vec2(0.13f, 0.43f), 8, 3, 307u);

                const size_t index = (static_cast<size_t>(y) * WEATHER_SIZE + x) * 4;
                pixels[index + 0] = static_cast<uint8_t>(std::clamp(coverage, 0.0f, 1.0f) * 255.0f);
                pixels[index + 1] = static_cast<uint8_t>(std::clamp(type, 0.0f, 1.0f) * 255.0f);
                pixels[index + 2] = static_cast<uint8_t>(std::clamp(detail, 0.0f, 1.0f) * 255.0f);
                pixels[index + 3] = 255;
            }
        }

        TextureSpecification specification;
        specification.width = WEATHER_SIZE;
        specification.height = WEATHER_SIZE;
        specification.format = ImageFormat::RGBA8;
        m_weatherMap = Texture2D::Create(specification);
        m_weatherMap->SetData(pixels.data(), static_cast<uint32_t>(pixels.size()));
    }

    // ---- 3D 形状噪声：R = Perlin-Worley / G = 中频 Worley / B = 高频 Worley / A = Perlin ----
    {
        std::vector<uint8_t> data(static_cast<size_t>(NOISE_SIZE) * NOISE_SIZE * NOISE_SIZE * 4);

        unsigned int threadCount = std::max(1u, std::thread::hardware_concurrency());
        threadCount = std::min(threadCount, 16u);
        std::vector<std::thread> workers;
        workers.reserve(threadCount);

        for (unsigned int worker = 0; worker < threadCount; ++worker) {
            workers.emplace_back([&data, worker, threadCount]() {
                for (int z = static_cast<int>(worker); z < NOISE_SIZE; z += static_cast<int>(threadCount)) {
                    for (int y = 0; y < NOISE_SIZE; ++y) {
                        for (int x = 0; x < NOISE_SIZE; ++x) {
                            const glm::vec3 p((static_cast<float>(x) + 0.5f) / NOISE_SIZE,
                                              (static_cast<float>(y) + 0.5f) / NOISE_SIZE,
                                              (static_cast<float>(z) + 0.5f) / NOISE_SIZE);

                            const float perlin = std::clamp(0.5f + 0.5f * FbmPerlin3(p, 4, 4, 401u), 0.0f, 1.0f);
                            const float w4 = InvertedWorley3(p, 4, 501u);
                            const float w8 = InvertedWorley3(p, 8, 601u);
                            const float w16 = InvertedWorley3(p, 16, 701u);

                            const float worleyFbm = w4 * 0.625f + w8 * 0.25f + w16 * 0.125f;
                            const float perlinWorley = Lerp(worleyFbm, 1.0f, perlin);

                            const size_t index = ((static_cast<size_t>(z) * NOISE_SIZE + y) * NOISE_SIZE + x) * 4;
                            data[index + 0] = static_cast<uint8_t>(std::clamp(perlinWorley, 0.0f, 1.0f) * 255.0f);
                            data[index + 1] = static_cast<uint8_t>(std::clamp(w8, 0.0f, 1.0f) * 255.0f);
                            data[index + 2] = static_cast<uint8_t>(std::clamp(w16, 0.0f, 1.0f) * 255.0f);
                            data[index + 3] = static_cast<uint8_t>(std::clamp(perlin, 0.0f, 1.0f) * 255.0f);
                        }
                    }
                }
            });
        }

        for (std::thread& worker : workers) {
            worker.join();
        }

        Texture3DSpecification specification;
        specification.width = NOISE_SIZE;
        specification.height = NOISE_SIZE;
        specification.depth = NOISE_SIZE;
        specification.format = ImageFormat::RGBA8;
        m_shapeNoise = Texture3D::Create(specification);
        m_shapeNoise->SetData(data.data(), static_cast<uint32_t>(data.size()));
    }
}

void VolumetricCloudLayer::BuildTerrain()
{
    constexpr int vertexCount = TERRAIN_SEGMENTS + 1;
    constexpr float cellSize = TERRAIN_SIZE / static_cast<float>(TERRAIN_SEGMENTS);
    constexpr float halfSize = TERRAIN_SIZE * 0.5f;

    std::vector<float> heights(static_cast<size_t>(vertexCount) * vertexCount);
    for (int z = 0; z < vertexCount; ++z) {
        const float worldZ = -halfSize + static_cast<float>(z) * cellSize;
        for (int x = 0; x < vertexCount; ++x) {
            const float worldX = -halfSize + static_cast<float>(x) * cellSize;
            heights[static_cast<size_t>(z) * vertexCount + x] = TerrainHeight(worldX, worldZ);
        }
    }

    // 两遍 3x3 高斯平滑：削弱 ridged noise 产生的锯齿状尖峰，让山脊更自然
    for (int pass = 0; pass < 2; ++pass) {
        std::vector<float> blurred = heights;
        for (int z = 1; z < vertexCount - 1; ++z) {
            for (int x = 1; x < vertexCount - 1; ++x) {
                const size_t index = static_cast<size_t>(z) * vertexCount + x;
                float sum = heights[index] * 4.0f;
                sum += (heights[index - 1] + heights[index + 1] +
                        heights[index - vertexCount] + heights[index + vertexCount]) * 2.0f;
                sum += heights[index - vertexCount - 1] + heights[index - vertexCount + 1] +
                       heights[index + vertexCount - 1] + heights[index + vertexCount + 1];
                blurred[index] = sum / 16.0f;
            }
        }
        heights.swap(blurred);
    }

    const auto sampleHeight = [&](int x, int z) {
        x = std::clamp(x, 0, vertexCount - 1);
        z = std::clamp(z, 0, vertexCount - 1);
        return heights[static_cast<size_t>(z) * vertexCount + x];
    };

    std::vector<float> vertices;
    vertices.reserve(static_cast<size_t>(vertexCount) * vertexCount * 10);

    const glm::vec3 snowColor(0.78f, 0.83f, 0.92f);
    const glm::vec3 rockDark(0.036f, 0.034f, 0.034f);
    const glm::vec3 rockLight(0.110f, 0.102f, 0.094f);
    const glm::vec3 forestDark(0.016f, 0.032f, 0.022f);
    const glm::vec3 forestLight(0.034f, 0.055f, 0.036f);

    for (int z = 0; z < vertexCount; ++z) {
        const float worldZ = -halfSize + static_cast<float>(z) * cellSize;
        for (int x = 0; x < vertexCount; ++x) {
            const float worldX = -halfSize + static_cast<float>(x) * cellSize;
            const float height = heights[static_cast<size_t>(z) * vertexCount + x];

            const float left = sampleHeight(x - 1, z);
            const float right = sampleHeight(x + 1, z);
            const float down = sampleHeight(x, z - 1);
            const float up = sampleHeight(x, z + 1);
            const glm::vec3 normal = glm::normalize(glm::vec3(left - right, 2.0f * cellSize, down - up));

            const float detailNoise = 0.5f + 0.5f * FbmPerlin2(glm::vec2(worldX, worldZ) * 0.0009f, 3, 3, 811u);
            const float slope = 1.0f - std::clamp(normal.y, 0.0f, 1.0f);

            glm::vec3 albedo = glm::mix(forestDark, forestLight, detailNoise);
            albedo = glm::mix(albedo, glm::mix(rockDark, rockLight, detailNoise), SmoothStep(420.0f, 1350.0f, height));
            const float snow = SmoothStep(1900.0f, 2600.0f, height) * (1.0f - SmoothStep(0.68f, 0.94f, slope));
            albedo = glm::mix(albedo, snowColor, snow);

            vertices.push_back(worldX);
            vertices.push_back(height);
            vertices.push_back(worldZ);

            vertices.push_back(normal.x);
            vertices.push_back(normal.y);
            vertices.push_back(normal.z);

            vertices.push_back(albedo.r);
            vertices.push_back(albedo.g);
            vertices.push_back(albedo.b);
            vertices.push_back(1.0f);
        }
    }

    std::vector<uint32_t> indices;
    indices.reserve(static_cast<size_t>(TERRAIN_SEGMENTS) * TERRAIN_SEGMENTS * 6);
    for (int z = 0; z < TERRAIN_SEGMENTS; ++z) {
        for (int x = 0; x < TERRAIN_SEGMENTS; ++x) {
            const uint32_t topLeft = static_cast<uint32_t>(z) * vertexCount + x;
            const uint32_t topRight = topLeft + 1;
            const uint32_t bottomLeft = topLeft + vertexCount;
            const uint32_t bottomRight = bottomLeft + 1;

            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);

            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }

    m_terrainMesh = VertexArray::Create();
    Ref<VertexBuffer> vertexBuffer = VertexBuffer::Create(static_cast<uint32_t>(vertices.size() * sizeof(float)));
    vertexBuffer->SetLayout({
        { ShaderDataType::Float3, "a_Position" },
        { ShaderDataType::Float3, "a_Normal" },
        { ShaderDataType::Float4, "a_Color" },
    });
    vertexBuffer->SetData(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(float)));
    m_terrainMesh->AddVertexBuffer(vertexBuffer);
    m_terrainMesh->SetIndexBuffer(IndexBuffer::Create(indices.data(), static_cast<uint32_t>(indices.size())));
}

void VolumetricCloudLayer::CreateFullscreenQuad()
{
    const float vertices[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f,  1.0f,
    };
    const uint32_t indices[] = { 0, 1, 2, 2, 3, 0 };

    m_fullscreenQuad = VertexArray::Create();
    Ref<VertexBuffer> vertexBuffer = VertexBuffer::Create(const_cast<float*>(vertices), sizeof(vertices));
    vertexBuffer->SetLayout({ { ShaderDataType::Float2, "a_Position" } });
    m_fullscreenQuad->AddVertexBuffer(vertexBuffer);
    m_fullscreenQuad->SetIndexBuffer(IndexBuffer::Create(const_cast<uint32_t*>(indices), 6));
}

void VolumetricCloudLayer::EnsureRenderTargets()
{
    const Window& window = Application::Get().GetWindow();
    const uint32_t width = window.GetWidth();
    const uint32_t height = window.GetHeight();
    if (width == 0 || height == 0) {
        return;
    }

    uint32_t cloudWidth = std::max(1u, static_cast<uint32_t>(static_cast<float>(width) * m_cloud.renderScale));
    uint32_t cloudHeight = std::max(1u, static_cast<uint32_t>(static_cast<float>(height) * m_cloud.renderScale));

    if (!m_sceneFbo) {
        FramebufferSpecification sceneSpecification;
        sceneSpecification.width = width;
        sceneSpecification.height = height;
        sceneSpecification.attachments = {
            FramebufferTextureFormat::RGBA16F,
            FramebufferTextureFormat::DEPTH24,
        };
        m_sceneFbo = Framebuffer::Create(sceneSpecification);
        m_sceneWidth = width;
        m_sceneHeight = height;

        FramebufferSpecification cloudSpecification;
        cloudSpecification.width = cloudWidth;
        cloudSpecification.height = cloudHeight;
        cloudSpecification.attachments = { FramebufferTextureFormat::RGBA16F };
        m_cloudFbo = Framebuffer::Create(cloudSpecification);
        m_cloudWidth = cloudWidth;
        m_cloudHeight = cloudHeight;
        return;
    }

    if (width != m_sceneWidth || height != m_sceneHeight) {
        m_sceneFbo->Resize(width, height);
        m_sceneWidth = width;
        m_sceneHeight = height;
    }

    if (cloudWidth != m_cloudWidth || cloudHeight != m_cloudHeight) {
        m_cloudFbo->Resize(cloudWidth, cloudHeight);
        m_cloudWidth = cloudWidth;
        m_cloudHeight = cloudHeight;
    }
}

void VolumetricCloudLayer::UpdateCamera(float deltaTime)
{
    ImGuiIO& io = ImGui::GetIO();

    // 右键拖动视角（ImGui 正在操作面板时不接管鼠标）
    const bool rightDown = Input::IsMouseButtonPressed(Mouse::ButtonRight);
    if (rightDown && !io.WantCaptureMouse) {
        const glm::vec2 mouse = Input::GetMousePosition();
        if (m_dragging) {
            const glm::vec2 delta = mouse - m_lastMouse;
            m_camera.yaw += glm::radians(delta.x * 0.11f);
            m_camera.pitch -= glm::radians(delta.y * 0.11f);
            m_camera.pitch = std::clamp(m_camera.pitch, glm::radians(-88.0f), glm::radians(88.0f));
        }
        m_lastMouse = mouse;
        m_dragging = true;
    } else {
        m_dragging = false;
    }

    if (io.WantTextInput) {
        return;
    }

    glm::vec3 movement(0.0f);
    const glm::vec3 forward = m_camera.Forward();
    const glm::vec3 right = m_camera.Right();

    if (Input::IsKeyPressed(Key::W)) movement += forward;
    if (Input::IsKeyPressed(Key::S)) movement -= forward;
    if (Input::IsKeyPressed(Key::D)) movement += right;
    if (Input::IsKeyPressed(Key::A)) movement -= right;
    if (Input::IsKeyPressed(Key::E) || Input::IsKeyPressed(Key::Space)) movement.y += 1.0f;
    if (Input::IsKeyPressed(Key::Q) || Input::IsKeyPressed(Key::LeftControl)) movement.y -= 1.0f;

    const bool boost = Input::IsKeyPressed(Key::LeftShift) || Input::IsKeyPressed(Key::RightShift);
    if (glm::length(movement) > 0.0f) {
        m_camera.position += glm::normalize(movement) * m_moveSpeed * (boost ? 4.0f : 1.0f) * deltaTime;
    }

    if (m_autoCruise) {
        const glm::vec3 cruiseDirection = glm::normalize(glm::vec3(forward.x, 0.0f, forward.z));
        m_camera.position += cruiseDirection * m_cruiseSpeed * deltaTime;
    }

    m_camera.position.y = std::max(m_camera.position.y, 80.0f);
}

glm::vec3 VolumetricCloudLayer::SunDirection() const
{
    const float cosElevation = std::cos(m_sunElevation);
    return glm::normalize(glm::vec3(cosElevation * std::sin(m_sunAzimuth),
                                    std::sin(m_sunElevation),
                                    -cosElevation * std::cos(m_sunAzimuth)));
}

glm::vec2 VolumetricCloudLayer::WeatherOffset() const
{
    return m_windOffset;
}

void VolumetricCloudLayer::OnUpdate()
{
    if (!m_active) {
        return;
    }

    EnsureInitialized();
    EnsureRenderTargets();
    if (!m_sceneFbo || m_sceneWidth == 0 || m_sceneHeight == 0) {
        return;
    }

    // 本场景自己管理 HDR：场景 -> 离屏 RGBA16F，云层 -> 半分辨率，最后统一 tonemap
    Renderer::SetHDREnabled(false);
    Renderer::SetLightCullingEnabled(false);

    const float deltaTime = std::min(Timer::GetDeltaTime(), 0.1f);
    m_time += deltaTime;
    ++m_frameIndex;

    // UI 里云底/云顶是独立滑条，保证至少留出 600m 厚度
    if (m_cloud.cloudTop < m_cloud.cloudBase + 600.0f) {
        m_cloud.cloudTop = m_cloud.cloudBase + 600.0f;
    }

    UpdateCamera(deltaTime);

    // 天气图以风速平移（纹理覆盖 26km）
    m_windOffset += glm::vec2(1.0f, 0.35f) * (m_cloud.windSpeed * deltaTime / 26000.0f);
    m_windOffset.x = m_windOffset.x - std::floor(m_windOffset.x);
    m_windOffset.y = m_windOffset.y - std::floor(m_windOffset.y);

    RenderTerrain();
    RenderClouds();
    RenderComposite();

    RenderCommand::SetViewport(0, 0, m_sceneWidth, m_sceneHeight);
}

void VolumetricCloudLayer::RenderTerrain()
{
    m_sceneFbo->Bind();
    RenderCommand::SetDepthTest(true);
    RenderCommand::SetBlending(false);
    RenderCommand::Clear();

    Renderer::BeginScene(m_camera);

    const glm::vec3 sunDirection = SunDirection();
    m_terrainShader->Bind();
    m_terrainShader->SetFloat3("u_SunDirection", sunDirection);
    m_terrainShader->SetFloat3("u_SunColor", m_sunColor);
    m_terrainShader->SetFloat("u_SunIntensity", m_sunIntensity);
    m_terrainShader->SetFloat3("u_SkyAmbient", m_skyAmbient);
    m_terrainShader->SetFloat3("u_HazeColor", m_hazeColor);
    m_terrainShader->SetFloat3("u_SunHazeColor", glm::mix(m_hazeColor, m_sunColor, 0.55f));
    m_terrainShader->SetFloat("u_FogDensity", m_fogDensity);
    m_terrainShader->SetFloat("u_CloudBase", m_cloud.cloudBase);
    m_terrainShader->SetFloat("u_CloudTop", m_cloud.cloudTop);
    m_terrainShader->SetFloat2("u_WeatherOffset", WeatherOffset());
    m_weatherMap->Bind(0);
    RenderCommand::DrawIndexed(m_terrainMesh);

    Renderer::EndScene();
    m_sceneFbo->Unbind();
}

void VolumetricCloudLayer::RenderClouds()
{
    m_cloudFbo->Bind();
    RenderCommand::SetDepthTest(false);
    RenderCommand::SetBlending(false);
    RenderCommand::Clear();

    const glm::mat4 viewProjection = m_camera.GetProjectionMatrix() * m_camera.GetViewMatrix();
    const glm::mat4 inverseViewProjection = glm::inverse(viewProjection);

    m_cloudShader->Bind();
    m_cloudShader->SetMat4("u_InvViewProj", inverseViewProjection);
    m_cloudShader->SetFloat3("u_CameraPosition", m_camera.GetPosition());
    m_cloudShader->SetFloat3("u_SunDirection", SunDirection());
    m_cloudShader->SetFloat3("u_SunColor", m_sunColor);
    m_cloudShader->SetFloat("u_SunIntensity", m_sunIntensity);
    m_cloudShader->SetFloat3("u_SkyZenith", m_skyZenith);
    m_cloudShader->SetFloat3("u_SkyHorizon", m_skyHorizon);
    m_cloudShader->SetFloat3("u_HazeColor", m_hazeColor);
    m_cloudShader->SetFloat("u_FogDensity", m_fogDensity);
    m_cloudShader->SetFloat("u_Coverage", m_cloud.coverage);
    m_cloudShader->SetFloat("u_Density", m_cloud.density);
    m_cloudShader->SetFloat("u_CloudBase", m_cloud.cloudBase);
    m_cloudShader->SetFloat("u_CloudTop", m_cloud.cloudTop);
    m_cloudShader->SetFloat("u_Extinction", m_cloud.extinction);
    m_cloudShader->SetFloat("u_DetailStrength", m_cloud.detailStrength);
    m_cloudShader->SetFloat("u_Ambient", m_cloud.ambient);
    m_cloudShader->SetFloat2("u_WindOffset", WeatherOffset());
    m_cloudShader->SetFloat("u_Time", m_time);
    m_cloudShader->SetFloat2("u_Resolution", glm::vec2(static_cast<float>(m_cloudWidth),
                                                      static_cast<float>(m_cloudHeight)));
    m_cloudShader->SetFloat("u_Frame", static_cast<float>(m_frameIndex % 4096));
    m_cloudShader->SetInt("u_Steps", m_cloud.steps);
    m_cloudShader->SetInt("u_LightSteps", m_cloud.lightSteps);

    RenderCommand::BindTexture2D(0, m_sceneFbo->GetDepthAttachmentRendererID());
    m_weatherMap->Bind(1);
    m_shapeNoise->Bind(2);
    RenderCommand::DrawIndexed(m_fullscreenQuad);

    m_cloudFbo->Unbind();
    RenderCommand::SetViewport(0, 0, m_sceneWidth, m_sceneHeight);
}

void VolumetricCloudLayer::RenderComposite()
{
    RenderCommand::SetViewport(0, 0, m_sceneWidth, m_sceneHeight);
    RenderCommand::SetDepthTest(false);
    RenderCommand::SetBlending(false);

    const glm::mat4 viewProjection = m_camera.GetProjectionMatrix() * m_camera.GetViewMatrix();
    const glm::mat4 inverseViewProjection = glm::inverse(viewProjection);

    m_compositeShader->Bind();
    m_compositeShader->SetMat4("u_InvViewProj", inverseViewProjection);
    m_compositeShader->SetFloat3("u_CameraPosition", m_camera.GetPosition());
    m_compositeShader->SetFloat3("u_SunDirection", SunDirection());
    m_compositeShader->SetFloat3("u_SunColor", m_sunColor);
    m_compositeShader->SetFloat("u_SunIntensity", m_sunIntensity);
    m_compositeShader->SetFloat3("u_SkyZenith", m_skyZenith);
    m_compositeShader->SetFloat3("u_SkyHorizon", m_skyHorizon);
    m_compositeShader->SetFloat3("u_HazeColor", m_hazeColor);
    m_compositeShader->SetFloat("u_Exposure", m_exposure);
    m_compositeShader->SetFloat("u_Denoise", m_cloud.denoise);
    m_compositeShader->SetInt("u_DebugView", m_debugView);
    m_compositeShader->SetFloat2("u_Resolution", glm::vec2(static_cast<float>(m_sceneWidth),
                                                          static_cast<float>(m_sceneHeight)));

    RenderCommand::BindTexture2D(0, m_sceneFbo->GetColorAttachmentRendererID());
    RenderCommand::BindTexture2D(1, m_cloudFbo->GetColorAttachmentRendererID());
    RenderCommand::BindTexture2D(2, m_sceneFbo->GetDepthAttachmentRendererID());
    RenderCommand::DrawIndexed(m_fullscreenQuad);

    RenderCommand::SetDepthTest(true);
    RenderCommand::SetBlending(true);
}

void VolumetricCloudLayer::ResetCamera()
{
    m_camera.position = glm::vec3(0.0f, 4800.0f, 13000.0f);
    m_camera.yaw = 0.0f;
    m_camera.pitch = glm::radians(-10.0f);
    m_camera.fov = glm::radians(58.0f);
}

void VolumetricCloudLayer::ApplyPreset(int preset)
{
    m_cloud.cloudBase = 1200.0f;
    m_cloud.cloudTop = 4600.0f;
    m_cloud.density = 1.0f;
    m_fogDensity = 0.000040f;
    m_camera.fov = glm::radians(58.0f);

    switch (preset) {
        case 0:  // 黄金时刻
            m_sunAzimuth = glm::radians(-22.0f);
            m_sunElevation = glm::radians(6.0f);
            m_sunIntensity = 5.0f;
            m_exposure = 1.05f;
            m_sunColor = glm::vec3(1.00f, 0.72f, 0.40f);
            m_skyZenith = glm::vec3(0.025f, 0.065f, 0.180f);
            m_skyHorizon = glm::vec3(0.92f, 0.34f, 0.09f);
            m_hazeColor = glm::vec3(0.40f, 0.36f, 0.36f);
            m_skyAmbient = glm::vec3(0.20f, 0.26f, 0.38f);
            m_cloud.coverage = 0.58f;
            m_cloud.extinction = 0.0060f;
            m_cloud.ambient = 0.24f;
            m_fogDensity = 0.000040f;
            break;

        case 1:  // 蓝调时刻
            m_sunAzimuth = glm::radians(52.0f);
            m_sunElevation = glm::radians(2.0f);
            m_sunIntensity = 1.8f;
            m_exposure = 1.55f;
            m_sunColor = glm::vec3(0.62f, 0.55f, 0.78f);
            m_skyZenith = glm::vec3(0.010f, 0.022f, 0.060f);
            m_skyHorizon = glm::vec3(0.20f, 0.28f, 0.44f);
            m_hazeColor = glm::vec3(0.14f, 0.18f, 0.27f);
            m_skyAmbient = glm::vec3(0.055f, 0.085f, 0.16f);
            m_cloud.coverage = 0.52f;
            m_cloud.extinction = 0.012f;
            m_cloud.ambient = 0.60f;
            m_fogDensity = 0.000080f;
            break;

        case 2:  // 风暴
            m_sunAzimuth = glm::radians(-18.0f);
            m_sunElevation = glm::radians(17.0f);
            m_sunIntensity = 2.2f;
            m_exposure = 0.95f;
            m_sunColor = glm::vec3(0.80f, 0.85f, 0.95f);
            m_skyZenith = glm::vec3(0.040f, 0.055f, 0.085f);
            m_skyHorizon = glm::vec3(0.30f, 0.29f, 0.30f);
            m_hazeColor = glm::vec3(0.22f, 0.22f, 0.23f);
            m_skyAmbient = glm::vec3(0.10f, 0.12f, 0.16f);
            m_cloud.coverage = 0.86f;
            m_cloud.cloudBase = 900.0f;
            m_cloud.cloudTop = 3100.0f;
            m_cloud.density = 1.35f;
            m_cloud.extinction = 0.014f;
            m_cloud.ambient = 0.50f;
            m_fogDensity = 0.00010f;
            break;

        default:  // 正午
            m_sunAzimuth = glm::radians(-12.0f);
            m_sunElevation = glm::radians(52.0f);
            m_sunIntensity = 4.4f;
            m_exposure = 0.82f;
            m_sunColor = glm::vec3(1.00f, 0.96f, 0.90f);
            m_skyZenith = glm::vec3(0.050f, 0.160f, 0.450f);
            m_skyHorizon = glm::vec3(0.55f, 0.70f, 0.92f);
            m_hazeColor = glm::vec3(0.55f, 0.65f, 0.80f);
            m_skyAmbient = glm::vec3(0.22f, 0.30f, 0.44f);
            m_cloud.coverage = 0.42f;
            m_cloud.cloudBase = 1500.0f;
            m_cloud.cloudTop = 3000.0f;
            m_cloud.density = 0.9f;
            m_cloud.extinction = 0.010f;
            m_cloud.ambient = 0.48f;
            m_fogDensity = 0.000050f;
            break;
    }
}

void VolumetricCloudLayer::OnImGuiRender()
{
    if (!m_active || !m_initialized) {
        return;
    }

    // ImGui 会截获键盘事件，H 必须在这里处理
    if (ImGui::IsKeyPressed(ImGuiKey_H, false)) {
        m_showPanel = !m_showPanel;
    }
    if (!m_showPanel) {
        return;
    }

    // 首次进入时收起面板，先让用户看到完整场景；展开后 ImGui 会记住状态
    if (!m_panelConfigured) {
        ImGui::SetNextWindowCollapsed(true);
        m_panelConfigured = true;
    }

    ImGui::Begin("体积云 · Horizon", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::PushItemWidth(150.0f);

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("FPS %.1f  (%.2f ms)", io.Framerate, io.Framerate > 0.0f ? 1000.0f / io.Framerate : 0.0f);
    ImGui::Text("云缓冲 %u x %u  |  步进 %d", m_cloudWidth, m_cloudHeight, m_cloud.steps);
    ImGui::TextDisabled("F7 切换场景  |  右键拖动视角  |  WASD / QE 飞行  |  Shift 加速  |  R 重置");

    ImGui::Separator();
    ImGui::Text("预设");
    const char* presets[] = { "黄金时刻", "蓝调时刻", "风暴", "正午" };
    for (int i = 0; i < 4; ++i) {
        if (i % 2 != 0) {
            ImGui::SameLine();
        }
        if (ImGui::Button(presets[i], ImVec2(116.0f, 0.0f))) {
            ApplyPreset(i);
        }
    }

    ImGui::Separator();
    if (ImGui::CollapsingHeader("云层", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("云量", &m_cloud.coverage, 0.15f, 1.0f, "%.2f");
        ImGui::SliderFloat("密度", &m_cloud.density, 0.2f, 3.0f, "%.2f");
        ImGui::SliderFloat("云底 (m)", &m_cloud.cloudBase, 500.0f, 2500.0f, "%.0f");
        ImGui::SliderFloat("云顶 (m)", &m_cloud.cloudTop, 2500.0f, 6000.0f, "%.0f");
        ImGui::SliderFloat("消光", &m_cloud.extinction, 0.002f, 0.020f, "%.4f");
        ImGui::SliderFloat("细节侵蚀", &m_cloud.detailStrength, 0.0f, 0.75f, "%.2f");
        ImGui::SliderFloat("降噪", &m_cloud.denoise, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("环境光", &m_cloud.ambient, 0.0f, 1.2f, "%.2f");
        ImGui::SliderFloat("风速 (m/s)", &m_cloud.windSpeed, 0.0f, 80.0f, "%.0f");
        ImGui::SliderInt("步进次数", &m_cloud.steps, 32, 200);
        ImGui::SliderInt("光步进", &m_cloud.lightSteps, 2, 8);
        ImGui::SliderFloat("云渲染比例", &m_cloud.renderScale, 0.4f, 1.0f, "%.2f");
    }

    if (ImGui::CollapsingHeader("大气 / 光照", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderAngle("太阳方位", &m_sunAzimuth, -180.0f, 180.0f);
        ImGui::SliderAngle("太阳高度", &m_sunElevation, 0.5f, 80.0f);
        ImGui::SliderFloat("太阳强度", &m_sunIntensity, 0.2f, 9.0f, "%.2f");
        ImGui::SliderFloat("曝光", &m_exposure, 0.2f, 3.0f, "%.2f");
        ImGui::SliderFloat("大气雾", &m_fogDensity, 0.0f, 0.0002f, "%.6f");
    }

    if (ImGui::CollapsingHeader("相机", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("位置 %.0f, %.0f, %.0f", m_camera.position.x, m_camera.position.y, m_camera.position.z);
        ImGui::SliderFloat("移动速度", &m_moveSpeed, 100.0f, 4000.0f, "%.0f m/s");
        ImGui::Checkbox("自动巡航", &m_autoCruise);
        ImGui::SameLine();
        ImGui::SliderFloat("巡航速度", &m_cruiseSpeed, 10.0f, 400.0f, "%.0f m/s");
        if (ImGui::Button("重置视角")) {
            ResetCamera();
        }
    }

    if (ImGui::CollapsingHeader("调试")) {
        const char* debugViews[] = { "最终画面", "云透过率", "场景（无云）", "深度" };
        ImGui::Combo("视图", &m_debugView, debugViews, 4);
        ImGui::Checkbox("显示帮助", &m_showHelp);
    }

    if (m_showHelp) {
        ImGui::Separator();
        ImGui::TextDisabled("单次散射 + 多次散射近似 / 3D Perlin-Worley / 天气图侵蚀");
    }

    ImGui::PopItemWidth();
    ImGui::End();
}

void VolumetricCloudLayer::OnEvent(Event& event)
{
    if (!m_active) {
        return;
    }

    if (event.GetEventType() == EventType::KeyPressed) {
        const auto key = static_cast<KeyPressedEvent&>(event).GetKeyCode();
        if (key == Key::R) {
            ResetCamera();
            event.handled = true;
        } else if (key >= Key::D1 && key <= Key::D4) {
            // 1 ~ 4：最终 / 云透过率 / 场景 / 深度（调试用）
            m_debugView = static_cast<int>(key - Key::D1);
            event.handled = true;
        }
    }
}
