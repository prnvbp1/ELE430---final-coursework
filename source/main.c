/*
 * main.c
 * Purpose:
 *   Program entry point: wires together CLI parsing, buffer/logger init,
 *   thread creation, timeout-driven shutdown, and end-of-run reporting.
 *
 * Key responsibilities:
 *   - Parse and validate CLI arguments.
 *   - Initialise shared buffer and CSV logger.
 *   - Spawn producer and consumer threads.
 *   - Enforce timeout shutdown via an atomic stop flag.
 *   - Join all started threads and release all resources cleanly.
 *
 * Important invariants/assumptions:
 *   - stop_flag is the single shutdown signal (set by main on timeout or init failure).
 *   - Worker threads must periodically observe stop_flag and exit promptly.
 *   - Resources are destroyed exactly once via the cleanup section.
 */
#include "cli.h"
#include "runinfo.h"
#include "buffer.h"
#include "workers.h"
#include "util.h"
#include "logger.h"
#include "config.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>   // strerror
#include <unistd.h>   // getpid, sleep

int main(int argc, char **argv) {
    run_params_t params;
    int exit_code = EXIT_FAILURE;

    // ---- Parse CLI args ----
    int rc = parse_args(argc, argv, &params);
    if (rc != 0) {
        print_usage(argv[0]);
        fprintf(stderr, "parse_args failed (rc=%d)\n", rc);
        return EXIT_FAILURE;
    }

    // ---- Print run summary (stdout) ----
    print_run_summary(&params);

    buffer_t buf;
    int buf_ok = 0;

    logger_t lg;
    int lg_ok = 0;

    // ---- Init shared buffer (queue + sync primitives) ----
    int brc = buffer_init(&buf, params.q_capacity);
    if (brc != 0) {
        fprintf(stderr, "buffer_init failed (capacity=%d, rc=%d)\n", params.q_capacity, brc);
        goto cleanup;
    }
    buf_ok = 1;

    // ---- Init logger (CSV file) ----
    const char *log_path = "run_log.csv";
    int lrc = logger_init(&lg, log_path, &params);
    if (lrc != 0) {
        fprintf(stderr, "logger_init failed (path=%s, rc=%d)\n", log_path, lrc);
        goto cleanup;
    }
    lg_ok = 1;

    // ---- Stop flag (C11 atomic) ----
    // This is the single shutdown signal observed by all workers.
    atomic_int stop_flag = ATOMIC_VAR_INIT(0);

    // ---- Timing ----
    uint64_t t0 = now_ms_monotonic();

    logger_log(&lg, 0, "RUN_START", 'M', 0, NULL, 0, 0);
    logger_comment(&lg,
                   "defaults PRODUCER_WAIT_MAX_SEC=%d CONSUMER_WAIT_MAX_SEC=%d RAND_VALUE_RANGE=[%d..%d]",
                   PRODUCER_WAIT_MAX_SEC, CONSUMER_WAIT_MAX_SEC, RAND_VALUE_MIN, RAND_VALUE_MAX);

    // Poll interval for interruptible sem waits (lets threads exit on timeout cleanly)
    const int poll_ms = 200;

    // ---- Thread storage ----
    pthread_t prod[MAX_PRODUCERS] = {0};
    pthread_t cons[MAX_CONSUMERS] = {0};
    producer_args_t pargs[MAX_PRODUCERS] = {0};
    consumer_args_t cargs[MAX_CONSUMERS] = {0};

    int started_producers = 0;
    int started_consumers = 0;
    int start_failed = 0;

    // ---- Create producers ----
    for (int i = 0; i < params.producers; i++) {
        pargs[i].buf = &buf;
        pargs[i].stop_flag = &stop_flag;
        pargs[i].lg = &lg;

        pargs[i].id = i;
        pargs[i].fixed_priority = i % 10;  // deterministic 0..9 assignment
        pargs[i].seed = (unsigned int)(getpid() ^ (i * 2654435761u));

        // init stats
        pargs[i].ops = 0;
        pargs[i].blocked_total_ms = 0;
        pargs[i].blocked_events = 0;
        pargs[i].max_q = 0;

        pargs[i].t0_ms = t0;
        pargs[i].poll_ms = poll_ms;
        pargs[i].verbose = params.verbose ? 1 : 0;

        int prc = pthread_create(&prod[i], NULL, producer_thread, &pargs[i]);
        if (prc != 0) {
            fprintf(stderr, "pthread_create failed for producer %d (rc=%d: %s)\n", i, prc, strerror(prc));
            atomic_store(&stop_flag, 1);
            start_failed = 1;
            break;
        }

        started_producers++;
    }

    // ---- Create consumers ----
    if (!start_failed) {
        for (int i = 0; i < params.consumers; i++) {
            cargs[i].buf = &buf;
            cargs[i].stop_flag = &stop_flag;
            cargs[i].lg = &lg;

            cargs[i].id = i;
            cargs[i].seed = (unsigned int)(getpid() ^ (i * 2246822519u));

            // init stats
            cargs[i].ops = 0;
            cargs[i].blocked_total_ms = 0;
            cargs[i].blocked_events = 0;
            cargs[i].max_q = 0;

            cargs[i].t0_ms = t0;
            cargs[i].poll_ms = poll_ms;
            cargs[i].verbose = params.verbose ? 1 : 0;

            int crc = pthread_create(&cons[i], NULL, consumer_thread, &cargs[i]);
            if (crc != 0) {
                fprintf(stderr, "pthread_create failed for consumer %d (rc=%d: %s)\n", i, crc, strerror(crc));
                atomic_store(&stop_flag, 1);
                start_failed = 1;
                break;
            }

            started_consumers++;
        }
    }

    // ---- Run until timeout (unless init failed) ----
    if (!start_failed) {
        sleep((unsigned)params.timeout_sec);
        atomic_store(&stop_flag, 1);

        uint64_t t_rel = now_ms_monotonic() - t0;
        int qcount = buffer_count(&buf);
        // Record shutdown reason in the CSV (helps marking/debug).
        logger_log(&lg, t_rel, "STOP_SET_TIMEOUT", 'M', 0, NULL, qcount, 0);
    } else {
        uint64_t t_rel = now_ms_monotonic() - t0;
        int qcount = buffer_count(&buf);
        logger_log(&lg, t_rel, "STOP_SET_INIT_FAIL", 'M', 0, NULL, qcount, 0);
    }

    // ---- Join threads ----
    for (int i = 0; i < started_producers; i++) {
        int jrc = pthread_join(prod[i], NULL);
        if (jrc != 0) {
            fprintf(stderr, "pthread_join failed for producer %d (rc=%d: %s)\n", i, jrc, strerror(jrc));
        }
    }
    for (int i = 0; i < started_consumers; i++) {
        int jrc = pthread_join(cons[i], NULL);
        if (jrc != 0) {
            fprintf(stderr, "pthread_join failed for consumer %d (rc=%d: %s)\n", i, jrc, strerror(jrc));
        }
    }

    // ---- Aggregate stats ----
    unsigned long prod_ops = 0, cons_ops = 0;
    unsigned long prod_block_ms = 0, cons_block_ms = 0;
    unsigned long prod_block_ev = 0, cons_block_ev = 0;
    int max_q_seen = 0;

    for (int i = 0; i < started_producers; i++) {
        prod_ops      += pargs[i].ops;
        prod_block_ms += pargs[i].blocked_total_ms;
        prod_block_ev += pargs[i].blocked_events;
        if (pargs[i].max_q > max_q_seen) max_q_seen = pargs[i].max_q;
    }

    for (int i = 0; i < started_consumers; i++) {
        cons_ops      += cargs[i].ops;
        cons_block_ms += cargs[i].blocked_total_ms;
        cons_block_ev += cargs[i].blocked_events;
        if (cargs[i].max_q > max_q_seen) max_q_seen = cargs[i].max_q;
    }

    uint64_t t_end = now_ms_monotonic();
    double runtime_s = (double)(t_end - t0) / 1000.0;
    double throughput = (runtime_s > 0.0) ? ((double)cons_ops / runtime_s) : 0.0;

    printf("\n=== RUN STATS ===\n");
    printf("Produced ops : %lu\n", prod_ops);
    printf("Consumed ops : %lu\n", cons_ops);
    printf("Runtime      : %.3f s\n", runtime_s);
    printf("Throughput   : %.3f items/sec\n", throughput);
    printf("Prod blocked : %lu ms across %lu events\n", prod_block_ms, prod_block_ev);
    printf("Cons blocked : %lu ms across %lu events\n", cons_block_ms, cons_block_ev);
    printf("Max queue q  : %d\n", max_q_seen);
    printf("=============\n\n");

    logger_comment(&lg,
                   "summary start_failed=%d started_producers=%d started_consumers=%d "
                   "prod_ops=%lu cons_ops=%lu runtime_s=%.3f throughput=%.3f "
                   "prod_block_ms=%lu prod_block_ev=%lu cons_block_ms=%lu cons_block_ev=%lu max_q=%d",
                   start_failed, started_producers, started_consumers,
                   prod_ops, cons_ops, runtime_s, throughput,
                   prod_block_ms, prod_block_ev, cons_block_ms, cons_block_ev, max_q_seen);

    // ---- Log end of run ----
    uint64_t t_rel = t_end - t0;
    int qcount_end = buffer_count(&buf);
    logger_log(&lg, t_rel, "RUN_END", 'M', 0, NULL, qcount_end, 0);

    exit_code = start_failed ? EXIT_FAILURE : EXIT_SUCCESS;

cleanup:
    if (lg_ok) logger_close(&lg);
    if (buf_ok) buffer_destroy(&buf);
    return exit_code;
}
