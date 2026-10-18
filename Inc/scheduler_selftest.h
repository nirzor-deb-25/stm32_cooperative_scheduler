#ifndef SCHEDULER_SELFTEST_H
#define SCHEDULER_SELFTEST_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    /* 0: not run, 1: running, 2: passed, 3: failed. */
    uint32_t status;
    uint32_t checks_passed;
    uint32_t failure_line;
} scheduler_selftest_diagnostics_t;

extern volatile scheduler_selftest_diagnostics_t
    scheduler_selftest_diagnostics;

/* Run once at startup, before timebase_init(). */
bool scheduler_selftest_run(void);

#endif