#type vertex
// 太阳光晕：正对相机的区域最亮，边缘渐隐（相机位置来自 UBO，binding = 0）
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;

layout(std140, binding = 0) uniform Camera {
    mat4 u_Projection;
    mat4 u_View;
    vec4 u_CameraPosition;  // xyz: 世界空间位置（片元着色器使用）
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
    v_Normal = a_Position;  // 球心位于局部原点，位置方向即外法线方向
    v_Color = a_Color;
}

#type fragment
#version 450 core

layout(location = 0) out vec4 o_Color;

// 同一 uniform 块在不同 stage 中必须定义一致
layout(std140, binding = 0) uniform Camera {
    mat4 u_Projection;
    mat4 u_View;
    vec4 u_CameraPosition;  // xyz: 世界空间位置
};

in vec3 v_WorldPosition;
in vec3 v_Normal;
in vec4 v_Color;

void main()
{
    vec3 viewDirection = normalize(u_CameraPosition.xyz - v_WorldPosition);
    float facing = abs(dot(normalize(v_Normal), viewDirection));
    o_Color = vec4(v_Color.rgb, v_Color.a * pow(facing, 1.5));
}
