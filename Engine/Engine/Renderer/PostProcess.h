#pragma once

#include <cstdint>

#include "Engine/Core/Base.h"
#include "Framebuffer.h"
#include "Shader.h"
#include "VertexArray.h"

namespace OiEngine {
	// HDR 后处理链：场景渲染到 RGBA16F 离屏目标（线性空间，亮度可超过 1），
	// 再由全屏 pass 做 tonemap / gamma 校正后输出到默认帧缓冲。
	//
	// 使用方式（场景侧无需关心目标与 pass 细节）：
	//   Renderer::SetHDREnabled(true);
	//   Renderer::SetExposure(m_exposure);
	//   ...
	//   Renderer::BeginScene(camera, scene);   // 此时绑定 HDR 目标并清屏
	//   ... 场景绘制（着色器输出线性 HDR 颜色，不做 tonemap / gamma）...
	//   Renderer::EndScene();                  // 跑后处理链，输出到默认帧缓冲
	class OI_API PostProcess {
	public:
		static void Init();
		static void Shutdown();

		// 窗口尺寸变化：重建 HDR 目标（未启用时忽略）
		static void Resize(uint32_t width, uint32_t height);

		static bool IsEnabled() { return s_enabled; }
		// 启用时才创建 HDR 目标（首次启用按当前窗口尺寸创建）
		static void SetEnabled(bool enabled);

		static float GetExposure() { return s_exposure; }
		static void SetExposure(float exposure) { s_exposure = exposure; }

		// 绑定 HDR 目标并清屏（Renderer::BeginScene 调用）
		static void BeginScene();
		// 解绑 HDR 目标并跑后处理链，输出到默认帧缓冲（Renderer::EndScene 调用）
		static void EndScene();

		// 调试用：HDR 颜色附件纹理 ID（可直接用 ImGui::Image 预览）
		static uint32_t GetColorTextureRendererID();
	private:
		static void CreateTarget(uint32_t width, uint32_t height);

	private:
		static Ref<Framebuffer> s_hdrTarget;
		static Ref<Shader> s_tonemapShader;
		static Ref<VertexArray> s_fullscreenQuad;

		static bool s_enabled;
		static float s_exposure;
		static uint32_t s_width;
		static uint32_t s_height;
	};
}
