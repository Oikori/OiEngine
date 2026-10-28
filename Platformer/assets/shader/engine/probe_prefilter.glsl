#type vertex
// GGX prefilter：按粗糙度做重要性采样，输出分级模糊的环境镜面反射
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
uniform float u_Roughness;
uniform float u_Resolution;  // 当前 mip 的分辨率（用于抑制高光闪烁）

in vec3 v_LocalPosition;
layout(location = 0) out vec4 o_Color;

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

vec3 TangentToWorld(vec3 tangentSample, vec3 n)
{
	vec3 up = abs(n.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	vec3 tangentX = normalize(cross(up, n));
	vec3 tangentY = cross(n, tangentX);
	return tangentX * tangentSample.x + tangentY * tangentSample.y + n * tangentSample.z;
}

float DistributionGGX(float NdotH, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float denominator = NdotH * NdotH * (a2 - 1.0) + 1.0;
	return a2 / max(PI * denominator * denominator, 0.0001);
}

// 由低差异序列生成 GGX 半程向量（importance sampling）
vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
	float a = roughness * roughness;
	float phi = 2.0 * PI * Xi.x;
	float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
	float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));

	vec3 H = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
	return TangentToWorld(H, N);
}

void main()
{
	const vec3 N = normalize(v_LocalPosition);
	const vec3 R = N;
	const vec3 V = R;

	float totalWeight = 0.0;
	vec3 prefiltered = vec3(0.0);

	for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
		const vec2 Xi = Hammersley(i, SAMPLE_COUNT);
		const vec3 H = ImportanceSampleGGX(Xi, N, u_Roughness);
		const vec3 L = normalize(2.0 * dot(V, H) * H - V);

		const float NdotL = max(dot(N, L), 0.0);
		if (NdotL > 0.0) {
			prefiltered += texture(u_Environment, L).rgb * NdotL;
			totalWeight += NdotL;
		}
	}

	o_Color = vec4(prefiltered / max(totalWeight, 0.0001), 1.0);
}
