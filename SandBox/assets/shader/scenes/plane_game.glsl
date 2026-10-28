#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;

layout(std140, binding = 0) uniform Camera {
    mat4 u_Projection;
    mat4 u_View;
    vec4 u_CameraPosition;  // xyz: 世界空间位置（本着色器未用）
};

uniform mat4 u_Transform;

out vec4 v_Color;
out vec2 v_Local;

void main()
{
    gl_Position = u_Projection * u_View * u_Transform * vec4(a_Position, 1.0);
    v_Color = a_Color;
    v_Local = a_Position.xy;  // 局部坐标，用于径向渐变
}

#type fragment
#version 450 core

layout(location = 0) out vec4 o_Color;

in vec4 v_Color;
in vec2 v_Local;

uniform vec4 u_Tint;
uniform float u_Falloff;  // 0 = 保留顶点色，> 0 = 径向衰减系数（光晕/粒子）

void main()
{
    vec4 color = v_Color * u_Tint;
    if (u_Falloff > 0.0) {
        float distance = length(v_Local);
        color.a *= max(0.0, 1.0 - distance) * u_Falloff;
    }
    if (color.a <= 0.001) {
        discard;
    }
    o_Color = color;
}
