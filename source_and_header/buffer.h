/*
 * buffer.h
 * Purpose:
 *   Thread-safe bounded-buffer wrapper that combines the queue implementation
 *   with the synchronisation primitives (mutex + semaphores).
 *
 * Key responsibilities:
 *   - Initialise/destroy the shared buffer resources.
 *   - Provide safe producer/consumer operations (put/get).
 *   - Provide interruptible variants to support clean shutdown on timeout.
 *
 * Important invariants/assumptions:
 *   - All queue operations are protected by the internal mutex.
 *   - Semaphore counts reflect queue state:
 *       slots = free capacity remaining, items = number of queued items.
 *   - capacity is fixed at init time and never changes.
 *
 * Exported API:
 *   buffer_init, buffer_destroy,
 *   buffer_put, buffer_get, buffer_count,
 *   buffer_put_interruptible, buffer_get_interruptible
 */
#ifndef BUFFER_H
#define BUFFER_H

#include "queue.h"
#include "sync.h"
#include <stdatomic.h>


typedef struct {
    queue_t q;
    sync_t  s;
} buffer_t;

int  buffer_init(buffer_t *b, int capacity);
void buffer_destroy(buffer_t *b);

// Thread-safe put/get
int buffer_put(buffer_t *b, const msg_t *m);
int buffer_get(buffer_t *b, msg_t *out);

// Optional helper for instrumentation later
int buffer_count(buffer_t *b);


int buffer_put_interruptible(buffer_t *b, const msg_t *m, atomic_int *stop_flag, int poll_ms);
int buffer_get_interruptible(buffer_t *b, msg_t *out, atomic_int *stop_flag, int poll_ms);

#endif
