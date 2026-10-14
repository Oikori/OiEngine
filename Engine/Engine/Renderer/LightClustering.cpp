#include "LightClustering.h"

#include <algorithm>
#include <vector>

#include "RenderCommand.h"
#include "Engine/Core/Application.h"

namespace OiEngine {
	namespace {
		constexpr uint32_t CountSSBOBinding = 4;   // shader 里写作 layout(std430, binding = 4) buffer ClusterLightCounts
		constexpr uint32_t IndexSSBOBinding = 5;   // shader 里写作 layout(std430, binding = 5) buffer ClusterLightIndices
		constexpr uint32_t GroupSize = 64;         // 与着色器 local_size_x 一致

		// cluster 光源剔除 compute：
		// 每个 invocation 处理一个 cluster —— 先由 tile + 深度切片反推出该 cluster 在视空间的 AABB，
		// 再逐光源做保守的球 / AABB 相交测试，命中的光源索引通过原子计数写入列表。
		//
		// 索引布局（必须与着色器侧 ClusterIndexForFragment 一致）：slice 优先，其次 tile 行，最后 tile 列
		//   clusterIndex = (slice * tilesY + tileY) * tilesX + tileX
		const char* CLUSTER_CULL_SHADER = R"(
			#version 450 core

			#define MAX_LIGHTS 256
			#define MAX_SHADOW_CASTERS 8
			#define TILE_SIZE 16              // 与 LightClustering::TileSize 一致
			#define MAX_LIGHTS_PER_CLUSTER 48 // 与 LightClustering::MaxLightsPerCluster 一致

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

			layout(std430, binding = 4) buffer ClusterLightCounts {
				uint u_ClusterLightCounts[];
			};

			layout(std430, binding = 5) buffer ClusterLightIndices {
				uint u_ClusterLightIndices[];
			};

			layout(std140, binding = 0) uniform Camera {
				mat4 u_Projection;
				mat4 u_View;
				vec4 u_CameraPosition;
			};

			layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

			void main()
			{
				const uint clusterIndex = gl_GlobalInvocationID.x;
				const uint tilesX = uint(u_Grid.x);
				const uint tilesY = uint(u_Grid.y);
				const uint slices = uint(u_Grid.z);
				if (clusterIndex >= tilesX * tilesY * slices) {
					return;
				}

				const uint tilesPerSlice = tilesX * tilesY;
				const uint slice = clusterIndex / tilesPerSlice;
				const uint tile = clusterIndex % tilesPerSlice;
				const uint tileX = tile % tilesX;
				const uint tileY = tile / tilesX;

				// 深度切片范围（指数分布，与着色器侧 ClusterIndexForFragment 的映射互逆）
				const float nearClip = u_ClipPlanes.x;
				const float farClip = u_ClipPlanes.y;
				const float sliceNear = nearClip * pow(farClip / nearClip, float(slice) / float(slices));
				const float sliceFar = nearClip * pow(farClip / nearClip, float(slice + 1u) / float(slices));

				// tile 的 NDC 范围
				const vec2 ndcMin = vec2(float(tileX) / float(tilesX), float(tileY) / float(tilesY)) * 2.0 - 1.0;
				const vec2 ndcMax = vec2(float(tileX + 1u) / float(tilesX), float(tileY + 1u) / float(tilesY)) * 2.0 - 1.0;

				// 视空间 AABB：视空间里 z 为负（朝前），x = ndcX * (-z) / P00，y = ndcY * (-z) / P11
				const vec2 projScale = vec2(u_Projection[0][0], u_Projection[1][1]);
				vec3 aabbMin = vec3(1e30);
				vec3 aabbMax = vec3(-1e30);
				for (int corner = 0; corner < 4; ++corner) {
					const vec2 ndc = vec2((corner & 1) == 0 ? ndcMin.x : ndcMax.x, (corner & 2) == 0 ? ndcMin.y : ndcMax.y);
					const vec3 atNear = vec3(ndc * sliceNear / projScale, -sliceNear);
					const vec3 atFar = vec3(ndc * sliceFar / projScale, -sliceFar);
					aabbMin = min(aabbMin, min(atNear, atFar));
					aabbMax = max(aabbMax, max(atNear, atFar));
				}

				const uint lightCount = uint(u_Counts.x);
				for (uint i = 0u; i < lightCount; ++i) {
					const int type = int(u_Lights[i].positionType.w + 0.5);

					// 方向光影响所有 cluster；点光源 / 聚光灯用保守的包围球（聚光灯不收缩到光锥，宁可多算不漏算）
					if (type != 0) {
						const vec3 viewCenter = (u_View * vec4(u_Lights[i].positionType.xyz, 1.0)).xyz;
						const float radius = max(u_Lights[i].directionRange.w, 0.001);
						const vec3 closest = clamp(viewCenter, aabbMin, aabbMax);
						const vec3 delta = closest - viewCenter;
						if (dot(delta, delta) > radius * radius) {
							continue;
						}
					}

					const uint slot = atomicAdd(u_ClusterLightCounts[clusterIndex], 1u);
					if (slot < MAX_LIGHTS_PER_CLUSTER) {
						u_ClusterLightIndices[clusterIndex * MAX_LIGHTS_PER_CLUSTER + slot] = i;
					}
				}
			}
		)";
	}

	Ref<Shader> LightClustering::s_cullShader = nullptr;
	Ref<StorageBuffer> LightClustering::s_countBuffer = nullptr;
	Ref<StorageBuffer> LightClustering::s_indexBuffer = nullptr;

	uint32_t LightClustering::s_tilesX = 0;
	uint32_t LightClustering::s_tilesY = 0;
	bool LightClustering::s_enabled = false;

	void LightClustering::Init()
	{
		s_cullShader = Shader::CreateCompute("LightClusterCull", CLUSTER_CULL_SHADER);
	}

	void LightClustering::Shutdown()
	{
		s_indexBuffer.reset();
		s_countBuffer.reset();
		s_cullShader.reset();

		s_tilesX = 0;
		s_tilesY = 0;
		s_enabled = false;
	}

	void LightClustering::CreateGrid(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0) {
			return;
		}

		s_tilesX = std::max(1u, (width + TileSize - 1) / TileSize);
		s_tilesY = std::max(1u, (height + TileSize - 1) / TileSize);

		const uint32_t clusterCount = s_tilesX * s_tilesY * SliceCount;
		s_countBuffer = StorageBuffer::Create(clusterCount * sizeof(uint32_t), CountSSBOBinding);
		s_indexBuffer = StorageBuffer::Create(clusterCount * MaxLightsPerCluster * sizeof(uint32_t), IndexSSBOBinding);

		OI_INFO("Light clustering grid: {0} x {1} tiles x {2} slices = {3} clusters ({4} MB indices)",
			s_tilesX, s_tilesY, SliceCount, clusterCount,
			static_cast<double>(clusterCount) * MaxLightsPerCluster * sizeof(uint32_t) / (1024.0 * 1024.0));
	}

	void LightClustering::Resize(uint32_t width, uint32_t height)
	{
		if (!s_countBuffer || width == 0 || height == 0) {
			return;
		}

		const uint32_t tilesX = std::max(1u, (width + TileSize - 1) / TileSize);
		const uint32_t tilesY = std::max(1u, (height + TileSize - 1) / TileSize);
		if (tilesX == s_tilesX && tilesY == s_tilesY) {
			return;
		}

		CreateGrid(width, height);
	}

	void LightClustering::SetEnabled(bool enabled)
	{
		if (enabled && !s_countBuffer) {
			const Window& window = Application::Get().GetWindow();
			CreateGrid(window.GetWidth(), window.GetHeight());
		}
		s_enabled = enabled;
	}

	glm::ivec4 LightClustering::GetGridInfo()
	{
		if (!s_enabled || !s_countBuffer) {
			return glm::ivec4(0);
		}
		return glm::ivec4(static_cast<int>(s_tilesX), static_cast<int>(s_tilesY), static_cast<int>(SliceCount), 1);
	}

	void LightClustering::Dispatch()
	{
		if (!s_enabled || !s_countBuffer || !s_cullShader) {
			return;
		}

		const uint32_t clusterCount = s_tilesX * s_tilesY * SliceCount;
		// 清空光源计数（GPU 侧清零，不需要额外 compute pass）
		s_countBuffer->Clear(0, clusterCount * sizeof(uint32_t));

		s_cullShader->Dispatch((clusterCount + GroupSize - 1) / GroupSize, 1, 1);
		// compute 写入的 cluster 列表要被后续绘制的片元着色器读到
		RenderCommand::MemoryBarrierShaderStorage();
	}

	LightClustering::Stats LightClustering::ReadStats()
	{
		Stats stats;

		if (!s_countBuffer) {
			return stats;
		}

		stats.clusterCount = s_tilesX * s_tilesY * SliceCount;
		if (stats.clusterCount == 0) {
			return stats;
		}

		std::vector<uint32_t> counts(stats.clusterCount);
		s_countBuffer->GetData(counts.data(), static_cast<uint32_t>(counts.size() * sizeof(uint32_t)));

		uint64_t total = 0;
		for (const uint32_t count : counts) {
			total += count;
			stats.maxLights = std::max(stats.maxLights, count);
			if (count > MaxLightsPerCluster) {
				++stats.overflowClusters;
			}
		}
		stats.averageLights = static_cast<float>(total) / static_cast<float>(stats.clusterCount);

		return stats;
	}
}
