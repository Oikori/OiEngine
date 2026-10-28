#include "Log.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include "Base.h"

namespace OiEngine {
	namespace {
		LogLevel s_level = LogLevel::Info;
		bool s_initialized = false;
		std::shared_ptr<spdlog::logger> console_logger;
		std::shared_ptr<spdlog::logger> file_logger;

		spdlog::level::level_enum ToSpdlogLevel(LogLevel level)
		{
			switch (level) {
				case LogLevel::Trace:    return spdlog::level::trace;
				case LogLevel::Info:     return spdlog::level::info;
				case LogLevel::Warn:     return spdlog::level::warn;
				case LogLevel::Error:    return spdlog::level::err;
				case LogLevel::Critical: return spdlog::level::critical;
			}
			OI_ASSERT(false, "Unknown log level");
			return spdlog::level::off;
		}
	}

	namespace Logger {
		void Init()
		{
			// 幂等：Application 支持 Shutdown 后重新 Init，同名 logger 已注册时复用即可
			if (s_initialized) {
				SetLevel(s_level);
				return;
			}

			console_logger = spdlog::get("console");
			if (!console_logger) {
				console_logger = spdlog::stdout_color_mt("console");
			}

			file_logger = spdlog::get("file_logger");
			if (!file_logger) {
				try {
					// 单文件 5 MB、保留 3 份轮转文件；目录由 spdlog 自动创建
					file_logger = spdlog::rotating_logger_mt("file_logger", "logs/OiEngine.txt", 1048576 * 5, 3);
				} catch (const spdlog::spdlog_ex&) {
					// 工作目录不可写时退化为仅控制台输出，不阻断引擎启动
					file_logger.reset();
				}
			}
			if (file_logger) {
				file_logger->set_pattern("[%H:%M:%S %z] [thread %t] [%^%l%$] [%s:%#] %v");
				// 逐条 flush（info 及以上）：进程被强杀时已写日志也不会丢
				file_logger->flush_on(spdlog::level::info);
			}

			s_initialized = true;
			SetLevel(s_level);
		}

		void SetLevel(LogLevel level)
		{
			// 记住级别：Init 之前调用也允许，初始化时再应用到两个 logger
			s_level = level;
			const spdlog::level::level_enum level_num = ToSpdlogLevel(level);
			if (console_logger) {
				console_logger->set_level(level_num);
			}
			if (file_logger) {
				file_logger->set_level(level_num);
			}
		}

		LogLevel GetLevel()
		{
			return s_level;
		}

		void Log(LogLevel level, const char* file, int line, const std::string& message)
		{
			if (!s_initialized) {
				Init();  // 兜底：早于 Application::Init 的日志（如全局对象构造）也不会空指针
			}

			const spdlog::level::level_enum level_num = ToSpdlogLevel(level);
			const spdlog::source_loc location{file, line, ""};
			if (console_logger) {
				console_logger->log(location, level_num, message);
			}
			if (file_logger) {
				file_logger->log(location, level_num, message);
			}
		}
	}
}
