/*
 * workers.h
 * Purpose:
 *   Define producer/consumer thread entry points and their argument structures.
 *
 * Key responsibilities:
 *   - Hold per-thread configuration (IDs, RNG seed, priorities) and per-thread statistics.
 *   - Declare pthread entry points used by main() when spawning workers.
 *
 * Important invariants/assumptions:
 *   - The args structs are allocated in main() and remain valid until after pthread_join().
 *   - Each worker writes only to its own stats fields; main reads them after join.
 *   - stop_flag is shared across all threads and is the single shutdown signal.
 *
 * Exported API:
 *   producer_thread, consumer_thread
 */
#ifndef WORKERS_H
#define WORKERS_H

#include "buffer.h"
#include "cli.h"
#include "logger.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>

typedef struct {
    buffer_t *buf;
    atomic_int *stop_flag;
    logger_t *lg;

    int id;
    int fixed_priority;     // 0..9 per producer
    unsigned int seed;

    // Per-thread statistics (written by the worker thread; read by main after pthread_join)
    unsigned long ops;               // Successful operations (writes)
    unsigned long blocked_total_ms;  // Total blocked time (ms)
    unsigned long blocked_events;    // Count of ops where blocked_ms > 0
    int max_q;                       // Maximum queue occupancy observed

    uint64_t t0_ms;
    int poll_ms;
    int verbose;
} producer_args_t;

typedef struct {
    buffer_t *buf;
    atomic_int *stop_flag;
    logger_t *lg;

    int id;
    unsigned int seed;

    // Per-thread statistics (written by the worker thread; read by main after pthread_join)
    unsigned long ops;               // Successful operations (reads)
    unsigned long blocked_total_ms;  // Total blocked time (ms)
    unsigned long blocked_events;    // Count of ops where blocked_ms > 0
    int max_q;                       // Maximum queue occupancy observed

    uint64_t t0_ms;
    int poll_ms;
    int verbose;
} consumer_args_t;

void *producer_thread(void *arg);
void *consumer_thread(void *arg);

#endif // WORKERS_H
