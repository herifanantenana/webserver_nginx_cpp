#pragma once

#include <string>
namespace utils
{
	class Logger
	{
	public:
		enum Level
		{
			CONSOL,
			DEBUG,
			INFO,
			WARNING,
			ERROR,
			FATAL
		};

	private:
		Logger();
		~Logger();

		static const char *getLevelIcon(Level level);
		static const char *getLevelColor(Level level);
		static const char *getLevelString(Level level);

		static std::string getCurrentTimestamp();

	public:
		static void log(Level level, const char *file, const char *function, int line, const char *format, ...);
	};
} // namespace utils

#define LOG_CONSOLE(...) utils::Logger::log(utils::Logger::CONSOL, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(...) utils::Logger::log(utils::Logger::DEBUG, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...) utils::Logger::log(utils::Logger::INFO, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define LOG_WARNING(...) utils::Logger::log(utils::Logger::WARNING, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) utils::Logger::log(utils::Logger::ERROR, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define LOG_FATAL(...) utils::Logger::log(utils::Logger::FATAL, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)