README: What has been built so far (Modules 1-9)
===============================================

Overview
--------
This project implements a Producer-Consumer system using POSIX threads and a shared bounded queue
(FIFO insertion with priority-aware removal).
Producers generate integer "messages" and write them to a shared queue; Consumers read messages from the queue.
The model runs for a specified timeout and then terminates cleanly. The program also logs events to CSV and
produces end-of-run performance statistics for analysis and report writing.


Module 1: Compile-time configuration (config.h)
-----------------------------------------------
A central header stores compile-time defaults required by the brief, such as:
- producer maximum sleep interval
- consumer maximum sleep interval
- maximum supported producers/consumers
- random value range (0-9)

These defaults are easy to identify and change in one place.


Module 2: Command-line interface parsing (cli.c / cli.h)
--------------------------------------------------------
Runtime parameters are passed via command line and validated:
- -p : number of producers (1-10)
- -c : number of consumers (1-3)
- -q : queue capacity (1-20)
- -t : timeout (seconds)
- -v : optional verbose mode

On invalid input, the program prints usage instructions and exits safely.


Module 3: Run summary (runinfo.c / runinfo.h)
---------------------------------------------
At program start, a concise run summary is printed including:
- time/date
- username and hostname
- runtime parameters (from CLI)
- compile-time defaults (from config.h)

This supports reproducibility and traceability of test runs.


Module 4: Queue data structure (queue.c / queue.h)
--------------------------------------------------
Implements a bounded queue storing message tuples:
- value
- priority (0-9)
- producer_id
- an internal sequence number (for stable ordering)

Read rule:
- If more than one item is present, the queue returns the item with the highest priority first.
- If priorities tie, the earliest inserted item is returned (FIFO tie-break).


Module 5: Synchronisation layer + thread-safe buffer (sync.c/.h, buffer.c/.h)
----------------------------------------------------------------------------
Implements safe bounded-buffer behaviour using:
- pthread mutex to protect queue operations (critical section)
- counting semaphore for "items available"
- counting semaphore for "slots available"

Exposes safe operations:
- buffer_put() / buffer_get()
and interruptible variants (using timed waits to allow clean shutdown):
- buffer_put_interruptible() / buffer_get_interruptible()

This enforces:
- producers do not write to a full queue
- consumers do not read from an empty queue


Module 6-7: Producer and Consumer worker threads (workers.c / workers.h)
-----------------------------------------------------------------------
Creates producer and consumer threads.

Producer behaviour:
- generates random values (within configured range)
- attaches a fixed producer-assigned priority (0-9) to each message
- writes messages into the shared buffer
- sleeps for a random time between actions (within compile-time limits)

Consumer behaviour:
- reads messages from the shared buffer
- logs what was read
- sleeps for a random time between actions (within compile-time limits)

Both thread types:
- periodically check a shared atomic stop flag
- terminate cleanly after timeout


Module 8: CSV logging (logger.c / logger.h)
-------------------------------------------
A thread-safe logger writes run_log.csv to the current working directory.
The log includes:
- metadata comment lines starting with "#"
- a CSV header row
- event rows such as RUN_START, P_START/C_START, P_WRITE, C_READ, STOP_SET_*, RUN_END, and *_ERROR
- time (ms), actor type/id, message fields, queue occupancy, and blocked time

This output supports performance analysis and report graphs.


Module 9: Performance statistics summary (main + workers stats fields)
---------------------------------------------------------------------
Each thread tracks:
- number of successful operations
- total blocked time (ms)
- number of blocked events
- maximum queue occupancy observed

At the end of the run, main aggregates and prints:
- total produced operations and total consumed operations
- runtime and throughput (items/sec)
- producer and consumer blocked time/events
- maximum queue occupancy observed during the run


How to build and run
--------------------
Build (Linux/WSL):
  make release
  # or:
  make debug

Run example (from repo root):
  make run ARGS="-p 5 -c 3 -q 10 -t 10 -v"

Run directly:
  ./build/release/learn_cw -p 5 -c 3 -q 10 -t 10 -v

Outputs:
- run summary printed to terminal
- run_log.csv created in the current working directory
- end-of-run RUN STATS printed to terminal
