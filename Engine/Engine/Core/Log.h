#pragma once

#include <format>
#include <string>

#include "export.h"

namespace OiEngine {
    enum class LogLevel {
        Trace = 0, Info, Warn, Error, Critical
    };

	// 引擎日志：按级别过滤后写到控制台（Trace 默认关闭）。
	// 宏用 std::format 做格式化，支持 spdlog 风格的 {0} / {1} 位置参数与普通的 {} 占位。
    namespace Logger {
        OI_ENGINE_API void Init();
        OI_ENGINE_API void SetLevel(LogLevel level);
        OI_ENGINE_API LogLevel GetLevel();
        OI_ENGINE_API void Log(LogLevel level, const char *file, int line, const std::string& message);
    }
}

// 客户端日志宏（引擎与 SandBox 通用）
#define OI_TRACE(...)     ::OiEngine::Logger::Log(LogLevel::Trace, __FILE__, __LINE__, std::format(__VA_ARGS__))
#define OI_INFO(...)      ::OiEngine::Logger::Log(LogLevel::Info, __FILE__, __LINE__, std::format(__VA_ARGS__))
#define OI_WARN(...)      ::OiEngine::Logger::Log(LogLevel::Warn, __FILE__, __LINE__, std::format(__VA_ARGS__))
#define OI_ERROR(...)     ::OiEngine::Logger::Log(LogLevel::Error, __FILE__, __LINE__, std::format(__VA_ARGS__))
#define OI_CRITICAL(...)  ::OiEngine::Logger::Log(LogLevel::Critical, __FILE__, __LINE__, std::format(__VA_ARGS__))
