#include <stddef.h>
#include <stdint.h>

#include "scheduler.h"
#include "scheduler_selftest.h"

/*
 * Test-only access to the real timebase counter.
 * Interrupts are disabled while tests control this value.
 */
extern volatile uint32_t coop_milliseconds;

volatile scheduler_selftest_diagnostics_t
    scheduler_selftest_diagnostics = {0};

static uint32_t callback_count;
static uint32_t callback_advance_ms;

#define CHECK(condition)                                      \
    do                                                        \
    {                                                         \
        if (!(condition))                                     \
        {                                                     \
            scheduler_selftest_diagnostics.failure_line =     \
                (uint32_t)__LINE__;                           \
            return false;                                     \
        }                                                     \
        scheduler_selftest_diagnostics.checks_passed++;        \
    } while (0)

static void test_callback(void *context)
{
    (void)context;

    callback_count++;

    /* Simulate elapsed time during a callback. */
    coop_milliseconds += callback_advance_ms;
}

static void prepare_test(uint32_t now)
{
    scheduler_init();
    coop_milliseconds = now;
    callback_count = 0U;
    callback_advance_ms = 0U;
}

static bool run_cases(void)
{
    /* Test 1: before, exactly at, and after a release. */
    prepare_test(1000U);

    CHECK(scheduler_add(
        test_callback, NULL, 100U, 100U, NULL));

    CHECK(scheduler_dispatch() == 0U);

    coop_milliseconds = 1099U;
    CHECK(scheduler_dispatch() == 0U);

    coop_milliseconds = 1100U;
    CHECK(scheduler_dispatch() == 1U);
    CHECK(callback_count == 1U);
    CHECK(scheduler_diagnostics[0].next_release_ms == 1200U);

    /* Repeated polling must not run the same release twice. */
    CHECK(scheduler_dispatch() == 0U);

    /* Test 2: late dispatch skips old releases. */
    prepare_test(0U);

    CHECK(scheduler_add(
        test_callback, NULL, 100U, 100U, NULL));

    coop_milliseconds = 350U;

    CHECK(scheduler_dispatch() == 1U);
    CHECK(scheduler_diagnostics[0].max_lateness_ms == 250U);
    CHECK(scheduler_diagnostics[0].skipped_releases == 2U);
    CHECK(scheduler_diagnostics[0].next_release_ms == 400U);
    CHECK(scheduler_dispatch() == 0U);

    /* Test 3: a long callback also causes skipped releases. */
    prepare_test(0U);

    CHECK(scheduler_add(
        test_callback, NULL, 100U, 100U, NULL));

    callback_advance_ms = 250U;
    coop_milliseconds = 100U;

    CHECK(scheduler_dispatch() == 1U);
    CHECK(coop_milliseconds == 350U);
    CHECK(scheduler_diagnostics[0].skipped_releases == 2U);
    CHECK(scheduler_diagnostics[0].next_release_ms == 400U);

    /* Test 4: a future release across clock wraparound. */
    prepare_test(0xFFFFFFF0UL);

    CHECK(scheduler_add(
        test_callback, NULL, 32U, 32U, NULL));

    CHECK(scheduler_diagnostics[0].next_release_ms == 0x10U);
    CHECK(scheduler_dispatch() == 0U);

    coop_milliseconds = 0xFFFFFFFFUL;
    CHECK(scheduler_dispatch() == 0U);

    coop_milliseconds = 0U;
    CHECK(scheduler_dispatch() == 0U);

    coop_milliseconds = 0x10U;
    CHECK(scheduler_dispatch() == 1U);
    CHECK(scheduler_diagnostics[0].next_release_ms == 0x30U);
    CHECK(scheduler_diagnostics[0].skipped_releases == 0U);

    /* Test 5: the clock wraps during callback execution. */
    prepare_test(0xFFFFFFF0UL);

    CHECK(scheduler_add(
        test_callback, NULL, 16U, 0U, NULL));

    callback_advance_ms = 40U;

    CHECK(scheduler_dispatch() == 1U);
    CHECK(coop_milliseconds == 0x18U);
    CHECK(scheduler_diagnostics[0].skipped_releases == 2U);
    CHECK(scheduler_diagnostics[0].next_release_ms == 0x20U);

    /* Test 6: invalid registrations and table capacity. */
    prepare_test(0U);

    CHECK(!scheduler_add(
        NULL, NULL, 100U, 0U, NULL));

    CHECK(!scheduler_add(
        test_callback, NULL, 0U, 0U, NULL));

    CHECK(!scheduler_add(
        test_callback, NULL, 0x80000000UL, 0U, NULL));

    CHECK(!scheduler_add(
        test_callback, NULL, 100U, 0x80000000UL, NULL));

    CHECK(scheduler_task_count == 0U);

    for (uint32_t index = 0U;
         index < SCHEDULER_MAX_TASKS;
         index++)
    {
        uint32_t task_id = UINT32_MAX;

        CHECK(scheduler_add(
            test_callback, NULL, 100U, 0U, &task_id));

        CHECK(task_id == index);
    }

    CHECK(!scheduler_add(
        test_callback, NULL, 100U, 0U, NULL));

    CHECK(scheduler_task_count == SCHEDULER_MAX_TASKS);

    return true;
}

bool scheduler_selftest_run(void)
{
    uint32_t saved_primask;

    __asm volatile (
        "mrs %0, primask\n"
        "cpsid i"
        : "=r" (saved_primask)
        :
        : "memory"
    );

    uint32_t saved_milliseconds = coop_milliseconds;

    scheduler_selftest_diagnostics.status = 1U;
    scheduler_selftest_diagnostics.checks_passed = 0U;
    scheduler_selftest_diagnostics.failure_line = 0U;

    bool passed = run_cases();

    /* Remove test registrations before the real application starts. */
    scheduler_init();
    coop_milliseconds = saved_milliseconds;

    scheduler_selftest_diagnostics.status = passed ? 2U : 3U;

    __asm volatile (
        "msr primask, %0"
        :
        : "r" (saved_primask)
        : "memory"
    );

    return passed;
}