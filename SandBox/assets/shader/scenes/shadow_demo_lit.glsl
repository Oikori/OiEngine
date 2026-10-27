#type vertex
// 光照着色器（PBR）：顶点阶段做变换并传递世界坐标 / 法线 / 顶点色
// 片元阶段为 Cook-Torrance 直接光照（GGX 分布 + Smith 几何项 + Schlick 菲涅尔）
// + 环境光近似（Karis 解析 env BRDF），方向光乘阴影可见度（3x3 PCF）；输出线性 HDR
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec4 a_Color;

layout(std140, binding = 0) uniform Camera {
    mat4 u_Projection;
    mat4 u_View;
    vec4 u_CameraPosition;  // xyz: 世界空间位置（片元着色器计算视线方向）
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
#define PI 3.141592653589793

// 与 C++ 侧 LightingData / LightData 的 std430 布局严格对应
struct Light {
    vec4 positionType;   // xyz: 世界空间位置（Point/Spot）或传播方向（Directional）；w: 类型
    vec4 directionRange; // xyz: 方向（Spot）；w: 影响范围（Point/Spot）
    vec4 colorIntensity; // rgb: 颜色；w: 强度
    vec4 params;         // x: 内锥角余弦；y: 外锥角余弦（Spot）
    vec4 flags;          // x: 阴影矩阵槽位（-1 不投影）；y: 阴影深度偏移基数
};

layout(std430, binding = 3) buffer Lighting {
    vec4  u_Ambient;      // rgb: 颜色；a: 强度
    ivec4 u_Counts;       // x: 光源数；y: 阴影矩阵数；z: 每 cluster 光源上限；w: 备用
    vec4  u_ClipPlanes;   // x: near；y: far；z: 1/log(far/near)；w: 备用（cluster 深度切片用）
    ivec4 u_Grid;         // x/y: tilesX / tilesY；z: 深度切片数；w: 是否启用 cluster 剔除
    Light u_Lights[MAX_LIGHTS];
    mat4  u_ShadowMatrices[MAX_SHADOW_CASTERS];
};

// 与 C++ 侧 ShadowData 的 std140 布局严格对应，由 Renderer::BeginShadowPass 上传
layout(std140, binding = 2) uniform Shadow {
    mat4 u_LightSpaceMatrix;
    vec4 u_ShadowParams;  // x: 1/阴影贴图尺寸（texel 大小）；y: 深度偏移基数
};

// 相机 UBO：与顶点着色器中的同名块保持一致（同一 binding = 0），这里用 u_CameraPosition 计算视线方向
layout(std140, binding = 0) uniform Camera {
    mat4 u_Projection;
    mat4 u_View;
    vec4 u_CameraPosition;
};

uniform sampler2D u_ShadowMap;
uniform vec4 u_Albedo;      // rgb: 基础色（线性空间）；a: 不透明度
uniform float u_Metallic;   // 金属度：0 电介质 / 1 金属
uniform float u_Roughness;  // 粗糙度：越小越光滑
uniform float u_AO;         // 环境光遮蔽（只作用于环境光照项）

layout(location = 0) out vec4 o_Color;

in vec3 v_WorldPosition;
in vec3 v_Normal;
in vec4 v_Color;

// ---------- PBR 基础函数 ----------

// GGX / Trowbridge-Reitz 法线分布函数
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denominator = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / max(PI * denominator * denominator, 0.0001);
}

// Schlick-GGX 几何项（直接光照版 k = (r + 1)^2 / 8）
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

// Schlick 菲涅尔近似
vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Karis 解析 env BRDF 近似：场景没有环境贴图，直接把环境光颜色当作常量环境辐照度
vec3 EnvBRDFApprox(vec3 F0, float roughness, float NdotV)
{
    const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * NdotV)) * r.x + r.y;
    vec2 ab = vec2(-1.04, 1.04) * a004 + r.zw;
    return F0 * ab.x + ab.y;
}

// ACES filmic tonemap 移至后处理链（PostProcess），本着色器输出线性 HDR 颜色

// 方向光阴影：世界坐标投影到光空间，与深度图比较（3x3 PCF），返回可见度（1 完全受光 / 0 全阴影）
float ComputeVisibility(vec3 normal, vec3 lightDirection)
{
    vec4 lightSpacePosition = u_LightSpaceMatrix * vec4(v_WorldPosition, 1.0);
    vec3 projected = lightSpacePosition.xyz / lightSpacePosition.w * 0.5 + 0.5;
    if (projected.z > 1.0) {
        return 1.0;  // 超出光源视锥远端
    }

    // 斜率缩放偏移：掠射角表面需要更大的偏移，避免自阴影条纹
    float bias = max(u_ShadowParams.y * (1.0 - dot(normal, lightDirection)), u_ShadowParams.y * 0.2);
    float texel = u_ShadowParams.x;

    float visibility = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float shadowDepth = texture(u_ShadowMap, projected.xy + vec2(x, y) * texel).r;
            visibility += projected.z - bias > shadowDepth ? 0.0 : 1.0;
        }
    }
    return visibility / 9.0;
}

void main()
{
    vec3 albedo = v_Color.rgb * u_Albedo.rgb;
    float metallic = clamp(u_Metallic, 0.0, 1.0);
    float roughness = clamp(u_Roughness, 0.045, 1.0);  // 下限避免粗糙度趋零时高光数值爆炸
    vec3 F0 = mix(vec3(0.04), albedo, metallic);        // 电介质基础反射率 0.04

    vec3 N = normalize(v_Normal);
    vec3 V = normalize(u_CameraPosition.xyz - v_WorldPosition);
    float NdotV = max(dot(N, V), 0.0001);

    vec3 directLight = vec3(0.0);

    for (int i = 0; i < u_Counts.x; ++i) {
        const int type = int(u_Lights[i].positionType.w + 0.5);

        vec3 lightDirection;  // 从着色点指向光源
        float attenuation = 1.0;

        if (type == 0) {  // 方向光：positionType.xyz 为传播方向，取反得到指向光源的方向；阴影可见度即衰减
            lightDirection = -normalize(u_Lights[i].positionType.xyz);
            attenuation = ComputeVisibility(N, lightDirection);
        } else {  // 点光源 / 聚光灯
            vec3 toLight = u_Lights[i].positionType.xyz - v_WorldPosition;
            float distance = length(toLight);
            lightDirection = toLight / max(distance, 0.0001);

            // 窗函数截断的平方反比衰减（intensity 为 1 单位距离处的照度）
            float w = clamp(1.0 - pow(distance / max(u_Lights[i].directionRange.w, 0.0001), 4.0), 0.0, 1.0);
            attenuation = (w * w) / max(distance * distance, 0.01);

            if (type == 2) {
                float theta = dot(lightDirection, normalize(-u_Lights[i].directionRange.xyz));
                float epsilon = max(u_Lights[i].params.x - u_Lights[i].params.y, 0.0001);
                attenuation *= clamp((theta - u_Lights[i].params.y) / epsilon, 0.0, 1.0);
            }
        }

        float NdotL = max(dot(N, lightDirection), 0.0);
        if (NdotL <= 0.0 || attenuation <= 0.0) {
            continue;
        }

        vec3 H = normalize(V + lightDirection);
        vec3 radiance = u_Lights[i].colorIntensity.rgb * u_Lights[i].colorIntensity.w;

        float D = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, lightDirection, roughness);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

        // 镜面项 + 能量守恒的漫反射项（金属无漫反射）
        vec3 specular = D * G * F / max(4.0 * NdotV * NdotL, 0.0001);
        vec3 kD = (1.0 - F) * (1.0 - metallic);

        directLight += (kD * albedo / PI + specular) * radiance * NdotL * attenuation;
    }

    // 环境光：漫反射（乘 AO）+ Karis 解析镜面近似
    vec3 ambient = u_Ambient.rgb * u_Ambient.a * u_AO;
    vec3 ambientDiffuse = ambient * albedo * (1.0 - metallic);
    vec3 ambientSpecular = ambient * EnvBRDFApprox(F0, roughness, NdotV);

    vec3 color = directLight + ambientDiffuse + ambientSpecular;

    // 输出线性 HDR 颜色（曝光 / ACES tonemap / gamma 由 PostProcess 统一处理）
    o_Color = vec4(color, v_Color.a * u_Albedo.a);
}
