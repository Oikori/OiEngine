#include "OpenGLEnvironmentProbe.h"

#include <cmath>
#include <cstddef>
#include <vector>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include "Engine/Core/Application.h"
#include "Engine/Renderer/Buffer.h"
#include "Engine/Renderer/RenderCommand.h"

namespace OiEngine {
	namespace {
		// 立方体（24 顶点 / 36 索引）：捕获与卷积 pass 都直接渲染它，方向 = 局部坐标
		const float CUBE_POSITIONS[] = {
			 1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f,
			-1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,
			-1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,
			-1.0f, -1.0f,  1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,
			-1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,
			 1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,
		};
		const uint32_t CUBE_INDICES[] = {
			0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7, 8, 9, 10, 8, 10, 11,
			12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23,
		};

		const char* CUBE_VERTEX_SHADER = R"(
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
		)";

		// irradiance 卷积：对半球做余弦加权采样，得到漫反射环境辐照度
		const char* IRRADIANCE_FRAGMENT_SHADER = R"(
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
		)";

		// GGX prefilter：按粗糙度做重要性采样，输出分级模糊的环境镜面反射
		const char* PREFILTER_FRAGMENT_SHADER = R"(
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
		)";
	}

	OpenGLEnvironmentProbe::OpenGLEnvironmentProbe(uint32_t captureSize, uint32_t irradianceSize, uint32_t prefilterSize)
		: m_captureSize(captureSize), m_irradianceSize(irradianceSize), m_prefilterSize(prefilterSize)
	{
		// mip 链：128 -> 64 -> ... -> 1
		m_prefilterLevels = 1;
		for (uint32_t size = m_prefilterSize; size > 1; size >>= 1) {
			++m_prefilterLevels;
		}
	}

	OpenGLEnvironmentProbe::~OpenGLEnvironmentProbe()
	{
		glDeleteFramebuffers(1, &m_prefilterFramebuffer);
		glDeleteTextures(1, &m_prefilterTexture);
		glDeleteFramebuffers(1, &m_irradianceFramebuffer);
		glDeleteTextures(1, &m_irradianceTexture);
		glDeleteRenderbuffers(1, &m_captureDepth);
		glDeleteFramebuffers(1, &m_captureFramebuffer);
		glDeleteTextures(1, &m_captureTexture);
	}

	void OpenGLEnvironmentProbe::InitResources()
	{
		const auto setupCubeParameters = [](GLenum minFilter) {
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, minFilter);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		};

		// 捕获目标：RGBA16F cubemap + 深度 renderbuffer（HDR 环境，自发光可以超过 1）
		glGenTextures(1, &m_captureTexture);
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_captureTexture);
		for (uint32_t face = 0; face < 6; ++face) {
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGBA16F, m_captureSize, m_captureSize, 0, GL_RGBA, GL_FLOAT, nullptr);
		}
		setupCubeParameters(GL_LINEAR);

		glGenRenderbuffers(1, &m_captureDepth);
		glBindRenderbuffer(GL_RENDERBUFFER, m_captureDepth);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, m_captureSize, m_captureSize);

		glGenFramebuffers(1, &m_captureFramebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, m_captureFramebuffer);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_captureDepth);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X, m_captureTexture, 0);
		OI_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Probe capture framebuffer is incomplete!");

		// irradiance 目标
		glGenTextures(1, &m_irradianceTexture);
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_irradianceTexture);
		for (uint32_t face = 0; face < 6; ++face) {
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGBA16F, m_irradianceSize, m_irradianceSize, 0, GL_RGBA, GL_FLOAT, nullptr);
		}
		setupCubeParameters(GL_LINEAR);

		glGenFramebuffers(1, &m_irradianceFramebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, m_irradianceFramebuffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X, m_irradianceTexture, 0);
		OI_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Probe irradiance framebuffer is incomplete!");

		// prefilter 目标：mip 链（供 roughness 分级采样）
		glGenTextures(1, &m_prefilterTexture);
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_prefilterTexture);
		glTexStorage2D(GL_TEXTURE_CUBE_MAP, m_prefilterLevels, GL_RGBA16F, m_prefilterSize, m_prefilterSize);
		setupCubeParameters(GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(m_prefilterLevels - 1));

		glGenFramebuffers(1, &m_prefilterFramebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, m_prefilterFramebuffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X, m_prefilterTexture, 0);
		OI_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Probe prefilter framebuffer is incomplete!");

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);

		// 卷积用的立方体与着色器
		m_cubeMesh = VertexArray::Create();
		{
			float positions[sizeof(CUBE_POSITIONS) / sizeof(float)];
			for (size_t i = 0; i < sizeof(CUBE_POSITIONS) / sizeof(float); ++i) {
				positions[i] = CUBE_POSITIONS[i];
			}
			Ref<VertexBuffer> vertexBuffer = VertexBuffer::Create(positions, static_cast<uint32_t>(sizeof(positions)));
			vertexBuffer->SetLayout({ { ShaderDataType::Float3, "a_Position" } });
			m_cubeMesh->AddVertexBuffer(vertexBuffer);

			std::vector<uint32_t> indices(CUBE_INDICES, CUBE_INDICES + sizeof(CUBE_INDICES) / sizeof(uint32_t));
			m_cubeMesh->SetIndexBuffer(IndexBuffer::Create(indices.data(), static_cast<uint32_t>(indices.size())));
		}

		m_irradianceShader = Shader::Create("ProbeIrradiance", CUBE_VERTEX_SHADER, IRRADIANCE_FRAGMENT_SHADER);
		m_prefilterShader = Shader::Create("ProbePrefilter", CUBE_VERTEX_SHADER, PREFILTER_FRAGMENT_SHADER);

		OI_INFO("Environment probe created: capture {0}, irradiance {1}, prefilter {2} ({3} mips)",
			m_captureSize, m_irradianceSize, m_prefilterSize, m_prefilterLevels);
	}

	void OpenGLEnvironmentProbe::RenderFaces(const Ref<Shader>& shader, uint32_t texture, uint32_t size, uint32_t mipLevel, const glm::vec3& position)
	{
		const glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
		const glm::mat4 views[6] = {
			glm::lookAt(position, position + glm::vec3( 1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
			glm::lookAt(position, position + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
			glm::lookAt(position, position + glm::vec3( 0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
			glm::lookAt(position, position + glm::vec3( 0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
			glm::lookAt(position, position + glm::vec3( 0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
			glm::lookAt(position, position + glm::vec3( 0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
		};

		for (uint32_t face = 0; face < 6; ++face) {
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, texture, static_cast<GLint>(mipLevel));
			glViewport(0, 0, static_cast<GLsizei>(size), static_cast<GLsizei>(size));
			glClear(GL_COLOR_BUFFER_BIT);

			shader->SetMat4("u_FaceProjection", projection);
			shader->SetMat4("u_FaceView", views[face]);
			RenderCommand::DrawIndexed(m_cubeMesh);
		}
	}

	void OpenGLEnvironmentProbe::Capture(const glm::vec3& position, const CaptureCallback& drawScene, float nearClip, float farClip)
	{
		if (!m_irradianceShader) {
			InitResources();
		}

		// 1. 捕获：把场景画进 cubemap 的 6 个面（绘制内容由场景回调决定）
		{
			const glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, nearClip, farClip);
			const glm::mat4 views[6] = {
				glm::lookAt(position, position + glm::vec3( 1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
				glm::lookAt(position, position + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
				glm::lookAt(position, position + glm::vec3( 0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
				glm::lookAt(position, position + glm::vec3( 0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
				glm::lookAt(position, position + glm::vec3( 0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
				glm::lookAt(position, position + glm::vec3( 0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
			};

			RenderCommand::SetDepthTest(true);
			glBindFramebuffer(GL_FRAMEBUFFER, m_captureFramebuffer);
			for (uint32_t face = 0; face < 6; ++face) {
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, m_captureTexture, 0);
				glViewport(0, 0, static_cast<GLsizei>(m_captureSize), static_cast<GLsizei>(m_captureSize));
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				drawScene(views[face], projection);
			}
		}

		// 2. irradiance 卷积（漫反射环境光）
		glBindFramebuffer(GL_FRAMEBUFFER, m_irradianceFramebuffer);
		RenderCommand::SetDepthTest(false);
		RenderCommand::BindTextureCube(0, m_captureTexture);
		m_irradianceShader->Bind();
		m_irradianceShader->SetInt("u_Environment", 0);
		RenderFaces(m_irradianceShader, m_irradianceTexture, m_irradianceSize, 0, glm::vec3(0.0f));

		// 3. GGX prefilter（环境镜面反射，逐 mip 增加粗糙度）
		glBindFramebuffer(GL_FRAMEBUFFER, m_prefilterFramebuffer);
		RenderCommand::BindTextureCube(0, m_captureTexture);
		m_prefilterShader->Bind();
		m_prefilterShader->SetInt("u_Environment", 0);
		for (uint32_t mip = 0; mip < m_prefilterLevels; ++mip) {
			const float roughness = m_prefilterLevels > 1
				? static_cast<float>(mip) / static_cast<float>(m_prefilterLevels - 1) : 0.0f;
			m_prefilterShader->SetFloat("u_Roughness", roughness);
			m_prefilterShader->SetFloat("u_Resolution", static_cast<float>(m_prefilterSize >> mip));
			RenderFaces(m_prefilterShader, m_prefilterTexture, m_prefilterSize >> mip, mip, glm::vec3(0.0f));
		}

		// 4. 收尾：恢复默认帧缓冲、深度测试与窗口视口
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		RenderCommand::SetDepthTest(true);

		const Window& window = Application::Get().GetWindow();
		RenderCommand::SetViewport(0, 0, window.GetWidth(), window.GetHeight());
	}

	void OpenGLEnvironmentProbe::BindIrradiance(uint32_t slot) const
	{
		RenderCommand::BindTextureCube(slot, m_irradianceTexture);
	}

	void OpenGLEnvironmentProbe::BindPrefiltered(uint32_t slot) const
	{
		RenderCommand::BindTextureCube(slot, m_prefilterTexture);
	}
}
