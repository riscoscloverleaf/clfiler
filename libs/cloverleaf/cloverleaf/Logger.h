//
// Created by lenz on 3/23/20.
//

#ifndef CLOVERLEAF_LOGGER_H
#define CLOVERLEAF_LOGGER_H

#include <stdarg.h>
enum LogLevel {
    LOG_NONE,
    LOG_CRIT,
    LOG_ERROR,
    LOG_WARN,
    LOG_INFO,
    LOG_DEBUG
};

class Logger {
protected:
    static char log_file[255];
    static void log(const char* level, const char* cntrl_string, va_list ap);
public:
    static LogLevel log_level;

    static void init(const char* log_file, LogLevel level);
    static void debug(const char* cntrl_string, ...);
    static void info(const char* cntrl_string, ...);
    static void warn(const char* cntrl_string, ...);
    static void error(const char* cntrl_string, ...);
    static void crit(const char* cntrl_string, ...);
};

#define Log_debug(fmt, ...) do { if (Logger::log_level >= LogLevel::LOG_DEBUG) Logger::debug(fmt, __VA_ARGS__); } while (0)
#define Log_info(fmt, ...) do { if (Logger::log_level >= LogLevel::LOG_INFO) Logger::info(fmt, __VA_ARGS__); } while (0)
#define Log_error(fmt, ...) do { if (Logger::log_level >= LogLevel::LOG_ERROR) Logger::error(fmt, __VA_ARGS__); } while (0)

#endif //CLOVERLEAF_LOGGER_H
