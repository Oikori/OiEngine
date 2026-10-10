#include "OpenGLShader.h"

#include <fstream>

#include <glad/glad.h>

namespace OiEngine {
	namespace {
		GLenum ShaderTypeFromString(const std::string& type)
		{
			if (type == "vertex")
				return GL_VERTEX_SHADER;
			if (type == "fragment" || type == "pixel")
				return GL_FRAGMENT_SHADER;

			OI_ASSERT(false, "Unknown shader type!");
			return 0;
		}

	    std::string ReadFile(const std::string& filepath)
		{
		    std::string result;
		    std::ifstream in(filepath, std::ios::in | std::ios::binary); // ifstream closes itself due to RAII
		    if (in) {
		        in.seekg(0, std::ios::end);
		        size_t size = in.tellg();
		        if (size != -1) {
		            result.resize(size);
		            in.seekg(0, std::ios::beg);
		            in.read(&result[0], size);
		        } else {
		            OI_ERROR("Could not read from file '{0}'", filepath);
		        }
		    } else {
		        OI_ERROR("Could not open file '{0}'", filepath);
		    }

		    return result;
		}

	    std::unordered_map<GLenum, std::string> PreProcess(const std::string& source)
		{
		    std::unordered_map<GLenum, std::string> shaderSources;

		    const char* typeToken = "#type";
		    size_t typeTokenLength = strlen(typeToken);
		    size_t pos = source.find(typeToken, 0); //Start of shader type declaration line
		    while (pos != std::string::npos) {
		        size_t eol = source.find_first_of("\r\n", pos); //End of shader type declaration line
		        OI_ASSERT(eol != std::string::npos, "Syntax error");
		        size_t begin = pos + typeTokenLength + 1; //Start of shader type name (after "#type " keyword)
		        std::string type = source.substr(begin, eol - begin);
		        OI_ASSERT(Utils::ShaderTypeFromString(type), "Invalid shader type specified");

		        size_t nextLinePos = source.find_first_not_of("\r\n", eol); //Start of shader code after shader type declaration line
		        OI_ASSERT(nextLinePos != std::string::npos, "Syntax error");
		        pos = source.find(typeToken, nextLinePos); //Start of next shader type declaration line

		        shaderSources[ShaderTypeFromString(type)] = (pos == std::string::npos) ? source.substr(nextLinePos) : source.substr(nextLinePos, pos - nextLinePos);
		    }

		    return shaderSources;
		}

	    unsigned int CompileShader(unsigned int type, const std::string &source)
		{
		    int success;
		    unsigned int shader = glCreateShader(type);
		    const char* pSource = source.c_str();
		    glShaderSource(shader, 1, &pSource, NULL);
		    glCompileShader(shader);
		    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
		    if (success == GL_FALSE) {
		        int logLength = 0;
		        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
		        std::vector<char> infoLog(logLength);
		        glGetShaderInfoLog(shader, logLength, &logLength, &infoLog[0]);
		        glDeleteShader(shader);

		        OI_ERROR("{0}", infoLog.data());
		        OI_ASSERT(false, "Shader compilation failed!");
		        return 0;
		    }
		    return shader;
		}

	    unsigned int LinkShader(const std::vector<unsigned int> &shaders)
		{
		    int success;
		    unsigned int program = glCreateProgram();
		    for (const auto &shader : shaders) {
		        glAttachShader(program, shader);
		    }
		    glLinkProgram(program);
		    glGetProgramiv(program, GL_LINK_STATUS, &success);
		    if (success == GL_FALSE) {
		        int logLength = 0;
		        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
		        std::vector<char> infoLog(logLength);
		        glGetProgramInfoLog(program, logLength, &logLength, &infoLog[0]);
		        glDeleteProgram(program);

		        OI_ERROR("{0}", infoLog.data());
		        OI_ASSERT(false && "Shader link failed!");
		        return 0;
		    }
		    return program;
		}
	}

	OpenGLShader::OpenGLShader(const std::string& filepath): m_filePath(filepath)
	{
		std::string source = ReadFile(filepath);
		auto shaderSources = PreProcess(source);

	    std::vector<unsigned int> shaders;
	    bool err = false;
	    for (const auto &kv : shaderSources) {
	        unsigned int shader = CompileShader(kv.first, kv.second);
	        if (shader == 0) {
	            err = true;
	            break;
	        }
	        shaders.push_back(shader);
	    }
	    if (err) {
	        for (auto &shader : shaders) {
	            glDeleteShader(shader);
	        }
	        return;
	    }

	    m_rendererID = LinkShader(shaders);
	    for (auto &shader : shaders) {
	        glDeleteShader(shader);
	    }

	    if (m_rendererID != 0) {
		    // Extract name from filepath
	        auto lastSlash = filepath.find_last_of("/\\");
	        lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
	        auto lastDot = filepath.rfind('.');
	        auto count = lastDot == std::string::npos ? filepath.size() - lastSlash : lastDot - lastSlash;
	        m_name = filepath.substr(lastSlash, count);
	    }
	}

	OpenGLShader::OpenGLShader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc)
        : m_name(name)
	{
	    unsigned int vertexShader = CompileShader(GL_VERTEX_SHADER, vertexSrc);
	    if (vertexShader == 0) {
	        return;
	    }
	    unsigned int fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSrc);
	    if (fragmentShader == 0) {
	        glDeleteShader(vertexShader);
	        return;
	    }

	    std::vector<unsigned int> shaders{vertexShader, fragmentShader};
	    m_rendererID = LinkShader(shaders);
	    glDeleteShader(vertexShader);
	    glDeleteShader(fragmentShader);
	}

	OpenGLShader::~OpenGLShader()
	{
		glDeleteProgram(m_rendererID);
	}

	void OpenGLShader::Bind() const
	{
		glUseProgram(m_rendererID);
	}

	void OpenGLShader::Unbind() const
	{
		glUseProgram(0);
	}

	void OpenGLShader::SetInt(const std::string& name, int value)
	{
        int location = GetLocationFromCache(name);
	    glUniform1i(location, value);
	}

	void OpenGLShader::SetIntArray(const std::string& name, int* values, uint32_t count)
	{
        int location = GetLocationFromCache(name);
	    glUniform1iv(location, count, values);
	}

	void OpenGLShader::SetFloat(const std::string& name, float value)
	{
        int location = GetLocationFromCache(name);
	    glUniform1f(location, value);
	}

	void OpenGLShader::SetFloat2(const std::string& name, const Math::Vector2& value)
	{
        int location = GetLocationFromCache(name);
	    glUniform2f(location, value.x, value.y);
	}

	void OpenGLShader::SetFloat3(const std::string& name, const Math::Vector3& value)
	{
        int location = GetLocationFromCache(name);
	    glUniform3f(location, value.x, value.y, value.z);
	}

	void OpenGLShader::SetFloat4(const std::string& name, const Math::Vector4& value)
	{
        int location = GetLocationFromCache(name);
	    glUniform4f(location, value.x, value.y, value.z, value.w);
	}

    void OpenGLShader::SetMat3(const std::string& name, const Math::Matrix3& value)
	{
        int location = GetLocationFromCache(name);
	    glUniformMatrix3fv(location, 1, GL_FALSE, value.Data());
	}

	void OpenGLShader::SetMat4(const std::string& name, const Math::Matrix4& value)
	{
        int location = GetLocationFromCache(name);
	    glUniformMatrix4fv(location, 1, GL_FALSE, value.Data());
	}

    int OpenGLShader::GetLocationFromCache(const std::string &name)
	{
	    auto iter = m_locationCache.find(name);
	    if (iter != m_locationCache.end()) {
	        return iter->second;
	    }

	    int location = glGetUniformLocation(m_rendererID, name.c_str());
	    m_locationCache.insert({name, location});
	    return location;
	}
}
