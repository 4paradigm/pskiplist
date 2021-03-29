#ifndef LOG4P_H
#define LOG4P_H

#include <iostream>
#include <limits>
#include <string>
#include <stdio.h>
#include <stdarg.h>

enum _LOG4P_LEVEL
{
	LOG4P_LEVEL_ERROR,
	LOG4P_LEVEL_WARNING,
	LOG4P_LEVEL_INFO,
	LOG4P_LEVEL_DEBUG
};

#ifdef DEBUG_LOG4P
static _LOG4P_LEVEL LOG4P_LEVEL = LOG4P_LEVEL_DEBUG;
#else
static _LOG4P_LEVEL LOG4P_LEVEL = LOG4P_LEVEL_INFO;
#endif

const static char* _LOG4P_LEVEL_PREFIX[] =
{
	"\033[1;31mE: ",
	"\033[1;35mW: ",
	"",
	"\033[1;33mD: "
};

static void _LOG4P(_LOG4P_LEVEL logLevel, char* file, char* func, unsigned int line, const char* format, ...)
{
	if (logLevel > LOG4P_LEVEL)
		return;

	char message[512];
	va_list args;
	va_start(args, format);
	vsnprintf(message, 512, format, args);
	va_end(args);

	if (logLevel == LOG4P_LEVEL_ERROR)
	{
		std::cerr << file << ":" << line << " " << func << std::endl;
		std::cerr << _LOG4P_LEVEL_PREFIX[logLevel] << message << "\033[0m" << std::endl;
	}
	else
	{
		std::cout << _LOG4P_LEVEL_PREFIX[logLevel] << message << "\033[0m" << std::endl;
	}
}

#define LOG4P(logLevel, format, ...) \
	_LOG4P(logLevel, __BASE_FILE__, __func__, __LINE__, format, ##__VA_ARGS__)

#define LOG4P_ERROR(format, ...) \
	LOG4P(LOG4P_LEVEL_ERROR, format, ##__VA_ARGS__)

#define LOG4P_WARNING(format, ...) \
	LOG4P(LOG4P_LEVEL_WARNING, format, ##__VA_ARGS__)

#define LOG4P_INFO(format, ...) \
	LOG4P(LOG4P_LEVEL_INFO, format, ##__VA_ARGS__)

#define LOG4P_DEBUG(format, ...) \
	LOG4P(LOG4P_LEVEL_DEBUG, format, ##__VA_ARGS__)

#endif // LOG4P_H