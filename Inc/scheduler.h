#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdbool.h>
#include <stdint.h>

#define SCHEDULER_MAX_TASKS 8U

typedef void (*scheduler_callback_t)(void *context);

/* Eight consecutive 32-bit words per task. */
typedef struct
{
    uint32_t enabled;
    uint32_t period_ms;
    uint32_t next_release_ms;
    uint32_t run_count;
    uint32_t skipped_releases;
    uint32_t max_lateness_ms;
    uint32_t max_duration_cycles;
    uint32_t last_duration_cycles;
} scheduler_task_diagnostics_t;

extern volatile uint32_t scheduler_task_count;

extern volatile scheduler_task_diagnostics_t
    scheduler_diagnostics[SCHEDULER_MAX_TASKS];

/* Call once before registering tasks. */
void scheduler_init(void);

/*
 * Register tasks before entering the dispatch loop.
 *
 * period_ms must be 1 through 0x7FFFFFFF.
 * first_delay_ms must be less than 0x80000000.
 *
 * task_id may be NULL if the caller does not need the ID.
 */
bool scheduler_add(
    scheduler_callback_t callback,
    void *context,
    uint32_t period_ms,
    uint32_t first_delay_ms,
    uint32_t *task_id
);

/*
 * Scan the task table once.
 * Run each due task at most once during this scan.
 * Return the number of callbacks executed.
 */
uint32_t scheduler_dispatch(void);

#endif