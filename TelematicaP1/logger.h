#ifndef LOGGER_H
#define LOGGER_H
#include <stdio.h>
#include <stdarg.h>

void logger_init(const char *filepath);
void logger_close(void);
void log_line(const char *fmt, ...);

#endif
