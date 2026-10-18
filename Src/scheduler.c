#include <stddef.h>
#include <stdint.h>

#include "scheduler.h"
#include "timebase.h"

#define HALF_COUNTER_RANGE 0x80000000UL

typedef struct
{
    scheduler_callback_t callback;
    void *context;
    uint32_t period_ms;
    uint32_t next_release_ms;
} scheduler_task_t;

static scheduler_task_t task_table[SCHEDULER_MAX_TASKS];

static bool dispatch_active = false;

volatile uint32_t scheduler_task_count = 0U;

volatile scheduler_task_diagnostics_t
    scheduler_diagnostics[SCHEDULER_MAX_TASKS] = {0};

/*
 * Unsigned subtraction handles counter wraparound.
 *
 * This comparison assumes that the deadline is less than
 * half the counter range away from the current time.
 */
static bool release_is_due(uint32_t now, uint32_t release)
{
    return (uint32_t)(now - release) < HALF_COUNTER_RANGE;
}

void scheduler_init(void)
{
    scheduler_task_count = 0U;
    dispatch_active = false;

    for (uint32_t index = 0U;
         index < SCHEDULER_MAX_TASKS;
         index++)
    {
        task_table[index] = (scheduler_task_t){0};

        scheduler_diagnostics[index] =
            (scheduler_task_diagnostics_t){0};
    }
}

bool scheduler_add(
    scheduler_callback_t callback,
    void *context,
    uint32_t period_ms,
    uint32_t first_delay_ms,
    uint32_t *task_id)
{
    if ((callback == NULL) ||
        (period_ms == 0U) ||
        (period_ms >= HALF_COUNTER_RANGE) ||
        (first_delay_ms >= HALF_COUNTER_RANGE) ||
        dispatch_active ||
        (scheduler_task_count >= SCHEDULER_MAX_TASKS))
    {
        return false;
    }

    uint32_t index = scheduler_task_count;
    uint32_t release = timebase_millis() + first_delay_ms;

    task_table[index].callback = callback;
    task_table[index].context = context;
    task_table[index].period_ms = period_ms;
    task_table[index].next_release_ms = release;

    scheduler_diagnostics[index] =
        (scheduler_task_diagnostics_t){
            .enabled = 1U,
            .period_ms = period_ms,
            .next_release_ms = release
        };

    if (task_id != NULL)
    {
        *task_id = index;
    }

    scheduler_task_count = index + 1U;

    return true;
}

uint32_t scheduler_dispatch(void)
{
    /*
     * Reject recursive dispatch from inside a callback.
     * Application code calls this function from the main loop.
     */
    if (dispatch_active)
    {
        return 0U;
    }

    dispatch_active = true;

    uint32_t executed = 0U;

    for (uint32_t index = 0U;
         index < scheduler_task_count;
         index++)
    {
        scheduler_task_t *task = &task_table[index];

        volatile scheduler_task_diagnostics_t *diagnostics =
            &scheduler_diagnostics[index];

        uint32_t now = timebase_millis();
        uint32_t release = task->next_release_ms;

        if (!release_is_due(now, release))
        {
            continue;
        }

        uint32_t lateness = now - release;

        if (lateness > diagnostics->max_lateness_ms)
        {
            diagnostics->max_lateness_ms = lateness;
        }

        uint32_t start_cycles = timebase_cycles();

        task->callback(task->context);

        uint32_t duration =
            timebase_cycles() - start_cycles;

        uint32_t finished_ms = timebase_millis();

        diagnostics->last_duration_cycles = duration;

        if (duration > diagnostics->max_duration_cycles)
        {
            diagnostics->max_duration_cycles = duration;
        }

        diagnostics->run_count++;
        executed++;

        /*
         * Preserve the original periodic timeline.
         *
         * Skip releases reached while this invocation was
         * late or executing. The next release is strictly
         * after finished_ms, avoiding a catch-up burst.
         */
        uint32_t elapsed = finished_ms - release;
        uint32_t skipped = elapsed / task->period_ms;

        diagnostics->skipped_releases += skipped;

        task->next_release_ms =
            finished_ms +
            (task->period_ms - (elapsed % task->period_ms));

        diagnostics->next_release_ms =
            task->next_release_ms;
    }

    dispatch_active = false;

    return executed;
}