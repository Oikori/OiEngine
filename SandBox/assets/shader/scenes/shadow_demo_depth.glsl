#type vertex
// 阴影 pass 的深度着色器：只做光源相机变换，不写颜色（帧缓冲只有深度附件）
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

void main()
{
}
