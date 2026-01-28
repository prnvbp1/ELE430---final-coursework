/*
 * util.h
 * Purpose:
 *   Small utilities used across the coursework codebase (time, RNG, sleep).
 *
 * Key responsibilities:
 *   - Provide a monotonic millisecond clock for relative timestamps.
 *   - Provide a simple re-entrant RNG helper.
 *   - Provide an interruptible sleep to support prompt shutdown (stop_flag).
 *
 * Exported API:
 *   now_ms_monotonic, rand_in_range, sleep_seconds, sleep_interruptible_ms
 */
#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stdatomic.h>

uint64_t now_ms_monotonic(void);
int rand_in_range(unsigned int *seed, int lo, int hi);

void sleep_seconds(int sec);
void sleep_interruptible_ms(atomic_int *stop_flag, int total_ms, int poll_ms);

#endif
