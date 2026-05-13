#include "error_policy.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void log_error(ErrorSeverity severity, const char *tag, const char *msg) {
    time_t now;
    time(&now);
    struct tm *local = localtime(&now);

    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", local);

    switch (severity) {
        case ERR_IGNORABLE:
            #ifndef __OPTIMIZE__
            fprintf(stderr, "[%s] [IGNORABLE] %s: %s\n", time_str, tag, msg);
            #endif
            break;
        case ERR_RECOVERABLE:
            fprintf(stderr, "[%s] [RECOVERABLE] %s: %s\n", time_str, tag, msg);
            break;
        case ERR_FATAL:
            fprintf(stderr, "[%s] [FATAL] %s: %s. Exiting daemon safely.\n", time_str, tag, msg);
            exit(EXIT_FAILURE);
    }
}
