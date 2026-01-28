/*
 * logger.c
 * Purpose:
 *   Implementation of the thread-safe CSV logger.
 *
 * Key responsibilities:
 *   - Emit a short metadata header (as '#' comment lines).
 *   - Emit a stable CSV header row and subsequent event rows.
 *   - Ensure concurrent logging from multiple threads does not interleave.
 *
 * Important invariants/assumptions:
 *   - This module does not interpret event strings; it only records them.
 *   - Time values are relative milliseconds from the run start (as provided by callers).
 */
#include "logger.h"

#include <stdarg.h>
#include <string.h>

#ifndef BUILD_TYPE
#define BUILD_TYPE "unknown"
#endif

int logger_init(logger_t *lg, const char *path, const run_params_t *params) {
    if (!lg) return -1;

    memset(lg, 0, sizeof(*lg));
    lg->enabled = 1;

    const char *use_path = (path && path[0]) ? path : "run_log.csv";
    lg->fp = fopen(use_path, "w");
    if (!lg->fp) return -2;

    if (pthread_mutex_init(&lg->mtx, NULL) != 0) {
        fclose(lg->fp);
        lg->fp = NULL;
        return -3;
    }

    // Header (comment lines), helpful for marking + human sanity.
    if (params) {
        fprintf(lg->fp, "# producers=%d consumers=%d q=%d timeout=%d verbose=%d\n",
                params->producers, params->consumers, params->q_capacity, params->timeout_sec,
                params->verbose ? 1 : 0);
    }

    fprintf(lg->fp, "# build_type=%s\n", BUILD_TYPE);
    fprintf(lg->fp, "# build_datetime=%s %s\n", __DATE__, __TIME__);
#ifdef __VERSION__
    fprintf(lg->fp, "# compiler=%s\n", __VERSION__);
#endif

    // Stable CSV schema for analysis.
    fprintf(lg->fp, "time_ms,event,actor_type,actor_id,value,priority,producer_id,q_count,blocked_ms\n");
    fflush(lg->fp);

    return 0;
}

void logger_close(logger_t *lg) {
    if (!lg) return;
    if (lg->fp) {
        fflush(lg->fp);
        fclose(lg->fp);
        lg->fp = NULL;
    }
    pthread_mutex_destroy(&lg->mtx);
    lg->enabled = 0;
}

void logger_log(logger_t *lg,
                uint64_t time_ms,
                const char *event,
                char actor_type,
                int actor_id,
                const msg_t *m,
                int q_count,
                int blocked_ms) {
    if (!lg || !lg->enabled || !lg->fp) return;

    pthread_mutex_lock(&lg->mtx);

    int value = -1, prio = -1, prod = -1;
    if (m) {
        value = m->value;
        prio  = m->priority;
        prod  = m->producer_id;
    }

    fprintf(lg->fp, "%llu,%s,%c,%d,%d,%d,%d,%d,%d\n",
            (unsigned long long)time_ms,
            event ? event : "EV",
            actor_type,
            actor_id,
            value, prio, prod,
            q_count,
            blocked_ms);

    // Flush each row to keep logs usable even if the process terminates early.
    fflush(lg->fp);
    pthread_mutex_unlock(&lg->mtx);
}

void logger_comment(logger_t *lg, const char *fmt, ...) {
    if (!lg || !lg->enabled || !lg->fp || !fmt) return;

    pthread_mutex_lock(&lg->mtx);

    fputs("# ", lg->fp);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(lg->fp, fmt, ap);
    va_end(ap);
    fputc('\n', lg->fp);

    fflush(lg->fp);
    pthread_mutex_unlock(&lg->mtx);
}
