#pragma once

#include "Engine/Core/Base.h"

namespace OiEngine {
	// 聚光灯阴影贴图数组：每盏投射阴影的聚光灯占一层深度图（NEAREST + 边界外返回 1.0，
	// 与 ShadowMap 的采样约定一致），光照着色器按层索引采样。
	// 流程与 ShadowMap 相同，由 Renderer::BeginShadowPass / EndShadowPass 驱动，见 AGENTS.md「阴影」一节。
	class OI_API ShadowMapArray {
	public:
		virtual ~ShadowMapArray() = default;

		virtual uint32_t GetSize() const = 0;
		virtual uint32_t GetLayerCount() const = 0;

		// 深度偏移基数（斜率缩放）：越大越不容易出现自阴影条纹，但阴影会"悬浮"
		virtual float GetBias() const = 0;
		virtual void SetBias(float bias) = 0;

		// 渲染到指定层：把该层挂到深度附件、绑定帧缓冲、设置视口为该层尺寸并清除深度
		virtual void BeginLayer(uint32_t layer) = 0;
		virtual void EndLayer() = 0;

		// 供光照着色器采样：把整张数组纹理绑定到指定纹理单元（shader 侧声明 sampler2DArray）
		virtual void BindDepthTexture(uint32_t slot = 0) const = 0;
		virtual uint32_t GetDepthTextureRendererID() const = 0;

		static Ref<ShadowMapArray> Create(uint32_t size = 1024, uint32_t layerCount = 4);
	};
}
