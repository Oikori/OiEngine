#pragma once

#include "Engine/Renderer/Shader.h"

namespace OiEngine {
	class OpenGLShader : public Shader {
	public:
		OpenGLShader(const std::string& filepath);
		OpenGLShader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc);
		~OpenGLShader() override;

		void Bind() const override;
		void Unbind() const override;

		void SetInt(const std::string& name, int value) override;
		void SetIntArray(const std::string& name, int* values, uint32_t count) override;
		void SetFloat(const std::string& name, float value) override;
		void SetFloat2(const std::string& name, const Math::Vector2& value) override;
		void SetFloat3(const std::string& name, const Math::Vector3& value) override;
		void SetFloat4(const std::string& name, const Math::Vector4& value) override;
		void SetMat3(const std::string& name, const Math::Matrix3& value) override;
		void SetMat4(const std::string& name, const Math::Matrix4& value) override;

		const std::string& GetName() const override { return m_name; }

	private:
	    int GetLocationFromCache(const std::string &name);

	private:
		uint32_t m_rendererID;
		std::string m_filePath;
		std::string m_name;

	    std::unordered_map<std::string, int> m_locationCache{};
	};

}
