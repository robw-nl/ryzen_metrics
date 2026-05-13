#ifndef ERROR_POLICY_H
#define ERROR_POLICY_H

/* Centralized Error Severity Policy  */
typedef enum {
    ERR_IGNORABLE,   /* Minor issues, logged only in debug, loop continues natively */
    ERR_RECOVERABLE, /* Subsystem degraded, logged, using safe fallbacks */
    ERR_FATAL        /* Critical failure, logged, immediate safe exit */
} ErrorSeverity;

void log_error(ErrorSeverity severity, const char *tag, const char *msg);

#endif
