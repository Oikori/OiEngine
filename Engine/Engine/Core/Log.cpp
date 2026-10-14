#include "Log.h"

#include <cstdio>

namespace OiEngine {
	namespace {
		Log::Level s_level = Log::Level::Info;

		const char* LevelName(Log::Level level)
		{
			switch (level) {
				case Log::Level::Trace:    return "TRACE";
				case Log::Level::Info:     return "INFO";
				case Log::Level::Warn:     return "WARN";
				case Log::Level::Error:    return "ERROR";
				case Log::Level::Critical: return "CRITICAL";
			}
			return "?";
		}

		void Write(Log::Level level, const std::string& message)
		{
			if (level < s_level) {
				return;
			}

			// 错误及以上写 stderr，其余写 stdout（便于控制台分流与日志重定向）
			std::FILE* stream = level >= Log::Level::Error ? stderr : stdout;
			std::fprintf(stream, "[%-8s] %s\n", LevelName(level), message.c_str());
			std::fflush(stream);
		}
	}

	void Log::SetLevel(Level level) { s_level = level; }
	Log::Level Log::GetLevel() { return s_level; }

	void Log::Trace(const std::string& message) { Write(Level::Trace, message); }
	void Log::Info(const std::string& message) { Write(Level::Info, message); }
	void Log::Warn(const std::string& message) { Write(Level::Warn, message); }
	void Log::Error(const std::string& message) { Write(Level::Error, message); }
	void Log::Critical(const std::string& message) { Write(Level::Critical, message); }
}
