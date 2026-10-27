#type vertex
// 不受光照影响的着色器（用于太阳、轨道线、土星环、光晕）
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

void main()
{
    gl_Position = u_Projection * u_View * u_Transform * vec4(a_Position, 1.0);
    v_Color = a_Color;
}

#type fragment
#version 450 core

layout(location = 0) out vec4 o_Color;

in vec4 v_Color;

void main()
{
    o_Color = v_Color;
}
