#type vertex
#version 450 core

layout(location = 0) in vec2 a_Position;

out vec2 v_Ndc;

void main()
{
    v_Ndc = a_Position;
    gl_Position = vec4(a_Position, 0.0, 1.0);
}

#type fragment
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
