#type vertex
// 环境探针捕获用的简化着色器：albedo × (环境光 + 阳光) + 自发光。
// 探针不能依赖它自己（否则递归），所以这里不做 IBL，只用直射光近似一次弹射结果。
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec4 a_Color;

uniform mat4 u_FaceViewProjection;  // 探针提供的 cubemap 面视图投影
uniform mat4 u_Transform;

out vec3 v_WorldNormal;
out vec4 v_Color;

void main()
{
    vec4 worldPosition = u_Transform * vec4(a_Position, 1.0);
    gl_Position = u_FaceViewProjection * worldPosition;
    v_WorldNormal = mat3(transpose(inverse(u_Transform))) * a_Normal;
    v_Color = a_Color;
}

#type fragment
#version 450 core

uniform vec3 u_ProbeSunDirection;  // 阳光传播方向
uniform vec3 u_ProbeSunColor;      // 阳光颜色 × 强度
uniform vec3 u_ProbeAmbient;       // 常量环境光兜底
uniform vec4 u_Albedo;
uniform vec3 u_Emissive;

layout(location = 0) out vec4 o_Color;

in vec3 v_WorldNormal;
in vec4 v_Color;

void main()
{
    const vec3 normal = normalize(v_WorldNormal);
    const vec3 albedo = v_Color.rgb * u_Albedo.rgb;
    const float sun = max(dot(normal, -u_ProbeSunDirection), 0.0);

    o_Color = vec4(albedo * (u_ProbeAmbient + u_ProbeSunColor * sun) + u_Emissive, 1.0);
}
