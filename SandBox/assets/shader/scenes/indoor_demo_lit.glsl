#type vertex
// 室内 PBR 光照：Cook-Torrance 直接光照 + Forward+ cluster 光源列表 + 方向光 / 聚光灯阴影。
// 输出线性 HDR 颜色（tonemap / gamma 由引擎后处理链处理），自发光用于灯具与窗户。
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec4 a_Color;

layout(std140, binding = 0) uniform Camera {
    mat4 u_Projection;
    mat4 u_View;
    vec4 u_CameraPosition;
};

uniform mat4 u_Transform;

out vec3 v_WorldPosition;
out vec3 v_Normal;
out vec4 v_Color;

void main()
{
    vec4 worldPosition = u_Transform * vec4(a_Position, 1.0);
    gl_Position = u_Projection * u_View * worldPosition;
    v_WorldPosition = worldPosition.xyz;
    // 法线用逆转置矩阵变换，兼容非均匀缩放
    v_Normal = mat3(transpose(inverse(u_Transform))) * a_Normal;
    v_Color = a_Color;
}

#type fragment
#version 450 core

#define MAX_LIGHTS 256
#define MAX_SHADOW_CASTERS 8
#define MAX_LIGHTS_PER_CLUSTER 48
#define TILE_SIZE 16
#define PI 3.141592653589793

// 与 C++ 侧 LightingData / LightData 的 std430 布局严格对应
struct Light {
    vec4 positionType;   // xyz: 世界空间位置（Point/Spot）或传播方向（Directional）；w: 类型
    vec4 directionRange; // xyz: 方向（Spot）；w: 影响范围（Point/Spot）
    vec4 colorIntensity; // rgb: 颜色；w: 强度
    vec4 params;         // x: 内锥角余弦；y: 外锥角余弦（Spot）
    vec4 flags;          // x: 阴影矩阵槽位（-1 不投影）；y: 阴影深度偏移基数
};

layout(std430, binding = 3) readonly buffer Lighting {
    vec4  u_Ambient;      // rgb: 颜色；a: 强度
    ivec4 u_Counts;       // x: 光源数；y: 阴影矩阵数；z: 每 cluster 光源上限；w: 备用
    vec4  u_ClipPlanes;   // x: near；y: far；z: 1/log(far/near)；w: 备用
    ivec4 u_Grid;         // x/y: tilesX / tilesY；z: 深度切片数；w: 是否启用 cluster 剔除
    Light u_Lights[MAX_LIGHTS];
    mat4  u_ShadowMatrices[MAX_SHADOW_CASTERS];
};

// Forward+ 剔除结果（由 LightClustering 的 compute 写入）
layout(std430, binding = 4) readonly buffer ClusterLightCounts {
    uint u_ClusterLightCounts[];
};
layout(std430, binding = 5) readonly buffer ClusterLightIndices {
    uint u_ClusterLightIndices[];
};

// 方向光阴影（与 ShadowData 的 std140 布局一致）
layout(std140, binding = 2) uniform Shadow {
    mat4 u_LightSpaceMatrix;
    vec4 u_ShadowParams;  // x: 1/贴图尺寸；y: 深度偏移基数
};

layout(std140, binding = 0) uniform Camera {
    mat4 u_Projection;
    mat4 u_View;
    vec4 u_CameraPosition;
};

uniform sampler2D u_SunShadowMap;          // 方向光阴影（纹理单元 1）
uniform sampler2DArray u_SpotShadowMaps;   // 聚光灯阴影（纹理单元 0，每盏一层）
uniform float u_SpotShadowTexel;           // 1 / 聚光灯阴影贴图尺寸
uniform samplerCube u_IrradianceMap;       // 环境探针：辐照度（纹理单元 2）
uniform samplerCube u_PrefilteredMap;      // 环境探针：粗糙度分级的镜面反射（纹理单元 3）
uniform float u_ProbeMaxLod;               // prefilter 的最大 mip（LOD = roughness * MaxLod）
uniform float u_ProbeIntensity;            // 探针环境光强度
uniform int u_UseProbe;                    // 是否使用环境探针（关闭时退回常量环境光）

uniform vec4 u_Albedo;      // rgb: 基础色（线性空间）；a: 不透明度
uniform vec3 u_Emissive;    // 自发光颜色（线性 HDR，可超过 1）
uniform float u_Metallic;
uniform float u_Roughness;
uniform float u_AO;

layout(location = 0) out vec4 o_Color;

in vec3 v_WorldPosition;
in vec3 v_Normal;
in vec4 v_Color;

// ---------- PBR 基础函数（GGX + Smith + Schlick） ----------

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denominator = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / max(PI * denominator * denominator, 0.0001);
}

float GeometrySchlickGGX(float NdotX, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotX / max(NdotX * (1.0 - k) + k, 0.0001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    return GeometrySchlickGGX(max(dot(N, V), 0.0), roughness) *
           GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 EnvBRDFApprox(vec3 F0, float roughness, float NdotV)
{
    const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * NdotV)) * r.x + r.y;
    vec2 ab = vec2(-1.04, 1.04) * a004 + r.zw;
    return F0 * ab.x + ab.y;
}

// ---------- 阴影可见度 ----------

// 方向光：世界坐标投影到光空间与深度图比较（3x3 PCF）
float ComputeSunVisibility(vec3 normal, vec3 lightDirection)
{
    vec4 lightSpacePosition = u_LightSpaceMatrix * vec4(v_WorldPosition, 1.0);
    vec3 projected = lightSpacePosition.xyz / lightSpacePosition.w * 0.5 + 0.5;
    if (projected.z > 1.0) {
        return 1.0;
    }

    float bias = max(u_ShadowParams.y * (1.0 - dot(normal, lightDirection)), u_ShadowParams.y * 0.2);
    float texel = u_ShadowParams.x;

    float visibility = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float shadowDepth = texture(u_SunShadowMap, projected.xy + vec2(x, y) * texel).r;
            visibility += projected.z - bias > shadowDepth ? 0.0 : 1.0;
        }
    }
    return visibility / 9.0;
}

// 聚光灯：按槽位取光空间矩阵，采样阴影贴图数组的对应层（3x3 PCF，bias 逐光源）
float ComputeSpotVisibility(int slot, float bias, vec3 normal, vec3 lightDirection)
{
    vec4 lightSpacePosition = u_ShadowMatrices[slot] * vec4(v_WorldPosition, 1.0);
    if (lightSpacePosition.w <= 0.0) {
        return 1.0;
    }

    vec3 projected = lightSpacePosition.xyz / lightSpacePosition.w * 0.5 + 0.5;
    if (projected.x < 0.0 || projected.x > 1.0 || projected.y < 0.0 || projected.y > 1.0 || projected.z > 1.0) {
        return 1.0;
    }

    float slopeBias = max(bias * (1.0 - dot(normal, lightDirection)), bias * 0.2);

    float visibility = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float shadowDepth = texture(u_SpotShadowMaps,
                vec3(projected.xy + vec2(x, y) * u_SpotShadowTexel, float(slot))).r;
            visibility += projected.z - slopeBias > shadowDepth ? 0.0 : 1.0;
        }
    }
    return visibility / 9.0;
}

// ---------- 光照累加 ----------

struct Surface {
    vec3 N;
    vec3 V;
    vec3 albedo;
    vec3 F0;
    float NdotV;
    float metallic;
    float roughness;
};

void AccumulateLight(const int index, const Surface surface, inout vec3 directLight)
{
    const int type = int(u_Lights[index].positionType.w + 0.5);

    vec3 lightDirection;  // 从着色点指向光源
    float attenuation = 1.0;

    if (type == 0) {
        lightDirection = -normalize(u_Lights[index].positionType.xyz);
        attenuation = ComputeSunVisibility(surface.N, lightDirection);
    } else {
        const vec3 toLight = u_Lights[index].positionType.xyz - v_WorldPosition;
        const float distance = length(toLight);
        lightDirection = toLight / max(distance, 0.0001);

        // 窗函数截断的平方反比衰减（intensity 为 1 单位距离处的照度）
        const float w = clamp(1.0 - pow(distance / max(u_Lights[index].directionRange.w, 0.0001), 4.0), 0.0, 1.0);
        attenuation = (w * w) / max(distance * distance, 0.01);

        if (type == 2) {
            const float theta = dot(lightDirection, normalize(-u_Lights[index].directionRange.xyz));
            const float epsilon = max(u_Lights[index].params.x - u_Lights[index].params.y, 0.0001);
            attenuation *= clamp((theta - u_Lights[index].params.y) / epsilon, 0.0, 1.0);

            const int slot = int(u_Lights[index].flags.x + 0.5);
            if (slot >= 0) {
                attenuation *= ComputeSpotVisibility(slot, u_Lights[index].flags.y, surface.N, lightDirection);
            }
        }
    }

    const float NdotL = max(dot(surface.N, lightDirection), 0.0);
    if (NdotL <= 0.0 || attenuation <= 0.0) {
        return;
    }

    const vec3 H = normalize(surface.V + lightDirection);
    const vec3 radiance = u_Lights[index].colorIntensity.rgb * u_Lights[index].colorIntensity.w;

    const float D = DistributionGGX(surface.N, H, surface.roughness);
    const float G = GeometrySmith(surface.N, surface.V, lightDirection, surface.roughness);
    const vec3 F = FresnelSchlick(max(dot(H, surface.V), 0.0), surface.F0);

    const vec3 specular = D * G * F / max(4.0 * surface.NdotV * NdotL, 0.0001);
    const vec3 kD = (1.0 - F) * (1.0 - surface.metallic);

    directLight += (kD * surface.albedo / PI + specular) * radiance * NdotL * attenuation;
}

// 片元所属 cluster（与 LightClustering 的 compute 源码使用同一套索引规则）
uint ClusterIndexForFragment(vec3 worldPosition)
{
    const float viewDepth = -(u_View * vec4(worldPosition, 1.0)).z;
    const float nearClip = max(u_ClipPlanes.x, 0.0001);
    const int slices = max(u_Grid.z, 1);
    const float sliceCoord = log(max(viewDepth, nearClip) / nearClip) * u_ClipPlanes.z * float(slices);
    const int slice = clamp(int(floor(sliceCoord)), 0, slices - 1);

    const ivec2 tiles = max(u_Grid.xy, ivec2(1));
    const ivec2 tile = clamp(ivec2(gl_FragCoord.xy / float(TILE_SIZE)), ivec2(0), tiles - 1);

    return uint((slice * tiles.y + tile.y) * tiles.x + tile.x);
}

void main()
{
    const vec3 albedo = v_Color.rgb * u_Albedo.rgb;
    const float metallic = clamp(u_Metallic, 0.0, 1.0);
    const float roughness = clamp(u_Roughness, 0.045, 1.0);

    Surface surface;
    surface.N = normalize(v_Normal);
    surface.V = normalize(u_CameraPosition.xyz - v_WorldPosition);
    surface.NdotV = max(dot(surface.N, surface.V), 0.0001);
    surface.albedo = albedo;
    surface.metallic = metallic;
    surface.roughness = roughness;
    surface.F0 = mix(vec3(0.04), albedo, metallic);

    vec3 directLight = vec3(0.0);

    if (u_Grid.w != 0) {
        // Forward+：只遍历本 cluster 的光源列表
        const uint clusterIndex = ClusterIndexForFragment(v_WorldPosition);
        const uint lightCount = min(u_ClusterLightCounts[clusterIndex], uint(MAX_LIGHTS_PER_CLUSTER));
        for (uint i = 0u; i < lightCount; ++i) {
            const uint lightIndex = u_ClusterLightIndices[clusterIndex * uint(MAX_LIGHTS_PER_CLUSTER) + i];
            AccumulateLight(int(lightIndex), surface, directLight);
        }
    } else {
        // 回退路径：遍历全部光源（对照排查用）
        for (int i = 0; i < u_Counts.x; ++i) {
            AccumulateLight(i, surface, directLight);
        }
    }

    // 环境光：探针（室内间接光 + 环境镜面）+ 常量环境光兜底
    vec3 ambientDiffuse = vec3(0.0);
    vec3 ambientSpecular = vec3(0.0);

    if (u_UseProbe != 0) {
        const vec3 reflectDirection = reflect(-surface.V, surface.N);
        const vec3 irradiance = texture(u_IrradianceMap, surface.N).rgb;
        const vec3 prefiltered = textureLod(u_PrefilteredMap, reflectDirection, roughness * u_ProbeMaxLod).rgb;

        ambientDiffuse = irradiance * albedo * (1.0 - metallic) * u_AO * u_ProbeIntensity;
        ambientSpecular = prefiltered * EnvBRDFApprox(surface.F0, roughness, surface.NdotV) * u_ProbeIntensity;
    }

    const vec3 constantAmbient = u_Ambient.rgb * u_Ambient.a * u_AO;
    ambientDiffuse += constantAmbient * albedo * (1.0 - metallic);
    ambientSpecular += constantAmbient * EnvBRDFApprox(surface.F0, roughness, surface.NdotV);

    // 线性 HDR 输出：曝光 / tonemap / gamma 由 PostProcess 统一处理
    o_Color = vec4(directLight + ambientDiffuse + ambientSpecular + u_Emissive, v_Color.a * u_Albedo.a);
}
