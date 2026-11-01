#include "Log.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include "Base.h"

namespace OiEngine {
    namespace {
        LogLevel s_level = LogLevel::Trace;
        bool s_initialized = false;
        std::shared_ptr<spdlog::logger> g_logger;

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

            g_logger = spdlog::get("OiEngine");
            if (!g_logger) {
                std::vector<spdlog::sink_ptr> logSinks;
                logSinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
                try {
                    // 单文件 5 MB、保留 3 份轮转文件；目录由 spdlog 自动创建
                    logSinks.emplace_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/OiEngine.txt", 1048576 * 5, 3));
                    logSinks[1]->set_pattern("[%H:%M:%S %z] [thread %t] [%^%l%$] [%s:%#] %v");
                } catch (const spdlog::spdlog_ex&) {
                    // 工作目录不可写时退化为仅控制台输出，不阻断引擎启动
                }
                g_logger = std::make_shared<spdlog::logger>("OiEngine", begin(logSinks), end(logSinks));
                spdlog::register_logger(g_logger);
                g_logger->set_level(spdlog::level::trace);
                // 逐条 flush（trace 及以上）：进程被强杀时已写日志也不会丢
                g_logger->flush_on(spdlog::level::trace);
            }

            s_initialized = true;
            SetLevel(s_level);
        }

        void SetLevel(LogLevel level)
        {
            // 记住级别：Init 之前调用也允许，初始化时再应用到 OiEngine logger
            s_level = level;
            const spdlog::level::level_enum level_num = ToSpdlogLevel(level);
            if (g_logger) {
                g_logger->set_level(level_num);
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
            if (g_logger) {
                g_logger->log(location, level_num, message);
            }
        }
    }
}
