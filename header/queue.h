/*
 * queue.h
 * Purpose:
 *   Bounded in-memory message container used by the producer-consumer model.
 *
 * Key responsibilities:
 *   - Store message tuples (value, priority, producer_id) and a sequence number for ordering.
 *   - Provide push/pop operations used by the thread-safe buffer layer.
 *
 * Important invariants/assumptions:
 *   - This module is NOT thread-safe by itself; callers must provide mutual exclusion.
 *   - queue_push() appends (FIFO insertion).
 *   - queue_pop() implements the coursework rule:
 *       - if more than one item is present, pop highest priority first
 *       - tie-break using lowest seq (earliest inserted)
 *
 * Exported API:
 *   queue_init, queue_destroy, queue_is_full, queue_is_empty, queue_push, queue_pop
 */
#ifndef QUEUE_H
#define QUEUE_H

#include <stdbool.h>

typedef struct {
    int value;      // produced data value
    int priority;   // 0..9
    int producer_id;
    unsigned long seq; // insertion order (for tie-breaking)
} msg_t;

typedef struct {
    msg_t *buf;
    int capacity;
    int count;
    unsigned long next_seq;
} queue_t;

int  queue_init(queue_t *q, int capacity);
void queue_destroy(queue_t *q);

bool queue_is_full(const queue_t *q);
bool queue_is_empty(const queue_t *q);

int queue_push(queue_t *q, const msg_t *m); // 0 ok, <0 error
int queue_pop(queue_t *q, msg_t *out);      // 0 ok, <0 empty/error

#endif

