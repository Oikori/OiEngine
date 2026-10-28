#type vertex
// 太阳指示球：不受光照的平涂
#version 450 core

layout(location = 0) in vec3 a_Position;

layout(std140, binding = 0) uniform Camera {
    mat4 u_Projection;
    mat4 u_View;
    vec4 u_CameraPosition;
};

uniform mat4 u_Transform;

void main()
{
    gl_Position = u_Projection * u_View * u_Transform * vec4(a_Position, 1.0);
}

#type fragment
#version 450 core

layout(location = 0) out vec4 o_Color;

void main()
{
    o_Color = vec4(1.0, 0.84, 0.48, 1.0);
}
