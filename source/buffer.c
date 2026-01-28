/*
 * buffer.c
 * Purpose:
 *   Implementation of the thread-safe bounded buffer operations used by
 *   producer and consumer threads.
 *
 * Key responsibilities:
 *   - Enforce bounded-buffer invariants using semaphores + mutex.
 *   - Provide interruptible operations (timed waits) so threads can shut down
 *     promptly when stop_flag is set (timeout/abort).
 *
 * Important invariants/assumptions:
 *   - The queue is modified only while holding the mutex.
 *   - Semaphore meaning:
 *       slots = number of free slots remaining (initially capacity)
 *       items = number of queued items available (initially 0)
 *   - On any failure after acquiring a semaphore, we "undo" via sem_post to
 *     keep semaphore counts consistent with queue state.
 */
#include "buffer.h"
#include <stdatomic.h>

int buffer_init(buffer_t *b, int capacity) {
    if (!b) return -1;
    if (queue_init(&b->q, capacity) != 0) return -2;
    if (sync_init(&b->s, capacity) != 0) { queue_destroy(&b->q); return -3; }
    return 0;
}

void buffer_destroy(buffer_t *b) {
    if (!b) return;
    sync_destroy(&b->s);
    queue_destroy(&b->q);
}

int buffer_put(buffer_t *b, const msg_t *m) {
    if (!b || !m) return -1;

    // Block until there is space: "must not write to full queue"
    if (sync_wait_slots(&b->s) != 0) return -2;

    if (sync_lock(&b->s) != 0) { sync_post_slots(&b->s); return -3; }
    int rc = queue_push(&b->q, m);
    sync_unlock(&b->s);

    if (rc != 0) { // should not happen if slots semaphore is correct, but be paranoid
        sync_post_slots(&b->s);
        return -4;
    }

    // Signal an item exists: lets consumers proceed
    sync_post_items(&b->s);
    return 0;
}

int buffer_get(buffer_t *b, msg_t *out) {
    if (!b || !out) return -1;

    // Block until there is data: "must not read from empty queue"
    if (sync_wait_items(&b->s) != 0) return -2;

    if (sync_lock(&b->s) != 0) { sync_post_items(&b->s); return -3; }
    int rc = queue_pop(&b->q, out);
    sync_unlock(&b->s);

    if (rc != 0) { // again: shouldn't happen, but don't trust reality
        sync_post_items(&b->s);
        return -4;
    }

    // Signal a free slot exists: lets producers proceed
    sync_post_slots(&b->s);
    return 0;
}

int buffer_count(buffer_t *b) {
    if (!b) return -1;
    if (sync_lock(&b->s) != 0) return -2;
    int c = b->q.count;
    if (sync_unlock(&b->s) != 0) return -3;
    return c;
}

int buffer_put_interruptible(buffer_t *b, const msg_t *m, atomic_int *stop_flag, int poll_ms) {
    if (!b || !m || !stop_flag) return -1;

    while (atomic_load(stop_flag) == 0) {
        // Timed waits are used so we can periodically re-check stop_flag and
        // avoid hanging forever on a blocking sem_wait during shutdown.
        int w = sync_timedwait_slots(&b->s, poll_ms);
        if (w == 1) continue;          // timed out, re-check stop flag
        if (w != 0) return -2;         // error

        // Stop may have been requested after we acquired a slot.
        // If so, release the slot back and exit without modifying the queue.
        if (atomic_load(stop_flag) != 0) { sync_post_slots(&b->s); return 1; }

        if (sync_lock(&b->s) != 0) { sync_post_slots(&b->s); return -3; }
        // Stop may have been requested while waiting for the mutex.
        if (atomic_load(stop_flag) != 0) { sync_unlock(&b->s); sync_post_slots(&b->s); return 1; }
        int rc = queue_push(&b->q, m);
        sync_unlock(&b->s);

        if (rc != 0) { sync_post_slots(&b->s); return -4; }
        sync_post_items(&b->s);
        return 0;
    }
    return 1; // stopped
}

int buffer_get_interruptible(buffer_t *b, msg_t *out, atomic_int *stop_flag, int poll_ms) {
    if (!b || !out || !stop_flag) return -1;

    while (atomic_load(stop_flag) == 0) {
        // Timed waits are used so we can periodically re-check stop_flag and
        // avoid hanging forever on a blocking sem_wait during shutdown.
        int w = sync_timedwait_items(&b->s, poll_ms);
        if (w == 1) continue;          // timed out, re-check stop flag
        if (w != 0) return -2;         // error

        // Stop may have been requested after we acquired an item token.
        // If so, release the item token back and exit without modifying the queue.
        if (atomic_load(stop_flag) != 0) { sync_post_items(&b->s); return 1; }

        if (sync_lock(&b->s) != 0) { sync_post_items(&b->s); return -3; }
        // Stop may have been requested while waiting for the mutex.
        if (atomic_load(stop_flag) != 0) { sync_unlock(&b->s); sync_post_items(&b->s); return 1; }
        int rc = queue_pop(&b->q, out);
        sync_unlock(&b->s);

        if (rc != 0) { sync_post_items(&b->s); return -4; }
        sync_post_slots(&b->s);
        return 0;
    }
    return 1; // stopped
}
