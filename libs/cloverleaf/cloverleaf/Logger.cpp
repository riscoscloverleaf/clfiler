//
// Created by lenz on 3/23/20.
//

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include "oslib/os.h"
#include <cloverleaf/Logger.h>

#define DEBUG_MAX_LINE_LENGTH 4096

char Logger::log_file[255];
static char line[DEBUG_MAX_LINE_LENGTH];
LogLevel Logger::log_level = LOG_DEBUG;

void Logger::init(const char* _log_file, LogLevel level)
{
    strcpy(log_file, _log_file);
    log_level = level;
}

void Logger::log(const char* level, const char* cntrl_string, va_list ap)
{
    vsnprintf(line, DEBUG_MAX_LINE_LENGTH - 1, cntrl_string, ap);
    line[DEBUG_MAX_LINE_LENGTH - 1] = '\0';
    FILE *f = fopen(log_file,"a");
    fprintf(f, "%9d %-9s %s\n", os_read_monotonic_time(), level, line);
    fclose(f);
}

void Logger::debug(const char* cntrl_string, ...) {
    va_list         ap;
    va_start(ap, cntrl_string);
    log("DEBUG", cntrl_string, ap);
}

void Logger::info(const char* cntrl_string, ...) {
    va_list         ap;
    va_start(ap, cntrl_string);
    log("INFO ", cntrl_string, ap);
}

void Logger::warn(const char* cntrl_string, ...) {
    va_list         ap;
    va_start(ap, cntrl_string);
    log("WARN ", cntrl_string, ap);
}

void Logger::error(const char* cntrl_string, ...) {
    va_list         ap;
    va_start(ap, cntrl_string);
    log("ERROR", cntrl_string, ap);
}

void Logger::crit(const char* cntrl_string, ...) {
    va_list         ap;
    va_start(ap, cntrl_string);
    log("CRIT ", cntrl_string, ap);
}