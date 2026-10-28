#type vertex
// irradiance 卷积：对半球做余弦加权采样，得到漫反射环境辐照度
#version 450 core

layout(location = 0) in vec3 a_Position;

uniform mat4 u_FaceProjection;
uniform mat4 u_FaceView;

out vec3 v_LocalPosition;

void main()
{
	v_LocalPosition = a_Position;
	gl_Position = u_FaceProjection * u_FaceView * vec4(a_Position, 1.0);
}

#type fragment
#version 450 core

#define PI 3.141592653589793
#define SAMPLE_COUNT 256u

uniform samplerCube u_Environment;

in vec3 v_LocalPosition;
layout(location = 0) out vec4 o_Color;

// 低差异序列（Hammersley）
float RadicalInverseVdC(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10;
}

vec2 Hammersley(uint i, uint n)
{
	return vec2(float(i) / float(n), RadicalInverseVdC(i));
}

// 以 N 为基准的正交基
vec3 TangentToWorld(vec3 tangentSample, vec3 n)
{
	vec3 up = abs(n.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	vec3 tangentX = normalize(cross(up, n));
	vec3 tangentY = cross(n, tangentX);
	return tangentX * tangentSample.x + tangentY * tangentSample.y + n * tangentSample.z;
}

void main()
{
	const vec3 N = normalize(v_LocalPosition);
	vec3 irradiance = vec3(0.0);

	for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
		const vec2 Xi = Hammersley(i, SAMPLE_COUNT);

		// 余弦加权半球采样（cos 因子已包含在采样分布中）
		const float phi = 2.0 * PI * Xi.x;
		const float cosTheta = sqrt(1.0 - Xi.y);
		const float sinTheta = sqrt(Xi.y);
		const vec3 tangentSample = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

		irradiance += texture(u_Environment, TangentToWorld(tangentSample, N)).rgb;
	}

	irradiance = PI * irradiance / float(SAMPLE_COUNT);
	o_Color = vec4(irradiance, 1.0);
}
