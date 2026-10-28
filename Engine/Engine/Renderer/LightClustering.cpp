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
	}

	Ref<Shader> LightClustering::s_cullShader = nullptr;
	Ref<StorageBuffer> LightClustering::s_countBuffer = nullptr;
	Ref<StorageBuffer> LightClustering::s_indexBuffer = nullptr;

	uint32_t LightClustering::s_tilesX = 0;
	uint32_t LightClustering::s_tilesY = 0;
	bool LightClustering::s_enabled = false;

	void LightClustering::Init()
	{
		s_cullShader = Shader::Create("assets/shader/engine/light_cluster_cull.comp.glsl");
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
