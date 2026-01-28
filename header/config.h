/*
 * config.h
 * Purpose:
 *   Central location for compile-time constants used by the producer-consumer model.
 *
 * Key responsibilities:
 *   - Define bounds required by the coursework brief (max producers/consumers/queue capacity).
 *   - Define timing parameters for worker sleeps.
 *   - Define message value range used by producers.
 *
 * Important invariants/assumptions:
 *   - These are compile-time constants (not CLI options), chosen to keep marking runs reproducible.
 *   - CLI parsing enforces MAX_* limits and will reject out-of-range values.
 *
 * Exported values:
 *   PRODUCER_WAIT_{MIN,MAX}_SEC, CONSUMER_WAIT_{MIN,MAX}_SEC,
 *   MAX_PRODUCERS, MAX_CONSUMERS, MAX_QUEUE_CAPACITY,
 *   RAND_VALUE_{MIN,MAX}, DEFAULT_VERBOSE
 */
#ifndef CONFIG_H
#define CONFIG_H

// ===== Compile-time defaults (easy to find, per spec) =====
// Built-in defaults required by coursework.
#define PRODUCER_WAIT_MAX_SEC 2
#define CONSUMER_WAIT_MAX_SEC 4

#define MAX_PRODUCERS 10
#define MAX_CONSUMERS 3

#define MAX_QUEUE_CAPACITY 20

#define RAND_VALUE_MIN 0
#define RAND_VALUE_MAX 9

// Optional: min waits (not specified as CLI args in the brief, so keep simple)
#define PRODUCER_WAIT_MIN_SEC 0
#define CONSUMER_WAIT_MIN_SEC 0

// Default verbosity must be OFF by default.
#define DEFAULT_VERBOSE 0

#endif
