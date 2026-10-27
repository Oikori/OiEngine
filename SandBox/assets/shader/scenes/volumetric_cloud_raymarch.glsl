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
