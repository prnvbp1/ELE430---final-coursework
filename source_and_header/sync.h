/*
 * sync.h
 * Purpose:
 *   Synchronisation primitives for the bounded-buffer implementation.
 *
 * Key responsibilities:
 *   - Provide a mutex to protect queue critical sections.
 *   - Provide counting semaphores to represent "items available" and "slots available".
 *   - Provide timed-wait operations used to support clean shutdown (timeout).
 *
 * Important invariants/assumptions:
 *   - items semaphore counts queued items (initially 0).
 *   - slots semaphore counts free capacity (initially = capacity).
 *   - Callers must keep semaphore operations consistent with queue updates.
 *
 * Exported API:
 *   sync_init, sync_destroy,
 *   sync_lock, sync_unlock,
 *   sync_wait_items, sync_post_items,
 *   sync_wait_slots, sync_post_slots,
 *   sync_timedwait_items, sync_timedwait_slots
 */
#ifndef SYNC_H
#define SYNC_H

#include <pthread.h>
#include <semaphore.h>

typedef struct {
    pthread_mutex_t mtx;  // protects queue operations (critical section)
    sem_t items;          // number of items currently available to consume
    sem_t slots;          // number of free slots currently available to produce into
} sync_t;

int  sync_init(sync_t *s, int capacity);
void sync_destroy(sync_t *s);

int sync_lock(sync_t *s);
int sync_unlock(sync_t *s);

int sync_wait_items(sync_t *s);
int sync_post_items(sync_t *s);

int sync_wait_slots(sync_t *s);
int sync_post_slots(sync_t *s);

int sync_timedwait_items(sync_t *s, int timeout_ms); // 0 got it, 1 timeout, <0 error
int sync_timedwait_slots(sync_t *s, int timeout_ms);


#endif
