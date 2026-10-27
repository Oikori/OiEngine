#type vertex
#version 450 core

layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_TexCoord;

out vec2 v_TexCoord;

void main()
{
	// 顶点已处于 NDC，直接输出
	v_TexCoord = a_TexCoord;
	gl_Position = vec4(a_Position, 0.0, 1.0);
}

#type fragment
#version 450 core

layout(location = 0) out vec4 o_Color;

uniform sampler2D u_HDRBuffer;  // 线性 HDR 颜色（RGBA16F）
uniform float u_Exposure;       // tonemap 之前相乘的曝光

in vec2 v_TexCoord;

// ACES filmic tonemap（Narkowicz 近似），压缩高光避免过曝发白
vec3 ACESFilm(vec3 color)
{
	const float a = 2.51;
	const float b = 0.03;
	const float c = 2.43;
	const float d = 0.59;
	const float e = 0.14;
	return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

void main()
{
	vec3 color = texture(u_HDRBuffer, v_TexCoord).rgb;
	color = ACESFilm(color * u_Exposure);
	color = pow(color, vec3(1.0 / 2.2));  // gamma 校正（输出 sRGB）
	o_Color = vec4(color, 1.0);
}
