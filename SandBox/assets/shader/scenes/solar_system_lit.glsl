#type vertex
// 走引擎光照系统的着色器（用于行星；球心位于局部原点，位置方向即球面法线）
// 相机 / 光照数据来自 Renderer 上传的 UBO（binding = 0/1），光照打包布局见 Renderer.cpp
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;

layout(std140, binding = 0) uniform Camera {
    mat4 u_Projection;
    mat4 u_View;
    vec4 u_CameraPosition;  // xyz: 世界空间位置（本着色器未用）
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
    v_Normal = mat3(u_Transform) * a_Position;
    v_Color = a_Color;
}

#type fragment
#version 450 core

#define MAX_LIGHTS 256
#define MAX_SHADOW_CASTERS 8

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

layout(location = 0) out vec4 o_Color;

in vec3 v_WorldPosition;
in vec3 v_Normal;
in vec4 v_Color;

// 单个光源的漫反射贡献（Lambert）
vec3 ShadeLight(Light light, vec3 normal, vec3 worldPosition)
{
    const int type = int(light.positionType.w + 0.5);

    vec3 lightDirection;  // 从着色点指向光源
    float attenuation = 1.0;

    if (type == 0) {  // 方向光：positionType.xyz 为传播方向，取反得到指向光源的方向
        lightDirection = -normalize(light.positionType.xyz);
    } else {
        vec3 toLight = light.positionType.xyz - worldPosition;
        float distance = length(toLight);
        lightDirection = toLight / max(distance, 0.0001);

        // 点光源 / 聚光灯：窗函数截断的平方反比衰减，intensity 为 1 单位距离处的照度
        float w = clamp(1.0 - pow(distance / max(light.directionRange.w, 0.0001), 4.0), 0.0, 1.0);
        attenuation = (w * w) / max(distance * distance, 0.01);

        if (type == 2) {  // 聚光灯：内锥全亮，内锥到外锥之间线性过渡
            float theta = dot(lightDirection, normalize(-light.directionRange.xyz));
            float epsilon = max(light.params.x - light.params.y, 0.0001);
            attenuation *= clamp((theta - light.params.y) / epsilon, 0.0, 1.0);
        }
    }

    float diffuse = max(dot(normal, lightDirection), 0.0);
    return light.colorIntensity.rgb * light.colorIntensity.w * diffuse * attenuation;
}

void main()
{
    vec3 normal = normalize(v_Normal);

    vec3 color = u_Ambient.rgb * u_Ambient.a;
    for (int i = 0; i < u_Counts.x; ++i) {
        color += ShadeLight(u_Lights[i], normal, v_WorldPosition);
    }

    o_Color = vec4(v_Color.rgb * color, v_Color.a);
}
