#include "IndoorDemo.h"

#include <cmath>
#include <cstdint>
#include <imgui.h>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

#include "Engine/Core/Application.h"

using namespace OiEngine;

namespace {
    constexpr float PI = 3.14159265358979323846f;
    constexpr uint32_t SPHERE_SEGMENTS = 32;  // 球体经度分段
    constexpr uint32_t SPHERE_RINGS = 16;     // 球体纬度分段

    // 纹理单元约定：0 = 聚光灯阴影数组；1 = 方向光阴影贴图；2/3 = 环境探针（irradiance / prefilter）
    constexpr uint32_t SPOT_SHADOW_TEXTURE_UNIT = 0;
    constexpr uint32_t SUN_SHADOW_TEXTURE_UNIT = 1;
    constexpr uint32_t PROBE_IRRADIANCE_TEXTURE_UNIT = 2;
    constexpr uint32_t PROBE_PREFILTER_TEXTURE_UNIT = 3;

    // 方向光阴影覆盖的包围球（房间 + 家具）
    const glm::vec3 SHADOW_FOCUS(0.0f, 2.0f, 0.0f);
    constexpr float SHADOW_RADIUS = 11.0f;

    // 吊灯（聚光灯，投射阴影）
    const glm::vec3 DOWNLIGHT_POSITIONS[4] = {
        { -4.0f, 3.86f, -3.0f }, { 4.0f, 3.86f, -3.0f },
        { -4.0f, 3.86f,  3.0f }, { 4.0f, 3.86f,  3.0f },
    };
    // 壁灯（点光源）
    const glm::vec3 WALL_LAMP_POSITIONS[4] = {
        { -7.7f, 2.5f, -3.0f }, { -7.7f, 2.5f, 3.0f },
        {  7.7f, 2.5f, -3.0f }, {  7.7f, 2.5f, 3.0f },
    };
    const glm::vec3 WALL_LAMP_COLORS[4] = {
        { 1.0f, 0.72f, 0.42f }, { 1.0f, 0.72f, 0.42f },
        { 1.0f, 0.72f, 0.42f }, { 1.0f, 0.72f, 0.42f },
    };
    // 台灯（点光源）
    const glm::vec3 TABLE_LAMP_POSITIONS[2] = {
        { -1.25f, 1.06f, 0.5f }, { 1.25f, 1.06f, 0.5f },
    };

    // ---------- 网格生成（顶点格式：位置 + 法线 + 颜色，共 10 个 float） ----------

    void PushVertex(std::vector<float>& vertices, const glm::vec3& position, const glm::vec3& normal, const glm::vec4& color)
    {
        vertices.push_back(position.x);
        vertices.push_back(position.y);
        vertices.push_back(position.z);
        vertices.push_back(normal.x);
        vertices.push_back(normal.y);
        vertices.push_back(normal.z);
        vertices.push_back(color.x);
        vertices.push_back(color.y);
        vertices.push_back(color.z);
        vertices.push_back(color.w);
    }

    // 轴对齐单位立方体（边长 1，-0.5 ~ 0.5），每面独立法线
    void AppendBox(std::vector<float>& vertices, std::vector<uint32_t>& indices)
    {
        constexpr float h = 0.5f;
        const glm::vec3 positions[8] = {
            { -h, -h, -h }, {  h, -h, -h }, {  h,  h, -h }, { -h,  h, -h },
            { -h, -h,  h }, {  h, -h,  h }, {  h,  h,  h }, { -h,  h,  h },
        };
        const glm::vec3 normals[6] = {
            {  0.0f,  0.0f,  1.0f }, {  0.0f,  0.0f, -1.0f },
            { -1.0f,  0.0f,  0.0f }, {  1.0f,  0.0f,  0.0f },
            {  0.0f, -1.0f,  0.0f }, {  0.0f,  1.0f,  0.0f },
        };
        const uint32_t faces[6][4] = {
            { 4, 5, 6, 7 }, { 0, 3, 2, 1 }, { 0, 4, 7, 3 },
            { 1, 2, 6, 5 }, { 0, 1, 5, 4 }, { 3, 7, 6, 2 },
        };

        const glm::vec4 white(1.0f, 1.0f, 1.0f, 1.0f);
        for (uint32_t face = 0; face < 6; ++face) {
            const uint32_t base = static_cast<uint32_t>(vertices.size() / 10);
            for (uint32_t corner = 0; corner < 4; ++corner) {
                PushVertex(vertices, positions[faces[face][corner]], normals[face], white);
            }
            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 0);
            indices.push_back(base + 2);
            indices.push_back(base + 3);
        }
    }

    // 单位球（半径 0.5，球心在局部原点）；球体法线即位置方向
    void AppendSphere(std::vector<float>& vertices, std::vector<uint32_t>& indices)
    {
        const glm::vec4 white(1.0f, 1.0f, 1.0f, 1.0f);

        for (uint32_t ring = 0; ring <= SPHERE_RINGS; ++ring) {
            const float phi = PI * static_cast<float>(ring) / SPHERE_RINGS;
            for (uint32_t segment = 0; segment <= SPHERE_SEGMENTS; ++segment) {
                const float theta = 2.0f * PI * static_cast<float>(segment) / SPHERE_SEGMENTS;
                const glm::vec3 position(0.5f * std::sin(phi) * std::cos(theta),
                                         0.5f * std::cos(phi),
                                         0.5f * std::sin(phi) * std::sin(theta));
                PushVertex(vertices, position, glm::normalize(position), white);
            }
        }

        for (uint32_t ring = 0; ring < SPHERE_RINGS; ++ring) {
            for (uint32_t segment = 0; segment < SPHERE_SEGMENTS; ++segment) {
                const uint32_t topLeft = ring * (SPHERE_SEGMENTS + 1) + segment;
                const uint32_t bottomLeft = topLeft + SPHERE_SEGMENTS + 1;
                indices.push_back(topLeft);
                indices.push_back(bottomLeft);
                indices.push_back(topLeft + 1);
                indices.push_back(topLeft + 1);
                indices.push_back(bottomLeft);
                indices.push_back(bottomLeft + 1);
            }
        }
    }

    // 单位方形平面（XZ 平面，1 x 1，法线朝 +Y）
    void AppendQuad(std::vector<float>& vertices, std::vector<uint32_t>& indices)
    {
        const glm::vec3 normal(0.0f, 1.0f, 0.0f);
        const glm::vec4 white(1.0f, 1.0f, 1.0f, 1.0f);
        PushVertex(vertices, glm::vec3(-0.5f, 0.0f, -0.5f), normal, white);
        PushVertex(vertices, glm::vec3(-0.5f, 0.0f,  0.5f), normal, white);
        PushVertex(vertices, glm::vec3( 0.5f, 0.0f,  0.5f), normal, white);
        PushVertex(vertices, glm::vec3( 0.5f, 0.0f, -0.5f), normal, white);
        indices = { 0, 1, 2, 0, 2, 3 };
    }

    Ref<VertexArray> CreateMesh(std::vector<float>& vertices, std::vector<uint32_t>& indices)
    {
        Ref<VertexArray> vertexArray = VertexArray::Create();

        Ref<VertexBuffer> vertexBuffer = VertexBuffer::Create(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(float)));
        vertexBuffer->SetLayout({
            { ShaderDataType::Float3, "a_Position" },
            { ShaderDataType::Float3, "a_Normal" },
            { ShaderDataType::Float4, "a_Color" },
        });
        vertexArray->AddVertexBuffer(vertexBuffer);

        Ref<IndexBuffer> indexBuffer = IndexBuffer::Create(indices.data(), static_cast<uint32_t>(indices.size()));
        vertexArray->SetIndexBuffer(indexBuffer);
        return vertexArray;
    }

    // ---------- 着色器 ----------

    const char* LIT_VERTEX_SHADER = R"(
        #version 450 core

        layout(location = 0) in vec3 a_Position;
        layout(location = 1) in vec3 a_Normal;
        layout(location = 2) in vec4 a_Color;

        layout(std140, binding = 0) uniform Camera {
            mat4 u_Projection;
            mat4 u_View;
            vec4 u_CameraPosition;
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
            // 法线用逆转置矩阵变换，兼容非均匀缩放
            v_Normal = mat3(transpose(inverse(u_Transform))) * a_Normal;
            v_Color = a_Color;
        }
    )";

    // 室内 PBR 光照：Cook-Torrance 直接光照 + Forward+ cluster 光源列表 + 方向光 / 聚光灯阴影。
    // 输出线性 HDR 颜色（tonemap / gamma 由引擎后处理链处理），自发光用于灯具与窗户。
    const char* LIT_FRAGMENT_SHADER = R"(
        #version 450 core

        #define MAX_LIGHTS 256
        #define MAX_SHADOW_CASTERS 8
        #define MAX_LIGHTS_PER_CLUSTER 48
        #define TILE_SIZE 16
        #define PI 3.141592653589793

        // 与 C++ 侧 LightingData / LightData 的 std430 布局严格对应
        struct Light {
            vec4 positionType;   // xyz: 世界空间位置（Point/Spot）或传播方向（Directional）；w: 类型
            vec4 directionRange; // xyz: 方向（Spot）；w: 影响范围（Point/Spot）
            vec4 colorIntensity; // rgb: 颜色；w: 强度
            vec4 params;         // x: 内锥角余弦；y: 外锥角余弦（Spot）
            vec4 flags;          // x: 阴影矩阵槽位（-1 不投影）；y: 阴影深度偏移基数
        };

        layout(std430, binding = 3) readonly buffer Lighting {
            vec4  u_Ambient;      // rgb: 颜色；a: 强度
            ivec4 u_Counts;       // x: 光源数；y: 阴影矩阵数；z: 每 cluster 光源上限；w: 备用
            vec4  u_ClipPlanes;   // x: near；y: far；z: 1/log(far/near)；w: 备用
            ivec4 u_Grid;         // x/y: tilesX / tilesY；z: 深度切片数；w: 是否启用 cluster 剔除
            Light u_Lights[MAX_LIGHTS];
            mat4  u_ShadowMatrices[MAX_SHADOW_CASTERS];
        };

        // Forward+ 剔除结果（由 LightClustering 的 compute 写入）
        layout(std430, binding = 4) readonly buffer ClusterLightCounts {
            uint u_ClusterLightCounts[];
        };
        layout(std430, binding = 5) readonly buffer ClusterLightIndices {
            uint u_ClusterLightIndices[];
        };

        // 方向光阴影（与 ShadowData 的 std140 布局一致）
        layout(std140, binding = 2) uniform Shadow {
            mat4 u_LightSpaceMatrix;
            vec4 u_ShadowParams;  // x: 1/贴图尺寸；y: 深度偏移基数
        };

        layout(std140, binding = 0) uniform Camera {
            mat4 u_Projection;
            mat4 u_View;
            vec4 u_CameraPosition;
        };

        uniform sampler2D u_SunShadowMap;          // 方向光阴影（纹理单元 1）
        uniform sampler2DArray u_SpotShadowMaps;   // 聚光灯阴影（纹理单元 0，每盏一层）
        uniform float u_SpotShadowTexel;           // 1 / 聚光灯阴影贴图尺寸
        uniform samplerCube u_IrradianceMap;       // 环境探针：辐照度（纹理单元 2）
        uniform samplerCube u_PrefilteredMap;      // 环境探针：粗糙度分级的镜面反射（纹理单元 3）
        uniform float u_ProbeMaxLod;               // prefilter 的最大 mip（LOD = roughness * MaxLod）
        uniform float u_ProbeIntensity;            // 探针环境光强度
        uniform int u_UseProbe;                    // 是否使用环境探针（关闭时退回常量环境光）

        uniform vec4 u_Albedo;      // rgb: 基础色（线性空间）；a: 不透明度
        uniform vec3 u_Emissive;    // 自发光颜色（线性 HDR，可超过 1）
        uniform float u_Metallic;
        uniform float u_Roughness;
        uniform float u_AO;

        layout(location = 0) out vec4 o_Color;

        in vec3 v_WorldPosition;
        in vec3 v_Normal;
        in vec4 v_Color;

        // ---------- PBR 基础函数（GGX + Smith + Schlick） ----------

        float DistributionGGX(vec3 N, vec3 H, float roughness)
        {
            float a = roughness * roughness;
            float a2 = a * a;
            float NdotH = max(dot(N, H), 0.0);
            float NdotH2 = NdotH * NdotH;
            float denominator = NdotH2 * (a2 - 1.0) + 1.0;
            return a2 / max(PI * denominator * denominator, 0.0001);
        }

        float GeometrySchlickGGX(float NdotX, float roughness)
        {
            float r = roughness + 1.0;
            float k = (r * r) / 8.0;
            return NdotX / max(NdotX * (1.0 - k) + k, 0.0001);
        }

        float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
        {
            return GeometrySchlickGGX(max(dot(N, V), 0.0), roughness) *
                   GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
        }

        vec3 FresnelSchlick(float cosTheta, vec3 F0)
        {
            return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
        }

        vec3 EnvBRDFApprox(vec3 F0, float roughness, float NdotV)
        {
            const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
            const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
            vec4 r = roughness * c0 + c1;
            float a004 = min(r.x * r.x, exp2(-9.28 * NdotV)) * r.x + r.y;
            vec2 ab = vec2(-1.04, 1.04) * a004 + r.zw;
            return F0 * ab.x + ab.y;
        }

        // ---------- 阴影可见度 ----------

        // 方向光：世界坐标投影到光空间与深度图比较（3x3 PCF）
        float ComputeSunVisibility(vec3 normal, vec3 lightDirection)
        {
            vec4 lightSpacePosition = u_LightSpaceMatrix * vec4(v_WorldPosition, 1.0);
            vec3 projected = lightSpacePosition.xyz / lightSpacePosition.w * 0.5 + 0.5;
            if (projected.z > 1.0) {
                return 1.0;
            }

            float bias = max(u_ShadowParams.y * (1.0 - dot(normal, lightDirection)), u_ShadowParams.y * 0.2);
            float texel = u_ShadowParams.x;

            float visibility = 0.0;
            for (int x = -1; x <= 1; ++x) {
                for (int y = -1; y <= 1; ++y) {
                    float shadowDepth = texture(u_SunShadowMap, projected.xy + vec2(x, y) * texel).r;
                    visibility += projected.z - bias > shadowDepth ? 0.0 : 1.0;
                }
            }
            return visibility / 9.0;
        }

        // 聚光灯：按槽位取光空间矩阵，采样阴影贴图数组的对应层（3x3 PCF，bias 逐光源）
        float ComputeSpotVisibility(int slot, float bias, vec3 normal, vec3 lightDirection)
        {
            vec4 lightSpacePosition = u_ShadowMatrices[slot] * vec4(v_WorldPosition, 1.0);
            if (lightSpacePosition.w <= 0.0) {
                return 1.0;
            }

            vec3 projected = lightSpacePosition.xyz / lightSpacePosition.w * 0.5 + 0.5;
            if (projected.x < 0.0 || projected.x > 1.0 || projected.y < 0.0 || projected.y > 1.0 || projected.z > 1.0) {
                return 1.0;
            }

            float slopeBias = max(bias * (1.0 - dot(normal, lightDirection)), bias * 0.2);

            float visibility = 0.0;
            for (int x = -1; x <= 1; ++x) {
                for (int y = -1; y <= 1; ++y) {
                    float shadowDepth = texture(u_SpotShadowMaps,
                        vec3(projected.xy + vec2(x, y) * u_SpotShadowTexel, float(slot))).r;
                    visibility += projected.z - slopeBias > shadowDepth ? 0.0 : 1.0;
                }
            }
            return visibility / 9.0;
        }

        // ---------- 光照累加 ----------

        struct Surface {
            vec3 N;
            vec3 V;
            vec3 albedo;
            vec3 F0;
            float NdotV;
            float metallic;
            float roughness;
        };

        void AccumulateLight(const int index, const Surface surface, inout vec3 directLight)
        {
            const int type = int(u_Lights[index].positionType.w + 0.5);

            vec3 lightDirection;  // 从着色点指向光源
            float attenuation = 1.0;

            if (type == 0) {
                lightDirection = -normalize(u_Lights[index].positionType.xyz);
                attenuation = ComputeSunVisibility(surface.N, lightDirection);
            } else {
                const vec3 toLight = u_Lights[index].positionType.xyz - v_WorldPosition;
                const float distance = length(toLight);
                lightDirection = toLight / max(distance, 0.0001);

                // 窗函数截断的平方反比衰减（intensity 为 1 单位距离处的照度）
                const float w = clamp(1.0 - pow(distance / max(u_Lights[index].directionRange.w, 0.0001), 4.0), 0.0, 1.0);
                attenuation = (w * w) / max(distance * distance, 0.01);

                if (type == 2) {
                    const float theta = dot(lightDirection, normalize(-u_Lights[index].directionRange.xyz));
                    const float epsilon = max(u_Lights[index].params.x - u_Lights[index].params.y, 0.0001);
                    attenuation *= clamp((theta - u_Lights[index].params.y) / epsilon, 0.0, 1.0);

                    const int slot = int(u_Lights[index].flags.x + 0.5);
                    if (slot >= 0) {
                        attenuation *= ComputeSpotVisibility(slot, u_Lights[index].flags.y, surface.N, lightDirection);
                    }
                }
            }

            const float NdotL = max(dot(surface.N, lightDirection), 0.0);
            if (NdotL <= 0.0 || attenuation <= 0.0) {
                return;
            }

            const vec3 H = normalize(surface.V + lightDirection);
            const vec3 radiance = u_Lights[index].colorIntensity.rgb * u_Lights[index].colorIntensity.w;

            const float D = DistributionGGX(surface.N, H, surface.roughness);
            const float G = GeometrySmith(surface.N, surface.V, lightDirection, surface.roughness);
            const vec3 F = FresnelSchlick(max(dot(H, surface.V), 0.0), surface.F0);

            const vec3 specular = D * G * F / max(4.0 * surface.NdotV * NdotL, 0.0001);
            const vec3 kD = (1.0 - F) * (1.0 - surface.metallic);

            directLight += (kD * surface.albedo / PI + specular) * radiance * NdotL * attenuation;
        }

        // 片元所属 cluster（与 LightClustering 的 compute 源码使用同一套索引规则）
        uint ClusterIndexForFragment(vec3 worldPosition)
        {
            const float viewDepth = -(u_View * vec4(worldPosition, 1.0)).z;
            const float nearClip = max(u_ClipPlanes.x, 0.0001);
            const int slices = max(u_Grid.z, 1);
            const float sliceCoord = log(max(viewDepth, nearClip) / nearClip) * u_ClipPlanes.z * float(slices);
            const int slice = clamp(int(floor(sliceCoord)), 0, slices - 1);

            const ivec2 tiles = max(u_Grid.xy, ivec2(1));
            const ivec2 tile = clamp(ivec2(gl_FragCoord.xy / float(TILE_SIZE)), ivec2(0), tiles - 1);

            return uint((slice * tiles.y + tile.y) * tiles.x + tile.x);
        }

        void main()
        {
            const vec3 albedo = v_Color.rgb * u_Albedo.rgb;
            const float metallic = clamp(u_Metallic, 0.0, 1.0);
            const float roughness = clamp(u_Roughness, 0.045, 1.0);

            Surface surface;
            surface.N = normalize(v_Normal);
            surface.V = normalize(u_CameraPosition.xyz - v_WorldPosition);
            surface.NdotV = max(dot(surface.N, surface.V), 0.0001);
            surface.albedo = albedo;
            surface.metallic = metallic;
            surface.roughness = roughness;
            surface.F0 = mix(vec3(0.04), albedo, metallic);

            vec3 directLight = vec3(0.0);

            if (u_Grid.w != 0) {
                // Forward+：只遍历本 cluster 的光源列表
                const uint clusterIndex = ClusterIndexForFragment(v_WorldPosition);
                const uint lightCount = min(u_ClusterLightCounts[clusterIndex], uint(MAX_LIGHTS_PER_CLUSTER));
                for (uint i = 0u; i < lightCount; ++i) {
                    const uint lightIndex = u_ClusterLightIndices[clusterIndex * uint(MAX_LIGHTS_PER_CLUSTER) + i];
                    AccumulateLight(int(lightIndex), surface, directLight);
                }
            } else {
                // 回退路径：遍历全部光源（对照排查用）
                for (int i = 0; i < u_Counts.x; ++i) {
                    AccumulateLight(i, surface, directLight);
                }
            }

            // 环境光：探针（室内间接光 + 环境镜面）+ 常量环境光兜底
            vec3 ambientDiffuse = vec3(0.0);
            vec3 ambientSpecular = vec3(0.0);

            if (u_UseProbe != 0) {
                const vec3 reflectDirection = reflect(-surface.V, surface.N);
                const vec3 irradiance = texture(u_IrradianceMap, surface.N).rgb;
                const vec3 prefiltered = textureLod(u_PrefilteredMap, reflectDirection, roughness * u_ProbeMaxLod).rgb;

                ambientDiffuse = irradiance * albedo * (1.0 - metallic) * u_AO * u_ProbeIntensity;
                ambientSpecular = prefiltered * EnvBRDFApprox(surface.F0, roughness, surface.NdotV) * u_ProbeIntensity;
            }

            const vec3 constantAmbient = u_Ambient.rgb * u_Ambient.a * u_AO;
            ambientDiffuse += constantAmbient * albedo * (1.0 - metallic);
            ambientSpecular += constantAmbient * EnvBRDFApprox(surface.F0, roughness, surface.NdotV);

            // 线性 HDR 输出：曝光 / tonemap / gamma 由 PostProcess 统一处理
            o_Color = vec4(directLight + ambientDiffuse + ambientSpecular + u_Emissive, v_Color.a * u_Albedo.a);
        }
    )";

    // 阴影 pass 的深度着色器（与 ShadowDemo 相同：只做光源相机变换）
    const char* DEPTH_VERTEX_SHADER = R"(
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
    )";

    const char* DEPTH_FRAGMENT_SHADER = R"(
        #version 450 core

        void main()
        {
        }
    )";

    // 平涂着色器：光源 gizmo 等不受光照的几何
    const char* UNLIT_VERTEX_SHADER = R"(        #version 450 core

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
    )";

    const char* UNLIT_FRAGMENT_SHADER = R"(
        #version 450 core

        layout(location = 0) out vec4 o_Color;

        uniform vec4 u_Color;

        void main()
        {
            o_Color = u_Color;
        }
    )";

    // 环境探针捕获用的简化着色器：albedo × (环境光 + 阳光) + 自发光。
    // 探针不能依赖它自己（否则递归），所以这里不做 IBL，只用直射光近似一次弹射结果。
    const char* PROBE_VERTEX_SHADER = R"(
        #version 450 core

        layout(location = 0) in vec3 a_Position;
        layout(location = 1) in vec3 a_Normal;
        layout(location = 2) in vec4 a_Color;

        uniform mat4 u_FaceViewProjection;  // 探针提供的 cubemap 面视图投影
        uniform mat4 u_Transform;

        out vec3 v_WorldNormal;
        out vec4 v_Color;

        void main()
        {
            vec4 worldPosition = u_Transform * vec4(a_Position, 1.0);
            gl_Position = u_FaceViewProjection * worldPosition;
            v_WorldNormal = mat3(transpose(inverse(u_Transform))) * a_Normal;
            v_Color = a_Color;
        }
    )";

    const char* PROBE_FRAGMENT_SHADER = R"(
        #version 450 core

        uniform vec3 u_ProbeSunDirection;  // 阳光传播方向
        uniform vec3 u_ProbeSunColor;      // 阳光颜色 × 强度
        uniform vec3 u_ProbeAmbient;       // 常量环境光兜底
        uniform vec4 u_Albedo;
        uniform vec3 u_Emissive;

        layout(location = 0) out vec4 o_Color;

        in vec3 v_WorldNormal;
        in vec4 v_Color;

        void main()
        {
            const vec3 normal = normalize(v_WorldNormal);
            const vec3 albedo = v_Color.rgb * u_Albedo.rgb;
            const float sun = max(dot(normal, -u_ProbeSunDirection), 0.0);

            o_Color = vec4(albedo * (u_ProbeAmbient + u_ProbeSunColor * sun) + u_Emissive, 1.0);
        }
    )";
}

// ---------- IndoorCamera ----------

glm::vec3 IndoorCamera::GetPosition() const
{
    const float cosPitch = std::cos(pitch);
    const glm::vec3 orbit = target + distance * glm::vec3(cosPitch * std::cos(yaw), std::sin(pitch), cosPitch * std::sin(yaw));

    // 把机位夹在房间内（轨道相机很容易转到墙外 / 天花板外，室内观察必须限制在房间内部）
    return glm::clamp(orbit, glm::vec3(-7.5f, 0.4f, -5.5f), glm::vec3(7.5f, 3.7f, 5.5f));
}

glm::mat4 IndoorCamera::GetViewMatrix() const
{
    return glm::lookAt(GetPosition(), target, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 IndoorCamera::GetProjectionMatrix() const
{
    const uint32_t width = Application::Get().GetWindow().GetWidth();
    const uint32_t height = Application::Get().GetWindow().GetHeight();
    const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 16.0f / 9.0f;
    return glm::perspective(glm::radians(fovDegrees), aspect, GetNearClip(), GetFarClip());
}

// ---------- IndoorDemoLayer ----------

void IndoorDemoLayer::OnAttach()
{
    m_litShader = Shader::Create("IndoorDemoLit", LIT_VERTEX_SHADER, LIT_FRAGMENT_SHADER);
    m_depthShader = Shader::Create("IndoorDemoDepth", DEPTH_VERTEX_SHADER, DEPTH_FRAGMENT_SHADER);
    m_unlitShader = Shader::Create("IndoorDemoUnlit", UNLIT_VERTEX_SHADER, UNLIT_FRAGMENT_SHADER);
    m_probeShader = Shader::Create("IndoorDemoProbe", PROBE_VERTEX_SHADER, PROBE_FRAGMENT_SHADER);

    m_sunShadowMap = CreateRef<ShadowMap>(4096);
    m_sunShadowMap->SetBias(0.0028f);
    m_spotShadowMaps = ShadowMapArray::Create(1024, MaxShadowCasters);
    m_spotShadowMaps->SetBias(0.0025f);
    m_probe = EnvironmentProbe::Create(256, 32, 128);

    {
        std::vector<float> vertices;
        std::vector<uint32_t> indices;
        AppendBox(vertices, indices);
        m_boxMesh = CreateMesh(vertices, indices);
    }
    {
        std::vector<float> vertices;
        std::vector<uint32_t> indices;
        AppendSphere(vertices, indices);
        m_sphereMesh = CreateMesh(vertices, indices);
    }
    {
        std::vector<float> vertices;
        std::vector<uint32_t> indices;
        AppendQuad(vertices, indices);
        m_quadMesh = CreateMesh(vertices, indices);
    }

    BuildRoom();

    // 光照：微弱冷色环境光（室内间接光的粗略近似）+ 阳光方向光
    m_scene.ambientColor = glm::vec3(0.42f, 0.48f, 0.62f);
    m_scene.ambientIntensity = m_ambientIntensity;
    m_scene.directionalLights.push_back(DirectionalLight{});
    AddFixtureLights();
    // 初始的额外点光源数量（UI 滑动条变化时也会重建）
    RebuildFillLights();

    // 环境探针：把房间（含阳光与自发光灯具）捕获成 cubemap，供间接光使用
    RecomputeProbe();

    // 纹理单元约定只需设置一次（两个采样器都固定绑定）
    m_litShader->Bind();
    m_litShader->SetInt("u_SunShadowMap", SUN_SHADOW_TEXTURE_UNIT);
    m_litShader->SetInt("u_SpotShadowMaps", SPOT_SHADOW_TEXTURE_UNIT);
    m_litShader->SetInt("u_IrradianceMap", PROBE_IRRADIANCE_TEXTURE_UNIT);
    m_litShader->SetInt("u_PrefilteredMap", PROBE_PREFILTER_TEXTURE_UNIT);
    m_litShader->SetFloat("u_SpotShadowTexel", 1.0f / static_cast<float>(m_spotShadowMaps->GetSize()));
    m_litShader->SetFloat("u_ProbeMaxLod", static_cast<float>(m_probe->GetPrefilterLevelCount() - 1));

    RenderCommand::SetClearColor(glm::vec4(0.02f, 0.022f, 0.03f, 1.0f));

    OI_INFO("IndoorDemo OnAttach: {0} surfaces, {1} shadow maps",
        m_surfaces.size(), m_spotShadowMaps->GetLayerCount());
}

void IndoorDemoLayer::OnDetach() {}

void IndoorDemoLayer::AddSurface(const glm::vec3& position, const glm::vec3& scale, const glm::vec4& color,
                                 float metallic, float roughness, bool castsShadow,
                                 const glm::vec3& emissive, const glm::vec3& rotation)
{
    Surface surface;
    surface.mesh = m_boxMesh;
    surface.position = position;
    surface.rotation = rotation;
    surface.scale = scale;
    surface.color = color;
    surface.emissive = emissive;
    surface.metallic = metallic;
    surface.roughness = roughness;
    surface.castsShadow = castsShadow;
    m_surfaces.push_back(surface);
}

void IndoorDemoLayer::BuildRoom()
{
    // 房间：x ∈ [-8, 8]，y ∈ [0, 4]，z ∈ [-6, 6]（-z 墙上有窗户）
    const glm::vec4 floorColor(0.34f, 0.33f, 0.32f, 1.0f);
    const glm::vec4 wallColor(0.72f, 0.70f, 0.66f, 1.0f);
    const glm::vec4 ceilingColor(0.80f, 0.80f, 0.78f, 1.0f);

    // 地面 / 天花板（墙体参与投影，接收面不投影）
    AddSurface({ 0.0f, -0.1f, 0.0f }, { 16.0f, 0.2f, 12.0f }, floorColor, 0.0f, 0.55f, true);
    AddSurface({ 0.0f, 4.1f, 0.0f }, { 16.0f, 0.2f, 12.0f }, ceilingColor, 0.0f, 0.9f, true);

    // 侧墙 / 后墙
    AddSurface({ -8.15f, 2.0f, 0.0f }, { 0.3f, 4.0f, 12.0f }, wallColor, 0.0f, 0.85f, true);
    AddSurface({  8.15f, 2.0f, 0.0f }, { 0.3f, 4.0f, 12.0f }, wallColor, 0.0f, 0.85f, true);
    AddSurface({  0.0f, 2.0f, 6.15f }, { 16.0f, 4.0f, 0.3f }, wallColor, 0.0f, 0.85f, true);

    // 前墙（带窗户洞口：x ∈ [-2.2, 2.2]，y ∈ [1.2, 3.2]）—— 四块拼出洞口
    AddSurface({ 0.0f, 0.6f, -6.15f }, { 16.0f, 1.2f, 0.3f }, wallColor, 0.0f, 0.85f, true);
    AddSurface({ 0.0f, 3.6f, -6.15f }, { 16.0f, 0.8f, 0.3f }, wallColor, 0.0f, 0.85f, true);
    AddSurface({ -5.1f, 2.2f, -6.15f }, { 5.8f, 2.0f, 0.3f }, wallColor, 0.0f, 0.85f, true);
    AddSurface({  5.1f, 2.2f, -6.15f }, { 5.8f, 2.0f, 0.3f }, wallColor, 0.0f, 0.85f, true);

    // 窗玻璃：自发光（HDR），相当于把室外天光"塞"进房间里
    {
        Surface pane;
        pane.mesh = m_quadMesh;
        pane.position = glm::vec3(0.0f, 2.2f, -6.0f);
        pane.rotation = glm::vec3(glm::radians(90.0f), 0.0f, 0.0f);  // +Y 法线转到 +Z（朝向室内）
        pane.scale = glm::vec3(4.4f, 1.0f, 2.0f);
        pane.color = glm::vec4(0.9f, 0.94f, 1.0f, 1.0f);
        pane.emissive = glm::vec3(1.25f, 1.5f, 2.0f);
        pane.roughness = 0.9f;
        m_surfaces.push_back(pane);
    }

    // 家具：桌子（桌面 + 两条支腿）、地毯、书架、柱子、桌面上的金属球与摆件
    const glm::vec4 woodColor(0.42f, 0.26f, 0.14f, 1.0f);
    AddSurface({ 0.0f, 0.86f, 0.5f }, { 3.2f, 0.14f, 1.8f }, woodColor, 0.0f, 0.35f, true);
    AddSurface({ -1.3f, 0.42f, 0.5f }, { 0.18f, 0.86f, 1.6f }, woodColor, 0.0f, 0.55f, true);
    AddSurface({  1.3f, 0.42f, 0.5f }, { 0.18f, 0.86f, 1.6f }, woodColor, 0.0f, 0.55f, true);

    {
        Surface rug;
        rug.mesh = m_quadMesh;
        rug.position = glm::vec3(0.0f, 0.01f, 0.5f);
        rug.scale = glm::vec3(5.4f, 1.0f, 3.6f);
        rug.color = glm::vec4(0.34f, 0.14f, 0.13f, 1.0f);
        rug.roughness = 0.95f;
        m_surfaces.push_back(rug);
    }

    // 书架（+x 墙）：三层隔板 + 两个侧板
    const glm::vec4 shelfColor(0.36f, 0.30f, 0.24f, 1.0f);
    for (int level = 0; level < 3; ++level) {
        AddSurface({ 7.0f, 1.1f + 0.8f * level, -2.5f }, { 1.6f, 0.1f, 3.0f }, shelfColor, 0.0f, 0.6f, true);
    }
    AddSurface({ 7.75f, 1.9f, -4.0f }, { 0.12f, 2.4f, 0.12f }, shelfColor, 0.0f, 0.6f, true);
    AddSurface({ 7.75f, 1.9f, -1.0f }, { 0.12f, 2.4f, 0.12f }, shelfColor, 0.0f, 0.6f, true);

    // 柱子（靠近窗户一侧，制造光影层次）
    const glm::vec4 columnColor(0.62f, 0.60f, 0.58f, 1.0f);
    AddSurface({ -5.0f, 2.0f, -1.0f }, { 0.6f, 4.0f, 0.6f }, columnColor, 0.0f, 0.8f, true);
    AddSurface({ -5.0f, 2.0f,  3.5f }, { 0.6f, 4.0f, 0.6f }, columnColor, 0.0f, 0.8f, true);

    // 桌面摆设：金属球（反射环境光）+ 两个木盒
    AddSurface({ 0.9f, 1.25f, 0.4f }, { 0.7f, 0.7f, 0.7f }, glm::vec4(0.95f, 0.93f, 0.88f, 1.0f), 1.0f, 0.12f, true);
    AddSurface({ -0.6f, 1.12f, 0.9f }, { 0.4f, 0.36f, 0.4f }, glm::vec4(0.55f, 0.32f, 0.18f, 1.0f), 0.0f, 0.45f, true);
    AddSurface({ -0.6f, 1.12f, 0.2f }, { 0.4f, 0.36f, 0.4f }, glm::vec4(0.20f, 0.42f, 0.30f, 1.0f), 0.0f, 0.45f, true);

    // 书架上的彩色书脊
    const glm::vec4 bookColors[6] = {
        { 0.70f, 0.25f, 0.22f, 1.0f }, { 0.22f, 0.38f, 0.68f, 1.0f }, { 0.75f, 0.62f, 0.22f, 1.0f },
        { 0.25f, 0.55f, 0.35f, 1.0f }, { 0.55f, 0.30f, 0.60f, 1.0f }, { 0.80f, 0.45f, 0.20f, 1.0f },
    };
    for (int i = 0; i < 6; ++i) {
        AddSurface({ 7.05f, 1.32f + 0.8f * (i % 3), -3.8f + 0.32f * i },
                   { 0.9f, 0.34f, 0.22f }, bookColors[i], 0.0f, 0.7f, true);
    }

    // 灯具外观（自发光，不投影以免挡住自己的光斑）—— 位置与光源一一对应
    for (int i = 0; i < 4; ++i) {
        Surface fixture;
        fixture.mesh = m_boxMesh;
        fixture.position = DOWNLIGHT_POSITIONS[i] + glm::vec3(0.0f, 0.06f, 0.0f);
        fixture.scale = glm::vec3(0.5f, 0.1f, 0.5f);
        fixture.color = glm::vec4(0.9f, 0.88f, 0.82f, 1.0f);
        fixture.emissive = glm::vec3(0.9f, 0.82f, 0.62f);
        fixture.roughness = 0.5f;
        m_surfaces.push_back(fixture);
    }
    for (int i = 0; i < 4; ++i) {
        Surface fixture;
        fixture.mesh = m_sphereMesh;
        fixture.position = WALL_LAMP_POSITIONS[i];
        fixture.scale = glm::vec3(0.34f);
        fixture.color = glm::vec4(1.0f, 0.86f, 0.62f, 1.0f);
        fixture.emissive = glm::vec3(1.6f, 1.05f, 0.45f);
        fixture.roughness = 0.4f;
        m_surfaces.push_back(fixture);
    }
    for (int i = 0; i < 2; ++i) {
        Surface fixture;
        fixture.mesh = m_sphereMesh;
        fixture.position = TABLE_LAMP_POSITIONS[i] + glm::vec3(0.0f, 0.18f, 0.0f);
        fixture.scale = glm::vec3(0.30f);
        fixture.color = glm::vec4(1.0f, 0.88f, 0.68f, 1.0f);
        fixture.emissive = glm::vec3(1.4f, 0.95f, 0.45f);
        fixture.roughness = 0.4f;
        m_surfaces.push_back(fixture);
    }
}

void IndoorDemoLayer::AddFixtureLights()
{
    // 吊灯：4 盏向下聚光灯，各自投射阴影（阴影贴图数组的第 i 层 = 阴影矩阵槽位 i）
    for (int i = 0; i < 4; ++i) {
        SpotLight downlight;
        downlight.position = DOWNLIGHT_POSITIONS[i];
        downlight.direction = glm::vec3(0.0f, -1.0f, 0.0f);
        downlight.innerAngle = glm::radians(18.0f);
        downlight.outerAngle = m_downlightOuterAngle;
        downlight.range = 9.0f;
        downlight.color = glm::vec3(1.0f, 0.92f, 0.78f);
        downlight.intensity = m_downlightIntensity;
        downlight.shadowIndex = m_useSpotShadows ? i : -1;
        downlight.shadowBias = 0.0025f;
        m_scene.spotLights.push_back(downlight);
    }
}

void IndoorDemoLayer::RebuildFillLights()
{
    m_fillLights.clear();

    // 压力测试光源：在房间内随机撒点，颜色按调色板循环，缓慢漂移制造动态光照
    const glm::vec3 palette[6] = {
        { 0.35f, 0.55f, 1.00f }, { 1.00f, 0.45f, 0.30f }, { 0.40f, 1.00f, 0.60f },
        { 1.00f, 0.80f, 0.35f }, { 0.75f, 0.45f, 1.00f }, { 0.35f, 0.90f, 1.00f },
    };

    uint32_t seed = 1337u;
    const auto nextRandom = [&seed]() {
        // xorshift：只用于演示用的伪随机布灯，不需要高质量随机数
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        return static_cast<float>(seed % 10000u) / 10000.0f;
    };

    for (int i = 0; i < m_fillLightCount; ++i) {
        PointLight light;
        light.position = glm::vec3(
            -7.2f + 14.4f * nextRandom(),
             0.4f +  3.2f * nextRandom(),
            -5.4f + 10.8f * nextRandom());
        // 室内小范围点光源（台灯 / 蜡烛量级）：range 决定它落在多少个 cluster 里，
        // 200 盏大 range 光源会把每个 cluster 的光源列表撑爆（UI 里的 overflow 计数会飙升）
        light.range = 1.2f + 1.3f * nextRandom();
        light.color = palette[i % 6];
        light.intensity = m_fillLightIntensity;
        m_fillLights.push_back(light);
    }
}

void IndoorDemoLayer::RecomputeProbe()
{
    const glm::vec3 probePosition(0.0f, 2.0f, 0.0f);  // 房间中心附近的视点，捕获整间屋子

    m_probe->Capture(probePosition, [this](const glm::mat4& faceView, const glm::mat4& faceProjection) {
        const DirectionalLight& sun = m_scene.directionalLights.front();

        m_probeShader->Bind();
        m_probeShader->SetMat4("u_FaceViewProjection", faceProjection * faceView);
        m_probeShader->SetFloat3("u_ProbeSunDirection", sun.direction);
        // 直射光已经由主光照计算，这里只用一部分强度近似"墙面/地面反射出去的光"，
        // 否则会把阳光在间接光里重复计一遍
        m_probeShader->SetFloat3("u_ProbeSunColor", sun.color * sun.intensity * m_probeSunScale);
        m_probeShader->SetFloat3("u_ProbeAmbient",
            m_scene.ambientColor * m_scene.ambientIntensity + glm::vec3(0.02f, 0.02f, 0.025f));

        for (const Surface& surface : m_surfaces) {
            m_probeShader->SetMat4("u_Transform", SurfaceTransform(surface));
            m_probeShader->SetFloat4("u_Albedo", surface.color);
            m_probeShader->SetFloat3("u_Emissive", surface.emissive);
            RenderCommand::DrawIndexed(surface.mesh);
        }
    });
}

void IndoorDemoLayer::OnUpdate()
{
    if (!m_active) {
        return;
    }

    // 每帧同步一次 VSync 开关（关闭后可看到真实帧耗时，便于对比剔除开 / 关的开销）
    if (m_vsync != Application::Get().GetWindow().IsVSync()) {
        Application::Get().GetWindow().SetVSync(m_vsync);
    }

    const float deltaTime = Timer::GetDeltaTime();
    m_time += deltaTime;

    // 本场景走 HDR 路径 + Forward+ cluster 剔除（两者都由引擎全局开关控制）
    Renderer::SetHDREnabled(true);
    Renderer::SetExposure(m_exposure);
    Renderer::SetLightCullingEnabled(m_useClustering);

    if (m_autoRotate) {
        m_camera.yaw += deltaTime * 0.08f;
    }

    // 动态光照：额外点光源缓慢漂移
    if (m_fillLightsAnimated) {
        for (std::size_t i = 0; i < m_fillLights.size(); ++i) {
            const float phase = m_time * 0.35f + static_cast<float>(i) * 1.7f;
            m_fillLights[i].position.x += std::sin(phase) * deltaTime * 0.35f;
            m_fillLights[i].position.z += std::cos(phase * 0.7f) * deltaTime * 0.35f;
            m_fillLights[i].position.x = glm::clamp(m_fillLights[i].position.x, -7.2f, 7.2f);
            m_fillLights[i].position.z = glm::clamp(m_fillLights[i].position.z, -5.4f, 5.4f);
            m_fillLights[i].intensity = m_fillLightIntensity * (0.75f + 0.25f * std::sin(phase * 1.3f));
        }
    } else {
        for (auto& light : m_fillLights) {
            light.intensity = m_fillLightIntensity;
        }
    }

    // ---- 同步 UI 调整到光照参数 ----
    m_scene.ambientIntensity = m_ambientIntensity;

    DirectionalLight& sun = m_scene.directionalLights.front();
    sun.direction = -glm::vec3(std::cos(m_sunPitch) * std::cos(m_sunYaw),
                               std::sin(m_sunPitch),
                               std::cos(m_sunPitch) * std::sin(m_sunYaw));
    sun.color = glm::vec3(1.0f, 0.94f, 0.84f);
    sun.intensity = m_sunIntensity;

    for (int i = 0; i < 4; ++i) {
        SpotLight& downlight = m_scene.spotLights[i];
        downlight.intensity = m_downlightIntensity;
        downlight.outerAngle = m_downlightOuterAngle;
        downlight.innerAngle = m_downlightOuterAngle * 0.45f;
        downlight.shadowIndex = m_useSpotShadows ? i : -1;
    }

    // 点光源列表：固定灯具（壁灯 / 台灯）+ 压力测试光源，每帧重建（几十~几百个，开销可忽略）
    m_scene.pointLights.clear();
    for (int i = 0; i < 4; ++i) {
        m_scene.pointLights.push_back({
            .position = WALL_LAMP_POSITIONS[i],
            .range = 7.0f,
            .color = WALL_LAMP_COLORS[i],
            .intensity = m_wallLampIntensity,
        });
    }
    for (int i = 0; i < 2; ++i) {
        m_scene.pointLights.push_back({
            .position = TABLE_LAMP_POSITIONS[i],
            .range = 4.5f,
            .color = glm::vec3(1.0f, 0.80f, 0.55f),
            .intensity = m_tableLampIntensity,
        });
    }
    m_scene.pointLights.insert(m_scene.pointLights.end(), m_fillLights.begin(), m_fillLights.end());

    m_visibleLightCount = static_cast<int>(m_scene.directionalLights.size() + m_scene.pointLights.size() + m_scene.spotLights.size());

    RenderCommand::Clear();

    // 阴影 pass 必须在 BeginScene 之前（期间相机 UBO 里是光源相机）
    RenderShadowPasses();

    Renderer::BeginScene(m_camera, m_scene);

    m_spotShadowMaps->BindDepthTexture(SPOT_SHADOW_TEXTURE_UNIT);
    m_sunShadowMap->BindDepthTexture(SUN_SHADOW_TEXTURE_UNIT);
    m_probe->BindIrradiance(PROBE_IRRADIANCE_TEXTURE_UNIT);
    m_probe->BindPrefiltered(PROBE_PREFILTER_TEXTURE_UNIT);

    m_litShader->Bind();
    m_litShader->SetInt("u_UseProbe", m_useProbe ? 1 : 0);
    m_litShader->SetFloat("u_ProbeIntensity", m_probeIntensity);

    for (const Surface& surface : m_surfaces) {
        SubmitSurface(surface);
    }

    SubmitGizmos();

    Renderer::EndScene();

    // 统计（cluster 计数需要读回 GPU，低频刷新避免每帧同步等待）
    if (++m_statsFrameCounter >= 30) {
        m_statsFrameCounter = 0;
        m_clusterStats = m_useClustering ? LightClustering::ReadStats() : LightClustering::Stats{};
    }
}

void IndoorDemoLayer::RenderShadowPasses()
{
    const DirectionalLight& sun = m_scene.directionalLights.front();

    if (m_useSunShadow) {
        const DirectionalLightShadow shadow = ComputeDirectionalLightShadow(sun, SHADOW_FOCUS, SHADOW_RADIUS);
        Renderer::BeginShadowPass(m_sunShadowMap, shadow.view, shadow.projection);
        for (const Surface& surface : m_surfaces) {
            if (surface.castsShadow) {
                Renderer::Submit(m_depthShader, surface.mesh, SurfaceTransform(surface));
            }
        }
        Renderer::EndShadowPass(m_sunShadowMap);
    }

    if (m_useSpotShadows) {
        for (const SpotLight& spot : m_scene.spotLights) {
            if (spot.shadowIndex < 0) {
                continue;
            }

            const SpotLightShadow shadow = ComputeSpotLightShadow(spot);
            // 光空间矩阵随光照 SSBO 上传（BeginScene），着色器按 flags.x 的槽位取样
            Renderer::SetLightShadowMatrix(static_cast<uint32_t>(spot.shadowIndex), shadow.lightSpaceMatrix);

            Renderer::BeginShadowPass(m_spotShadowMaps, static_cast<uint32_t>(spot.shadowIndex), shadow.view, shadow.projection);
            for (const Surface& surface : m_surfaces) {
                if (surface.castsShadow) {
                    Renderer::Submit(m_depthShader, surface.mesh, SurfaceTransform(surface));
                }
            }
            Renderer::EndShadowPass(m_spotShadowMaps);
        }
    }
}

glm::mat4 IndoorDemoLayer::SurfaceTransform(const Surface& surface) const
{
    glm::mat4 transform = glm::translate(glm::mat4(1.0f), surface.position);
    if (surface.rotation.x != 0.0f) transform = glm::rotate(transform, surface.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
    if (surface.rotation.y != 0.0f) transform = glm::rotate(transform, surface.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
    if (surface.rotation.z != 0.0f) transform = glm::rotate(transform, surface.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
    return glm::scale(transform, surface.scale);
}

void IndoorDemoLayer::SubmitSurface(const Surface& surface)
{
    m_litShader->Bind();
    m_litShader->SetMat4("u_Transform", SurfaceTransform(surface));
    m_litShader->SetFloat4("u_Albedo", surface.color);
    m_litShader->SetFloat3("u_Emissive", surface.emissive);
    m_litShader->SetFloat("u_Metallic", surface.metallic);
    m_litShader->SetFloat("u_Roughness", surface.roughness);
    m_litShader->SetFloat("u_AO", surface.ao);
    RenderCommand::DrawIndexed(surface.mesh);
}

void IndoorDemoLayer::SubmitGizmos()
{
    if (!m_showLightGizmos) {
        return;
    }

    // 压力测试光源的可视化：小圆球（平涂，颜色即光源颜色）
    m_unlitShader->Bind();
    for (const PointLight& light : m_fillLights) {
        const float scale = glm::clamp(light.range * 0.03f, 0.04f, 0.09f);
        const glm::mat4 transform = glm::scale(glm::translate(glm::mat4(1.0f), light.position), glm::vec3(scale));
        m_unlitShader->SetMat4("u_Transform", transform);
        m_unlitShader->SetFloat4("u_Color", glm::vec4(light.color, 1.0f));
        RenderCommand::DrawIndexed(m_sphereMesh);
    }
}

void IndoorDemoLayer::OnImGuiRender()
{
    if (!m_active) {
        return;
    }

    ImGui::Begin("Indoor Demo", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("FPS: %.1f (%.2f ms)", io.Framerate, io.Framerate > 0.0f ? 1000.0f / io.Framerate : 0.0f);
    ImGui::Text("Lights: %d total (%d point, %d spot, %d dir)",
        m_visibleLightCount, static_cast<int>(m_scene.pointLights.size()),
        static_cast<int>(m_scene.spotLights.size()), static_cast<int>(m_scene.directionalLights.size()));

    ImGui::Separator();
    ImGui::Text("HDR / Post");
    ImGui::SliderFloat("Exposure", &m_exposure, 0.05f, 4.0f, "%.2f");
    ImGui::Checkbox("VSync", &m_vsync);

    ImGui::Separator();
    ImGui::Text("Environment probe (IBL)");
    ImGui::Checkbox("Use probe", &m_useProbe);
    ImGui::SliderFloat("Probe intensity", &m_probeIntensity, 0.0f, 3.0f, "%.2f");
    ImGui::SliderFloat("Probe sun scale", &m_probeSunScale, 0.0f, 1.0f, "%.2f");
    if (ImGui::Button("Recompute probe")) {
        RecomputeProbe();
    }
    ImGui::TextDisabled("捕获整间屋子 -> irradiance + GGX prefilter");

    ImGui::Separator();
    ImGui::Text("Sun (window light)");
    ImGui::SliderAngle("Sun yaw", &m_sunYaw, 0.0f, 360.0f);
    ImGui::SliderAngle("Sun pitch", &m_sunPitch, 5.0f, 80.0f);
    ImGui::SliderFloat("Sun intensity", &m_sunIntensity, 0.0f, 40.0f, "%.1f");
    ImGui::Checkbox("Sun shadow", &m_useSunShadow);

    ImGui::Separator();
    ImGui::Text("Fixtures");
    ImGui::SliderFloat("Downlight intensity", &m_downlightIntensity, 0.0f, 250.0f, "%.1f");
    ImGui::SliderAngle("Downlight angle", &m_downlightOuterAngle, 10.0f, 60.0f);
    ImGui::Checkbox("Spot shadows", &m_useSpotShadows);
    ImGui::SliderFloat("Wall lamp intensity", &m_wallLampIntensity, 0.0f, 150.0f, "%.1f");
    ImGui::SliderFloat("Table lamp intensity", &m_tableLampIntensity, 0.0f, 100.0f, "%.1f");
    ImGui::SliderFloat("Ambient", &m_ambientIntensity, 0.0f, 0.5f, "%.3f");

    ImGui::Separator();
    ImGui::Text("Stress test");
    if (ImGui::SliderInt("Extra point lights", &m_fillLightCount, 0, 240)) {
        RebuildFillLights();
    }
    ImGui::SliderFloat("Extra intensity", &m_fillLightIntensity, 0.0f, 40.0f, "%.1f");
    ImGui::Checkbox("Animate extra lights", &m_fillLightsAnimated);
    ImGui::Checkbox("Show light gizmos", &m_showLightGizmos);
    ImGui::Checkbox("Light culling (Forward+)", &m_useClustering);

    if (m_useClustering) {
        const glm::ivec4 grid = LightClustering::GetGridInfo();
        ImGui::Text("Cluster grid: %d x %d tiles x %d slices = %d clusters",
            grid.x, grid.y, grid.z, grid.x * grid.y * grid.z);
        ImGui::Text("Lights per cluster: avg %.1f, max %u, overflow %u",
            m_clusterStats.averageLights, m_clusterStats.maxLights, m_clusterStats.overflowClusters);
    } else {
        ImGui::TextDisabled("Culling off: every pixel loops over all lights");
    }

    ImGui::Separator();
    ImGui::Text("Debug");
    ImGui::Checkbox("Show sun shadow map", &m_showShadowMap);

    ImGui::Separator();
    ImGui::Text("Camera");
    ImGui::Checkbox("Auto rotate", &m_autoRotate);
    ImGui::SliderAngle("Camera yaw", &m_camera.yaw, 0.0f, 360.0f);
    ImGui::SliderAngle("Camera pitch", &m_camera.pitch, -10.0f, 80.0f);
    ImGui::SliderFloat("Distance", &m_camera.distance, 2.0f, 13.0f, "%.1f");
    ImGui::SliderFloat("FOV", &m_camera.fovDegrees, 30.0f, 90.0f, "%.0f");

    ImGui::Separator();
    ImGui::Checkbox("Show ImGui demo window", &m_showDemoWindow);
    ImGui::TextDisabled("F1 飞机大战  F2 太阳系  F5 阴影演示  F6 室内多光源");
    ImGui::End();

    if (m_showShadowMap) {
        ImGui::Begin("Shadow Map (sun)", &m_showShadowMap);
        ImGui::Image(static_cast<ImTextureID>(m_sunShadowMap->GetDepthTextureRendererID()), ImVec2(256.0f, 256.0f),
                     ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
        ImGui::End();
    }

    if (m_showDemoWindow) {
        ImGui::ShowDemoWindow(&m_showDemoWindow);
    }
}

void IndoorDemoLayer::OnEvent(Event& event) {}
