/*
 * cli.h
 * Purpose:
 *   Define the run-time configuration structure and the CLI parsing API.
 *
 * Key responsibilities:
 *   - Represent validated run parameters (producers, consumers, queue size, timeout, verbosity).
 *   - Provide parsing and usage-printing functions for main().
 *
 * Important invariants/assumptions:
 *   - parse_args() validates ranges and returns 0 on success.
 *   - On failure, parse_args() returns a negative code and main() exits safely.
 *
 * Exported API:
 *   parse_args, print_usage
 */
#ifndef CLI_H
#define CLI_H

#include <stdbool.h>

typedef struct {
    int producers;     // 1..10
    int consumers;     // 1..3
    int q_capacity;    // 1..20
    int timeout_sec;   // >0
    bool verbose;      // default off
} run_params_t;

int parse_args(int argc, char **argv, run_params_t *out);
void print_usage(const char *prog);

#endif
