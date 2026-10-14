#include "OpenGLStorageBuffer.h"

#include <glad/glad.h>

namespace OiEngine {
	OpenGLStorageBuffer::OpenGLStorageBuffer(uint32_t size, uint32_t binding)
	{
		glCreateBuffers(1, &m_RendererID);
		glNamedBufferData(m_RendererID, size, nullptr, GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, m_RendererID);
	}

	OpenGLStorageBuffer::~OpenGLStorageBuffer()
	{
		glDeleteBuffers(1, &m_RendererID);
	}

	void OpenGLStorageBuffer::SetData(const void* data, uint32_t size, uint32_t offset)
	{
		glNamedBufferSubData(m_RendererID, offset, size, data);
	}

	void OpenGLStorageBuffer::GetData(void* data, uint32_t size, uint32_t offset) const
	{
		glGetNamedBufferSubData(m_RendererID, offset, size, data);
	}

	void OpenGLStorageBuffer::Clear(uint32_t offset, uint32_t size, uint32_t value)
	{
		glClearNamedBufferSubData(m_RendererID, GL_R32UI, offset, size, GL_RED_INTEGER, GL_UNSIGNED_INT, &value);
	}
}
