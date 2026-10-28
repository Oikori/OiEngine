#pragma once

#include <cstdint>

#include "Engine/Core/Base.h"
#include "Engine/Math.h"
#include "StorageBuffer.h"
#include "Shader.h"

namespace OiEngine {
	// Forward+ 光源剔除：把视锥按 [屏幕 tile] × [视空间深度切片] 划分成 cluster，
	// 用 compute shader 计算每个 cluster 覆盖的光源列表（写入 SSBO），光照着色器只遍历所在 cluster 的光源。
	//
	// 使用方式：Renderer::SetLightCullingEnabled(true)（见 Renderer::BeginScene —— 上传光源数据后自动派发）。
	// shader 侧需要声明 ClusterLightCounts（binding = 4）/ ClusterLightIndices（binding = 5）两个 buffer，
	// 并按同一套 cluster 索引规则定位（见 IndoorDemo 的光照着色器 / LightClustering.cpp 的 compute 源码）。
	class OI_API LightClustering {
	public:
		static constexpr uint32_t TileSize = 16;              // 屏幕 tile 边长（像素）
		static constexpr uint32_t SliceCount = 24;            // 视空间深度切片数
		static constexpr uint32_t MaxLightsPerCluster = 48;   // 每 cluster 光源上限（超出的被丢弃）

		static void Init();
		static void Shutdown();

		// 窗口尺寸变化：重建 cluster 网格与 SSBO（未初始化时忽略）
		static void Resize(uint32_t width, uint32_t height);

		static bool IsEnabled() { return s_enabled; }
		// 启用时按当前窗口尺寸建立 cluster 网格；关闭后 BeginScene 不再派发剔除（着色器退回全遍历）
		static void SetEnabled(bool enabled);

		// 派发剔除 compute（需要先更新光照 SSBO，Renderer::BeginScene 内部调用）
		static void Dispatch();

		// 打包进光照 SSBO 的 u_Grid：x/y: tilesX / tilesY；z: 深度切片数；w: 是否启用剔除
		static glm::ivec4 GetGridInfo();

		// 调试统计：读回 cluster 光源计数（会等待 GPU，UI 里应低频调用）
		struct Stats {
			uint32_t clusterCount = 0;
			uint32_t maxLights = 0;
			float averageLights = 0.0f;
			uint32_t overflowClusters = 0;  // 光源数超过 MaxLightsPerCluster 的 cluster 数
		};
		static Stats ReadStats();
	private:
		static void CreateGrid(uint32_t width, uint32_t height);
	private:
		static Ref<Shader> s_cullShader;
		static Ref<StorageBuffer> s_countBuffer;  // 每 cluster 光源数（uint，binding = 4）
		static Ref<StorageBuffer> s_indexBuffer;  // 每 cluster 光源索引（uint，binding = 5）

		static uint32_t s_tilesX;
		static uint32_t s_tilesY;
		static bool s_enabled;
	};
}
