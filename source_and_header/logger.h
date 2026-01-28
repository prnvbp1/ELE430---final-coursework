/*
 * logger.h
 * Purpose:
 *   Thread-safe CSV logging for producer/consumer events and run metadata.
 *
 * Key responsibilities:
 *   - Create and manage the CSV log file (default: run_log.csv).
 *   - Serialize concurrent log writes from multiple threads.
 *   - Provide a stable CSV schema for analysis/reporting.
 *
 * Important invariants/assumptions:
 *   - All writes are protected by an internal mutex (logger_t::mtx).
 *   - logger_log() writes a single CSV row matching the declared schema.
 *   - logger_comment() writes a comment line starting with '#', which can be
 *     ignored by CSV parsers but is useful for run metadata.
 *
 * Exported API:
 *   logger_init, logger_close, logger_log, logger_comment
 */
#ifndef LOGGER_H
#define LOGGER_H

#include <pthread.h>
#include <stdio.h>
#include <stdint.h>

#include "cli.h"
#include "queue.h"   // for msg_t

typedef struct {
    FILE *fp;
    pthread_mutex_t mtx;
    int enabled;
} logger_t;

// path can be NULL -> default "run_log.csv"
int  logger_init(logger_t *lg, const char *path, const run_params_t *params);
void logger_close(logger_t *lg);

// Thread-safe log line (CSV row)
void logger_log(logger_t *lg,
                uint64_t time_ms,
                const char *event,     // "P_WRITE", "C_READ", "RUN_END", ...
                char actor_type,       // 'P' or 'C' or 'M'
                int actor_id,
                const msg_t *m,        // NULL allowed
                int q_count,
                int blocked_ms);

// Thread-safe comment line (written as "# <text>")
void logger_comment(logger_t *lg, const char *fmt, ...);

#endif
