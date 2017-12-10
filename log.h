#include <stdio.h>
#include <stdarg.h>

void log_msg(const char *format, ...){
    va_list ap;
    va_start(ap, format);

    vfprintf(BB_DATA->logfile, format, ap);
}
