/*
 * runinfo.h
 * Purpose:
 *   Print a concise, marker-friendly run summary at program start.
 *
 * Key responsibilities:
 *   - Report runtime parameters (CLI) and relevant compile-time defaults.
 *   - Provide basic traceability for evidence generation (time, user/host).
 *
 * Exported API:
 *   print_run_summary
 */
#ifndef RUNINFO_H
#define RUNINFO_H

#include "cli.h"

void print_run_summary(const run_params_t *p);

#endif
