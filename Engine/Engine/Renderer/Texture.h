#pragma once

#include <string>

#include "Engine/Core/Base.h"

namespace OiEngine {
	enum class ImageFormat {
		None = 0,
		R8,
		RGB8,
		RGBA8,
		RGBA32F
	};

	struct TextureSpecification {
		uint32_t width = 1;
		uint32_t height = 1;
		ImageFormat format = ImageFormat::RGBA8;
		bool generateMips = true;
	};

	class Texture {
	public:
		virtual ~Texture() = default;

		virtual const TextureSpecification& GetSpecification() const = 0;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;
		virtual uint32_t GetRendererID() const = 0;

		virtual const std::string& GetPath() const = 0;

		virtual void SetData(void* data, uint32_t size) = 0;

		virtual void Bind(uint32_t slot = 0) const = 0;

		virtual bool IsLoaded() const = 0;

		virtual bool operator==(const Texture& other) const = 0;
	};

	class Texture2D : public Texture {
	public:
		static Ref<Texture2D> Create(const TextureSpecification& specification);
		static Ref<Texture2D> Create(const std::string& path);
	};

}
