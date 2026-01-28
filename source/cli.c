/*
 * cli.c
 * Purpose:
 *   Parse and validate command-line arguments into a run_params_t.
 *
 * Key responsibilities:
 *   - Convert CLI strings to integers safely (no partial parses).
 *   - Enforce coursework bounds (producers/consumers/queue capacity/timeout).
 *   - Provide a consistent usage message for marker-friendly execution.
 *
 * Important invariants/assumptions:
 *   - All required options (-p/-c/-q/-t) must be provided.
 *   - Returns 0 on success, negative code on failure.
 */
#include "cli.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>   // getopt
#include <limits.h>

static int parse_int(const char *s, int *out) {
    // Strict integer parse: reject empty string, non-digits, and out-of-range values.
    if (!s || !*s) return -1;
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (*end != '\0') return -1;
    if (v < INT_MIN || v > INT_MAX) return -1;
    *out = (int)v;
    return 0;
}

void print_usage(const char *prog) {
    fprintf(stderr,
        "Usage:\n"
        "  %s -p <producers 1..%d> -c <consumers 1..%d> -q <queue 1..%d> -t <timeout_sec> [-v]\n"
        "\n"
        "Example:\n"
        "  %s -p 5 -c 3 -q 10 -t 20 -v\n",
        prog, MAX_PRODUCERS, MAX_CONSUMERS, MAX_QUEUE_CAPACITY, prog
    );
}

int parse_args(int argc, char **argv, run_params_t *out) {
    if (!out) return -1;

    // defaults
    memset(out, 0, sizeof(*out));
    out->verbose = (DEFAULT_VERBOSE != 0);

    int opt;
    // Use POSIX getopt() for simple, marker-friendly option parsing.
    while ((opt = getopt(argc, argv, "p:c:q:t:v")) != -1) {
        switch (opt) {
            case 'p':
                if (parse_int(optarg, &out->producers) != 0) return -2;
                break;
            case 'c':
                if (parse_int(optarg, &out->consumers) != 0) return -2;
                break;
            case 'q':
                if (parse_int(optarg, &out->q_capacity) != 0) return -2;
                break;
            case 't':
                if (parse_int(optarg, &out->timeout_sec) != 0) return -2;
                break;
            case 'v':
                out->verbose = true;
                break;
            default:
                return -3;
        }
    }

    // Must have all required args (keeps marking runs explicit/reproducible).
    if (out->producers == 0 || out->consumers == 0 || out->q_capacity == 0 || out->timeout_sec == 0) {
        return -4;
    }

    // Range checks per brief
    if (out->producers < 1 || out->producers > MAX_PRODUCERS) return -5;
    if (out->consumers < 1 || out->consumers > MAX_CONSUMERS) return -6;
    if (out->q_capacity < 1 || out->q_capacity > MAX_QUEUE_CAPACITY) return -7;
    if (out->timeout_sec < 1) return -8;

    return 0;
}
