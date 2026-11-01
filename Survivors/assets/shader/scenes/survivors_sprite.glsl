#type vertex
#version 450 core

// 贴图精灵：和 survivors.glsl 走同一个相机 UBO，只是把顶点色换成纹理采样。
// 顶点布局：position(xyz) + texcoord(uv)，原点在精灵中心（与程序化几何的约定一致）。
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;

layout(std140, binding = 0) uniform Camera {
    mat4 u_Projection;
    mat4 u_View;
    vec4 u_CameraPosition;
};

uniform mat4 u_Transform;

out vec2 v_TexCoord;

void main()
{
    gl_Position = u_Projection * u_View * u_Transform * vec4(a_Position, 1.0);
    v_TexCoord = a_TexCoord;
}

#type fragment
#version 450 core

layout(location = 0) out vec4 o_Color;

// 采样器固定绑到 unit 0，省掉每帧 SetInt
layout(binding = 0) uniform sampler2D u_Texture;

in vec2 v_TexCoord;

uniform vec4 u_Tint;        // 整体染色：rgb 乘贴图，a 乘透明度
uniform float u_Flash;      // 受击闪白 / Boss 蓄力泛红：与 u_FlashColor 混合（0..1）
uniform vec4 u_FlashColor;  // 闪光的颜色

void main()
{
    vec4 texel = texture(u_Texture, v_TexCoord);
    if (texel.a <= 0.004) {
        discard;
    }

    vec3 color = texel.rgb * u_Tint.rgb;
    color = mix(color, u_FlashColor.rgb, clamp(u_Flash, 0.0, 1.0));
    o_Color = vec4(color, texel.a * u_Tint.a);
}
