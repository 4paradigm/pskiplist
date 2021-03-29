// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, 4Paradigm Inc. */

#ifndef LOG4P_H
#define LOG4P_H

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#include <iostream>
#include <limits>
#include <string>
#include <stdio.h>
#include <stdarg.h>

#define LOG4P_LEVEL_ERROR 	0
#define LOG4P_LEVEL_WARNING	1
#define LOG4P_LEVEL_INFO	2
#define LOG4P_LEVEL_DEBUG	3

#define LOG4P_LEVEL LOG4P_LEVEL_INFO

const static char* _LOG4P_LEVEL_PREFIX[] =
{
	" \033[1;31mE: ",
	" \033[1;35mW: ",
	" ",
	" \033[1;33mD: "
};

static void _LOG4P(int logLevel, const char* file, const char* func, unsigned int line, const char* format, ...)
{
	// if (logLevel > LOG4P_LEVEL)
	// 	return;

	char message[512];
	va_list args;
	va_start(args, format);
	vsnprintf(message, 512, format, args);
	va_end(args);

	std::cout << file << ":" << line << " \033[1;36m" << func << "\033[0m" << _LOG4P_LEVEL_PREFIX[logLevel] << message << "\033[0m" << std::endl;
}

#define LOG4P(logLevel, format, ...) \
		{_LOG4P(logLevel, __FILENAME__, __func__, __LINE__, format, ##__VA_ARGS__);}

#if (LOG4P_LEVEL>=LOG4P_LEVEL_ERROR)
	#define LOG4P_ERROR(format, ...) \
		LOG4P(LOG4P_LEVEL_ERROR, format, ##__VA_ARGS__)
#else
	#define LOG4P_ERROR(format, ...) {}
#endif

#if (LOG4P_LEVEL>=LOG4P_LEVEL_WARNING)
	#define LOG4P_WARNING(format, ...) \
		LOG4P(LOG4P_LEVEL_WARNING, format, ##__VA_ARGS__)
#else
	#define LOG4P_WARNING(format, ...) {}
#endif

#if (LOG4P_LEVEL>=LOG4P_LEVEL_INFO)
	#define LOG4P_INFO(format, ...) \
		LOG4P(LOG4P_LEVEL_INFO, format, ##__VA_ARGS__)
#else
	#define LOG4P_INFO(format, ...) {}
#endif

#if (LOG4P_LEVEL>=LOG4P_LEVEL_DEBUG)
	#define LOG4P_DEBUG(format, ...) \
		LOG4P(LOG4P_LEVEL_DEBUG, format, ##__VA_ARGS__)
#else
	#define LOG4P_DEBUG(format, ...) {}
#endif

#endif // LOG4P_H