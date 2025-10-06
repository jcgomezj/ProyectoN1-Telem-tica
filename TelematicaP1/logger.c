#include "logger.h"
#include <time.h>
#include <pthread.h>

static FILE *gfp = NULL;
static pthread_mutex_t gmtx = PTHREAD_MUTEX_INITIALIZER;

void logger_init(const char *filepath) {
    pthread_mutex_lock(&gmtx);
    if (filepath && !gfp) gfp = fopen(filepath, "a");
    pthread_mutex_unlock(&gmtx);
}

void logger_close(void) {
    pthread_mutex_lock(&gmtx);
    if (gfp) { fclose(gfp); gfp = NULL; }
    pthread_mutex_unlock(&gmtx);
}

void log_line(const char *fmt, ...) {
    time_t now = time(NULL);
    struct tm tm_info;
#ifdef _WIN32
    localtime_s(&tm_info, &now);
#else
    localtime_r(&now, &tm_info);
#endif
    char tbuf[32];
    strftime(tbuf, sizeof tbuf, "%Y-%m-%d %H:%M:%S", &tm_info);

    pthread_mutex_lock(&gmtx);

    /* consola */
    printf("[%s] ", tbuf);
    va_list ap; va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");

    /* archivo */
    if (gfp) {
        fprintf(gfp, "[%s] ", tbuf);
        va_list ap2; va_start(ap2, fmt);
        vfprintf(gfp, fmt, ap2);
        va_end(ap2);
        fprintf(gfp, "\n");
        fflush(gfp);
    }

    pthread_mutex_unlock(&gmtx);
}
