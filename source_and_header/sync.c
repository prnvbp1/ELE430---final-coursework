/*
 * sync.c
 * Purpose:
 *   Implementation of mutex + semaphore helpers used by the bounded buffer.
 *
 * Key responsibilities:
 *   - Initialise/destroy pthread mutex and POSIX semaphores.
 *   - Provide EINTR-safe wait helpers.
 *   - Provide timed wait wrappers used by interruptible buffer operations.
 *
 * Important invariants/assumptions:
 *   - sem_timedwait() expects an absolute timeout based on CLOCK_REALTIME.
 *   - Callers implement the higher-level logic for shutdown using stop_flag;
 *     timed waits exist purely to allow periodic stop checks.
 */
#include "sync.h"
#include <errno.h>
#include <time.h>

int sync_init(sync_t *s, int capacity) {
    if (!s || capacity <= 0) return -1;

    if (pthread_mutex_init(&s->mtx, NULL) != 0) return -2;
    if (sem_init(&s->items, 0, 0) != 0) { pthread_mutex_destroy(&s->mtx); return -3; }
    if (sem_init(&s->slots, 0, (unsigned)capacity) != 0) {
        sem_destroy(&s->items);
        pthread_mutex_destroy(&s->mtx);
        return -4;
    }
    return 0;
}

void sync_destroy(sync_t *s) {
    if (!s) return;
    sem_destroy(&s->items);
    sem_destroy(&s->slots);
    pthread_mutex_destroy(&s->mtx);
}

int sync_lock(sync_t *s)   { return (!s) ? -1 : pthread_mutex_lock(&s->mtx); }
int sync_unlock(sync_t *s) { return (!s) ? -1 : pthread_mutex_unlock(&s->mtx); }

static int sem_wait_retry(sem_t *sem) {
    // Retry if interrupted by a signal (EINTR) so waits remain robust.
    int rc;
    do { rc = sem_wait(sem); } while (rc == -1 && errno == EINTR);
    return rc; // 0 ok, -1 fail
}

int sync_wait_items(sync_t *s) { return (!s) ? -1 : sem_wait_retry(&s->items); }
int sync_post_items(sync_t *s) { return (!s) ? -1 : sem_post(&s->items); }

int sync_wait_slots(sync_t *s) { return (!s) ? -1 : sem_wait_retry(&s->slots); }
int sync_post_slots(sync_t *s) { return (!s) ? -1 : sem_post(&s->slots); }


static void add_ms_to_timespec(struct timespec *ts, int ms) {
    ts->tv_sec  += ms / 1000;
    ts->tv_nsec += (ms % 1000) * 1000000L;
    if (ts->tv_nsec >= 1000000000L) {
        ts->tv_sec += 1;
        ts->tv_nsec -= 1000000000L;
    }
}

static int sem_timedwait_retry(sem_t *sem, int timeout_ms) {
    struct timespec ts;
    // sem_timedwait uses an absolute time on CLOCK_REALTIME.
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) return -2;
    add_ms_to_timespec(&ts, timeout_ms);

    int rc;
    do { rc = sem_timedwait(sem, &ts); } while (rc == -1 && errno == EINTR);

    if (rc == 0) return 0;
    if (errno == ETIMEDOUT) return 1;
    return -1;
}

int sync_timedwait_items(sync_t *s, int timeout_ms) {
    if (!s) return -1;
    return sem_timedwait_retry(&s->items, timeout_ms);
}

int sync_timedwait_slots(sync_t *s, int timeout_ms) {
    if (!s) return -1;
    return sem_timedwait_retry(&s->slots, timeout_ms);
}
