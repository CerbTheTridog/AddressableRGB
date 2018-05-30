/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See `log.c` for details.
 * 
 * From https://github.com/rxi/log.c
 */

#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdarg.h>

#define LOG_VERSION "0.1.0"

typedef void (*log_LockFn)(void *udata, int lock);

/* LOG_MATRIX_TRACE - Tracing something light-pattern related. The verbosity at this level is insane
 * LOG_TRACE        - Basically "I am in this function" logging
 * LOG_DEBUG        - Useful debugging messages
 * LOG_INFO         - Informative logs for a user
 * LOG_WARN         - Basic warnings. Nothing really bad, just a "Hey this happened incorrectly"
 * LOG_ERROR        - Something went wrong, but we can adapt
 * LOG_FATAL        - The program is ending because something went so very wrong
 */

enum { LOG_MATRIX_TRACE, LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL };

#define log_matrix_trace(...) log_log(LOG_MATRIX_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define log_trace(...) log_log(LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define log_debug(...) log_log(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define log_info(...)  log_log(LOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...)  log_log(LOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) log_log(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define log_fatal(...) log_log(LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)

void log_set_udata(void *udata);
void log_set_lock(log_LockFn fn);
void log_set_fp(FILE *fp);
void log_set_level(int level);
void log_set_quiet(int enable);

void log_log(int level, const char *file, int line, const char *fmt, ...);

#endif
