/*
 * queue.c
 * Purpose:
 *   Implementation of the bounded queue used by the shared buffer.
 *
 * Key responsibilities:
 *   - Maintain an array-backed container of msg_t up to a fixed capacity.
 *   - Provide FIFO insertion, and priority-aware removal with stable ordering.
 *
 * Important invariants/assumptions:
 *   - Not thread-safe; external locking is required (see buffer.c).
 *   - next_seq monotonically increases to provide a stable tie-break order.
 *   - Complexity is O(n) for pop selection and shifting; this is acceptable
 *     because coursework queue capacity is small (<= MAX_QUEUE_CAPACITY).
 */
#include "queue.h"

#include <stdlib.h>
#include <string.h>

int queue_init(queue_t *q, int capacity) {
    if (!q || capacity <= 0) return -1;
    q->buf = (msg_t*)calloc((size_t)capacity, sizeof(msg_t));
    if (!q->buf) return -2;
    q->capacity = capacity;
    q->count = 0;
    q->next_seq = 1;
    return 0;
}

void queue_destroy(queue_t *q) {
    if (!q) return;
    free(q->buf);
    q->buf = NULL;
    q->capacity = 0;
    q->count = 0;
    q->next_seq = 0;
}

bool queue_is_full(const queue_t *q) {
    return q && (q->count >= q->capacity);
}

bool queue_is_empty(const queue_t *q) {
    return !q || (q->count == 0);
}

int queue_push(queue_t *q, const msg_t *m) {
    if (!q || !m || !q->buf) return -1;
    if (queue_is_full(q)) return -2;

    // append at end (FIFO insertion)
    msg_t tmp = *m;
    tmp.seq = q->next_seq++;
    q->buf[q->count] = tmp;
    q->count++;
    return 0;
}

static int best_index_to_pop(const queue_t *q) {
    // q->count >= 1 guaranteed
    if (q->count == 1) return 0;

    int best = 0;
    for (int i = 1; i < q->count; i++) {
        // Priority rule:
        //   - higher priority value wins
        //   - if equal priority, earlier insertion (lower seq) wins (FIFO tie-break)
        if (q->buf[i].priority > q->buf[best].priority) {
            best = i;
        } else if (q->buf[i].priority == q->buf[best].priority &&
                   q->buf[i].seq < q->buf[best].seq) {
            best = i;
        }
    }
    return best;
}

int queue_pop(queue_t *q, msg_t *out) {
    if (!q || !out || !q->buf) return -1;
    if (queue_is_empty(q)) return -2;

    int idx = best_index_to_pop(q);
    *out = q->buf[idx];

    // Remove idx by shifting left (small bounded capacity => simple memmove is fine).
    if (idx < q->count - 1) {
        memmove(&q->buf[idx], &q->buf[idx + 1],
                (size_t)(q->count - idx - 1) * sizeof(msg_t));
    }
    q->count--;
    return 0;
}
