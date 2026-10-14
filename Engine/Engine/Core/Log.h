#pragma once

#include <format>
#include <string>

#include "Engine/Core/Base.h"

namespace OiEngine {
	// 引擎日志：按级别过滤后写到控制台（Trace 默认关闭）。
	// 宏用 std::format 做格式化，支持 spdlog 风格的 {0} / {1} 位置参数与普通的 {} 占位。
	class OI_API Log {
	public:
		enum class Level {
			Trace = 0, Info, Warn, Error, Critical
		};

		static void SetLevel(Level level);
		static Level GetLevel();

		static void Trace(const std::string& message);
		static void Info(const std::string& message);
		static void Warn(const std::string& message);
		static void Error(const std::string& message);
		static void Critical(const std::string& message);
	};
}

// 客户端日志宏（引擎与 SandBox 通用）
#define OI_TRACE(...)     ::OiEngine::Log::Trace(std::format(__VA_ARGS__))
#define OI_INFO(...)      ::OiEngine::Log::Info(std::format(__VA_ARGS__))
#define OI_WARN(...)      ::OiEngine::Log::Warn(std::format(__VA_ARGS__))
#define OI_ERROR(...)     ::OiEngine::Log::Error(std::format(__VA_ARGS__))
#define OI_CRITICAL(...)  ::OiEngine::Log::Critical(std::format(__VA_ARGS__))
