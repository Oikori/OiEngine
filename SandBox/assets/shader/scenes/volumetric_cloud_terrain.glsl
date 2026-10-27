#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec4 a_Color;

layout(std140, binding = 0) uniform Camera {
    mat4 u_Projection;
    mat4 u_View;
    vec4 u_CameraPosition;
};

out vec3 v_WorldPosition;
out vec3 v_Normal;
out vec4 v_Color;

void main()
{
    v_WorldPosition = a_Position;
    v_Normal = a_Normal;
    v_Color = a_Color;
    gl_Position = u_Projection * u_View * vec4(a_Position, 1.0);
}

#type fragment
#version 450 core

layout(location = 0) out vec4 o_Color;

in vec3 v_WorldPosition;
in vec3 v_Normal;
in vec4 v_Color;

layout(std140, binding = 0) uniform Camera {
    mat4 u_Projection;
    mat4 u_View;
    vec4 u_CameraPosition;
};

uniform vec3 u_SunDirection;     // 指向太阳
uniform vec3 u_SunColor;
uniform float u_SunIntensity;
uniform vec3 u_SkyAmbient;
uniform vec3 u_HazeColor;
uniform vec3 u_SunHazeColor;
uniform float u_FogDensity;
uniform float u_CloudBase;
uniform float u_CloudTop;
uniform vec2 u_WeatherOffset;
uniform sampler2D u_WeatherMap;

void main()
{
    vec3 N = normalize(v_Normal);
    vec3 toSun = normalize(u_SunDirection);
    float ndl = max(dot(N, toSun), 0.0);

    // 云影：用天气图的覆盖率粗略代替云阴影贴图（低于云顶的地形被云遮挡）
    float cloudShadow = 1.0;
    if (v_WorldPosition.y < u_CloudTop) {
        float coverage = texture(u_WeatherMap, v_WorldPosition.xz / 26000.0 + u_WeatherOffset).r;
        float below = 1.0 - smoothstep(u_CloudTop - 350.0, u_CloudTop + 100.0, v_WorldPosition.y);
        cloudShadow = 1.0 - 0.48 * smoothstep(0.45, 0.85, coverage) * below;
    }

    vec3 ambient = u_SkyAmbient * (0.55 + 0.45 * clamp(N.y, 0.0, 1.0));
    vec3 color = v_Color.rgb * (u_SunColor * u_SunIntensity * ndl * cloudShadow + ambient);

    vec3 toCamera = u_CameraPosition.xyz - v_WorldPosition;
    float distanceToCamera = length(toCamera);
    vec3 viewDir = toCamera / max(distanceToCamera, 0.001);
    float fog = 1.0 - exp(-distanceToCamera * u_FogDensity);
    float sunAmount = pow(max(dot(viewDir, toSun), 0.0), 3.0);
    vec3 haze = mix(u_HazeColor, u_SunHazeColor, sunAmount * 0.65);
    color = mix(color, haze, fog);

    o_Color = vec4(color, 1.0);
}
