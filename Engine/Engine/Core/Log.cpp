#include "Log.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include "Base.h"

namespace OiEngine {
	namespace {
		LogLevel s_level = LogLevel::Info;
	    std::shared_ptr<spdlog::logger> console_logger;
	    std::shared_ptr<spdlog::logger> file_logger;

		spdlog::level::level_enum LevelCovert(LogLevel level)
		{
			switch (level) {
			    case LogLevel::Trace:    return spdlog::level::trace;
				case LogLevel::Info:     return spdlog::level::info;
				case LogLevel::Warn:     return spdlog::level::warn;
				case LogLevel::Error:    return spdlog::level::err;
				case LogLevel::Critical: return spdlog::level::critical;
			}
		    OI_ASSERT(false, "Unkown log level");
			return spdlog::level::off;
		}
	}

    namespace Logger {
	    void Init()
	    {
	        console_logger = spdlog::stdout_color_mt("console");

	        // Create a file rotating logger with 5 MB size max and 3 rotated files
	        auto max_size = 1048576 * 5;
	        auto max_files = 3;
	        file_logger = spdlog::rotating_logger_mt("file_logger", "logs/OiEngine.txt", max_size, max_files);
	        file_logger->set_pattern("[%H:%M:%S %z] [thread %t] [%^%l%$] [%s:%#] %v");
	    }

	    void SetLevel(LogLevel level)
	    {
	        spdlog::level::level_enum level_num = LevelCovert(level);
	        console_logger->set_level(level_num);
	        file_logger->set_level(level_num);
	    }

	    LogLevel GetLevel()
	    {
	        return s_level;
	    }

	    void Log(LogLevel level, const char *file, int line, const std::string& message)
	    {
	        spdlog::level::level_enum level_num = LevelCovert(level);
	        console_logger->log(spdlog::source_loc{file, line, ""}, level_num, message);
	        file_logger->log(spdlog::source_loc{file, line, ""}, level_num, message);
	    }
	}
}
