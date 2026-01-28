/*
 * workers.c
 * Purpose:
 *   Producer and consumer thread implementations for the coursework model.
 *
 * Key responsibilities:
 *   - Producers: generate messages (value + fixed priority) and write into the shared buffer.
 *   - Consumers: read messages from the shared buffer and record events.
 *   - Maintain per-thread statistics for end-of-run reporting.
 *   - Observe stop_flag and exit promptly on timeout/shutdown.
 *
 * Important invariants/assumptions:
 *   - stop_flag is checked at the top of each loop and via interruptible operations.
 *   - blocked_ms is an approximation (measured around the buffer call).
 *   - Logging must not interleave across threads (logger module provides locking).
 */
#include "workers.h"
#include "util.h"
#include "config.h"
#include "logger.h"

#include <stdio.h>
#include <stdatomic.h>
#include <stdint.h>

// Safe q-count fetch (avoids crashing if buf is NULL)
static int safe_qcount(buffer_t *buf) {
    if (!buf) return -1;
    return buffer_count(buf);
}

// Centralised logging (falls back to stderr if logger is missing).
// In normal operation lg is non-NULL; the fallback prevents silent failures during bring-up.
static void log_evt(logger_t *lg,
                    uint64_t t_rel_ms,
                    const char *event,
                    char actor_type,
                    int actor_id,
                    const msg_t *m,
                    int q_count,
                    int blocked_ms) {
    if (lg) {
        logger_log(lg, t_rel_ms, event, actor_type, actor_id, m, q_count, blocked_ms);
        return;
    }

    // Fallback: prevents silent failure during bring-up
    int value = -1, prio = -1, prod = -1;
    if (m) {
        value = m->value;
        prio  = m->priority;
        prod  = m->producer_id;
    }

    fprintf(stderr,
            "%llu,%s,%c,%d,%d,%d,%d,%d,%d\n",
            (unsigned long long)t_rel_ms,
            event ? event : "EV",
            actor_type,
            actor_id,
            value, prio, prod,
            q_count,
            blocked_ms);
}

void *producer_thread(void *arg) {
    producer_args_t *a = (producer_args_t *)arg;
    if (!a || !a->buf || !a->stop_flag) return NULL;

    {
        uint64_t t_rel = now_ms_monotonic() - a->t0_ms;
        int qcount = safe_qcount(a->buf);
        log_evt(a->lg, t_rel, "P_START", 'P', a->id, NULL, qcount, 0);
    }

    while (atomic_load(a->stop_flag) == 0) {
        // Create message: value is random, priority is producer-fixed
        msg_t m;
        m.value = rand_in_range(&a->seed, RAND_VALUE_MIN, RAND_VALUE_MAX);
        m.priority = a->fixed_priority;
        m.producer_id = a->id;

        // Approximate blocking time (includes semaphore wait + brief critical section).
        uint64_t t_before = now_ms_monotonic();
        int rc = buffer_put_interruptible(a->buf, &m, a->stop_flag, a->poll_ms);
        int blocked_ms = (int)(now_ms_monotonic() - t_before);

        if (rc == 0) {
            int qcount = safe_qcount(a->buf);
            uint64_t t_rel = now_ms_monotonic() - a->t0_ms;

            // Log event
            log_evt(a->lg, t_rel, "P_WRITE", 'P', a->id, &m, qcount, blocked_ms);

            // Update per-thread stats
            a->ops++;
            a->blocked_total_ms += (unsigned long)blocked_ms;
            if (blocked_ms > 0) a->blocked_events++;
            if (qcount > a->max_q) a->max_q = qcount;

        } else if (rc == 1) {
            // stopped
            break;
        } else {
            uint64_t t_rel = now_ms_monotonic() - a->t0_ms;
            int qcount = safe_qcount(a->buf);
            log_evt(a->lg, t_rel, "P_ERROR", 'P', a->id, &m, qcount, blocked_ms);
            fprintf(stderr, "Producer %d: buffer_put_interruptible failed (rc=%d) - exiting\n", a->id, rc);
            break;
        }

        // Random wait between writes
        int wait_s = rand_in_range(&a->seed, PRODUCER_WAIT_MIN_SEC, PRODUCER_WAIT_MAX_SEC);
        sleep_interruptible_ms(a->stop_flag, wait_s * 1000, a->poll_ms);
    }

    {
        uint64_t t_rel = now_ms_monotonic() - a->t0_ms;
        int qcount = safe_qcount(a->buf);
        log_evt(a->lg, t_rel, "P_EXIT", 'P', a->id, NULL, qcount, 0);
    }

    return NULL;
}

void *consumer_thread(void *arg) {
    consumer_args_t *a = (consumer_args_t *)arg;
    if (!a || !a->buf || !a->stop_flag) return NULL;

    {
        uint64_t t_rel = now_ms_monotonic() - a->t0_ms;
        int qcount = safe_qcount(a->buf);
        log_evt(a->lg, t_rel, "C_START", 'C', a->id, NULL, qcount, 0);
    }

    while (atomic_load(a->stop_flag) == 0) {
        msg_t out;

        // Approximate blocking time (includes semaphore wait + brief critical section).
        uint64_t t_before = now_ms_monotonic();
        int rc = buffer_get_interruptible(a->buf, &out, a->stop_flag, a->poll_ms);
        int blocked_ms = (int)(now_ms_monotonic() - t_before);

        if (rc == 0) {
            int qcount = safe_qcount(a->buf);
            uint64_t t_rel = now_ms_monotonic() - a->t0_ms;

            // Log event
            log_evt(a->lg, t_rel, "C_READ", 'C', a->id, &out, qcount, blocked_ms);

            // Update per-thread stats
            a->ops++;
            a->blocked_total_ms += (unsigned long)blocked_ms;
            if (blocked_ms > 0) a->blocked_events++;
            if (qcount > a->max_q) a->max_q = qcount;

        } else if (rc == 1) {
            // stopped
            break;
        } else {
            uint64_t t_rel = now_ms_monotonic() - a->t0_ms;
            int qcount = safe_qcount(a->buf);
            log_evt(a->lg, t_rel, "C_ERROR", 'C', a->id, NULL, qcount, blocked_ms);
            fprintf(stderr, "Consumer %d: buffer_get_interruptible failed (rc=%d) - exiting\n", a->id, rc);
            break;
        }

        // Random wait between reads
        int wait_s = rand_in_range(&a->seed, CONSUMER_WAIT_MIN_SEC, CONSUMER_WAIT_MAX_SEC);
        sleep_interruptible_ms(a->stop_flag, wait_s * 1000, a->poll_ms);
    }

    {
        uint64_t t_rel = now_ms_monotonic() - a->t0_ms;
        int qcount = safe_qcount(a->buf);
        log_evt(a->lg, t_rel, "C_EXIT", 'C', a->id, NULL, qcount, 0);
    }

    return NULL;
}
