#pragma once

#include "Engine/Core/Base.h"

namespace OiEngine {
	// GPU 存储缓冲（Shader Storage Buffer）：容量远大于 UBO，shader 里可读可写（含原子操作），
	// 用于光源列表、cluster 光源剔除结果等大数据量场合。
	// shader 侧声明为 layout(std430, binding = N) buffer {...}，N 与 Create 的 binding 一致。
	class StorageBuffer {
	public:
		virtual ~StorageBuffer() = default;

		virtual void SetData(const void* data, uint32_t size, uint32_t offset = 0) = 0;
		// 读回数据（GPU -> CPU，会同步等待；调试统计等低频调用）
		virtual void GetData(void* data, uint32_t size, uint32_t offset = 0) const = 0;
		// 以 32 位整数清空一段区间（cluster 计数清零等）；只用于整数内容的缓冲
		virtual void Clear(uint32_t offset, uint32_t size, uint32_t value = 0) = 0;

		static Ref<StorageBuffer> Create(uint32_t size, uint32_t binding);
	};
}
