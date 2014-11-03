/* vegas_error.c
 *
 * Error handling routine
 */
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include "vegas_error.h"

/* For now just put it all to stderr.
 * Maybe do something clever like a stack in the future?
 */
 
static char *time_str(char *buf)
{
    struct timeval tv;
    struct tm gmt;
    
    gettimeofday(&tv,NULL);
    gmtime_r(&tv.tv_sec, &gmt);
    
    sprintf(buf, "%d:%d:%d",gmt.tm_hour, gmt.tm_min, gmt.tm_sec);
    return buf;
}
void vegas_error(const char *name, const char *msg) {
    char tbuf[64];
    fprintf(stderr, "HPC[%s]: Error (%s): %s\n", time_str(tbuf), name, msg);
    fflush(stderr);
}

void vegas_warn(const char *name, const char *msg) {
    char tbuf[64];
    fprintf(stderr, "HPC[%s]: Warning (%s): %s\n", time_str(tbuf), name, msg);
    fflush(stderr);
}

/* These undefines are required to prevent an infinite loop */
#undef printf
#undef fprintf

void tprintf(const char *fmt, ...)
{
    va_list args;
    char buf[64];
    va_start(args, fmt);
    printf("HPC[%s]:", time_str(buf));
    vprintf(fmt, args);
    va_end(args);
}

void tfprintf(FILE *fout, const char *fmt, ...)
{
    va_list args;
    char buf[64];
    va_start(args, fmt);
    fprintf(fout, "HPC[%s]:", time_str(buf));
    vfprintf(fout, fmt, args);
    va_end(args);
}

