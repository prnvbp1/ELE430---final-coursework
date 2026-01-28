/*
 * util.c
 * Purpose:
 *   Implementation of time/sleep/RNG helpers used by the model.
 *
 * Key responsibilities:
 *   - Provide now_ms_monotonic() for relative timestamps (run timing, logging).
 *   - Provide rand_in_range() for per-thread pseudo-random behaviour without global rand().
 *   - Provide sleep_interruptible_ms() so worker threads can stop promptly after timeout.
 *
 * Important invariants/assumptions:
 *   - now_ms_monotonic() must not jump backwards (uses CLOCK_MONOTONIC on POSIX).
 *   - sleep_interruptible_ms() is best-effort and checks stop_flag between chunks.
 */
#include "util.h"

#include <time.h>
#include <stdint.h>
#ifdef _WIN32
#include <windows.h>
#include <stdlib.h>
#else
#include <errno.h>
#include <unistd.h>
#endif

uint64_t now_ms_monotonic(void) {
#ifdef _WIN32
    return (uint64_t)GetTickCount64();
#else
    // Monotonic clock for relative timing (not affected by wall-clock changes).
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
#endif
}

/* Simple reentrant RNG (portable replacement for rand_r) */
static unsigned int xrng(unsigned int *seed) {
    /* Xorshift32 */
    unsigned int x = *seed;
    if (x == 0) x = 2463534242u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *seed = x;
    return x;
}

int rand_in_range(unsigned int *seed, int lo, int hi) {
    if (hi < lo) { int t = lo; lo = hi; hi = t; }
    int span = hi - lo + 1;
    unsigned int r = xrng(seed);
    return lo + (int)(r % (unsigned)span);
}

static void sleep_ms(int ms) {
    if (ms <= 0) return;
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
        // retry with remaining time
    }
#endif
}

void sleep_seconds(int sec) {
    if (sec <= 0) return;
    sleep_ms(sec * 1000);
}

void sleep_interruptible_ms(atomic_int *stop_flag, int total_ms, int poll_ms) {
    if (total_ms <= 0) return;

    if (!stop_flag || poll_ms <= 0) {
        sleep_ms(total_ms);
        return;
    }

    int remaining = total_ms;
    while (remaining > 0 && atomic_load(stop_flag) == 0) {
        int chunk = (remaining < poll_ms) ? remaining : poll_ms;
        sleep_ms(chunk);
        remaining -= chunk;
    }
}
