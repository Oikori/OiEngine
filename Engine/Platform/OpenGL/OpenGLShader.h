#pragma once

#include "Engine/Renderer/Shader.h"

namespace OiEngine {
	class OpenGLShader : public Shader {
	public:
		OpenGLShader(const std::string& filepath);
		OpenGLShader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc);
		// 计算着色器（computeSrc 为 GLSL compute 源码，local_size_* 在工作组布局限定符里声明）
		OpenGLShader(const std::string& name, const std::string& computeSrc);
		~OpenGLShader() override;

		void Bind() const override;
		void Unbind() const override;

		void SetInt(const std::string& name, int value) override;
		void SetIntArray(const std::string& name, int* values, uint32_t count) override;
		void SetFloat(const std::string& name, float value) override;
		void SetFloat2(const std::string& name, const glm::vec2& value) override;
		void SetFloat3(const std::string& name, const glm::vec3& value) override;
		void SetFloat4(const std::string& name, const glm::vec4& value) override;
		void SetMat3(const std::string& name, const glm::mat3& value) override;
		void SetMat4(const std::string& name, const glm::mat4& value) override;

		void Dispatch(uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1) const override;

		const std::string& GetName() const override { return m_name; }

	private:
	    int GetLocationFromCache(const std::string &name);

	private:
		uint32_t m_rendererID;
		std::string m_filePath;
		std::string m_name;
		bool m_isCompute = false;  // 计算着色器才能 Dispatch

	    std::unordered_map<std::string, int> m_locationCache{};
	};

}
