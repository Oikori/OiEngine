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

		m_irradianceShader = Shader::Create("assets/shader/engine/probe_irradiance.glsl");
		m_prefilterShader = Shader::Create("assets/shader/engine/probe_prefilter.glsl");

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
