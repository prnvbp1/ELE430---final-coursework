/*
 * runinfo.c
 * Purpose:
 *   Implementation of the run-summary output printed at program start.
 *
 * Key responsibilities:
 *   - Gather basic environment details (time, user, hostname) in a safe way.
 *   - Print CLI parameters and compile-time defaults to support reproducible runs.
 *
 * Important invariants/assumptions:
 *   - This module only prints; it does not modify program state.
 *   - Platform-specific APIs are isolated behind small helper functions.
 */
#include "runinfo.h"
#include "config.h"

#include <stdio.h>
#include <time.h>
#include <string.h>
#ifdef _WIN32
#include <stdlib.h>
#else
#include <unistd.h>
#include <pwd.h>
#endif

static void safe_get_username(char *buf, size_t n) {
    if (!buf || n == 0) return;
    buf[0] = '\0';

#ifdef _WIN32
    const char *u = getenv("USERNAME");
    if (!u) u = getenv("USER");
    if (u) {
        strncpy(buf, u, n - 1);
        buf[n - 1] = '\0';
    } else {
        strncpy(buf, "unknown_user", n - 1);
        buf[n - 1] = '\0';
    }
#else
    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_name) {
        strncpy(buf, pw->pw_name, n - 1);
        buf[n - 1] = '\0';
    } else {
        strncpy(buf, "unknown_user", n - 1);
        buf[n - 1] = '\0';
    }
#endif
}

static void safe_get_hostname(char *buf, size_t n) {
    if (!buf || n == 0) return;
#ifdef _WIN32
    const char *h = getenv("COMPUTERNAME");
    if (!h) h = getenv("HOSTNAME");
    if (h) {
        strncpy(buf, h, n - 1);
        buf[n - 1] = '\0';
    } else {
        strncpy(buf, "unknown_host", n - 1);
        buf[n - 1] = '\0';
    }
#else
    if (gethostname(buf, n) != 0) {
        strncpy(buf, "unknown_host", n - 1);
        buf[n - 1] = '\0';
    } else {
        buf[n - 1] = '\0';
    }
#endif
}

static void format_now(char *buf, size_t n) {
    if (!buf || n == 0) return;
    time_t t = time(NULL);
    struct tm tmv;
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    strftime(buf, n, "%Y-%m-%d %H:%M:%S", &tmv);
}

void print_run_summary(const run_params_t *p) {
    char user[64], host[64], now[64];
    safe_get_username(user, sizeof user);
    safe_get_hostname(host, sizeof host);
    format_now(now, sizeof now);

    // Summary content is intended for markers: short, explicit, and reproducible.
    printf("=== ELE430 Producer-Consumer Model Run Summary ===\n");
    printf("Time/Date: %s\n", now);
    printf("User@Host: %s@%s\n", user, host);

    printf("\n-- Run-time parameters (CLI) --\n");
    printf("Producers: %d\n", p->producers);
    printf("Consumers: %d\n", p->consumers);
    printf("Queue cap: %d\n", p->q_capacity);
    printf("Timeout : %d s\n", p->timeout_sec);
    printf("Verbose : %s\n", p->verbose ? "ON" : "OFF");

    printf("\n-- Compiled model parameters (defaults) --\n");
    printf("PRODUCER_WAIT_MAX_SEC = %d\n", PRODUCER_WAIT_MAX_SEC);
    printf("CONSUMER_WAIT_MAX_SEC = %d\n", CONSUMER_WAIT_MAX_SEC);
    printf("MAX_PRODUCERS         = %d\n", MAX_PRODUCERS);
    printf("MAX_CONSUMERS         = %d\n", MAX_CONSUMERS);
    printf("RAND_VALUE_RANGE      = [%d..%d]\n", RAND_VALUE_MIN, RAND_VALUE_MAX);

    // Initial state at program start
    printf("\n-- Initial state --\n");
    printf("Queue: EMPTY (initial)\n");
    printf("Producers: NOT STARTED\n");
    printf("Consumers: NOT STARTED\n");
    printf("=================================================\n\n");
}
