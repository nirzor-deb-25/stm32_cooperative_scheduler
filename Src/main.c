#include <stdbool.h>
#include <stdint.h>

#include "stm32_regs.h"
#include "timebase.h"
#include "scheduler.h"
#include "scheduler_selftest.h"

#define ALL_LED_MASK (0xFUL << 12)

/* Diagnostic controls and counters, visible through ST-LINK. */
volatile uint32_t coop_stage = 0U;
volatile uint32_t coop_failure_code = 0U;
volatile uint32_t coop_work_ms = 0U;
volatile uint32_t coop_work_count = 0U;
volatile uint32_t coop_green_count = 0U;
volatile uint32_t coop_blue_count = 0U;

typedef struct
{
    uint32_t pin;
    bool on;
} led_context_t;

static led_context_t green_led = {
    .pin = 12U,
    .on = false
};

static led_context_t blue_led = {
    .pin = 15U,
    .on = false
};

static void leds_init(void)
{
    RCC_AHB1ENR |= (1UL << 3);
    (void)RCC_AHB1ENR;

    GPIOD_BSRR = ALL_LED_MASK << 16;

    GPIOD_OTYPER &= ~ALL_LED_MASK;
    GPIOD_OSPEEDR &= ~(0xFFUL << 24);
    GPIOD_PUPDR &= ~(0xFFUL << 24);

    GPIOD_MODER =
        (GPIOD_MODER & ~(0xFFUL << 24)) |
        (0x55UL << 24);
}

__attribute__((noreturn))
static void demo_fail(uint32_t code)
{
    coop_failure_code = code;

    /* Red LD5 indicates a setup failure. */
    GPIOD_BSRR = (1UL << 14);

    for (;;)
    {
        __asm volatile ("nop");
    }
}

static void toggle_led(led_context_t *led)
{
    led->on = !led->on;

    GPIOD_BSRR = led->on
        ? (1UL << led->pin)
        : (1UL << (led->pin + 16U));
}

static void green_task(void *context)
{
    toggle_led((led_context_t *)context);
    coop_green_count++;
}

static void blue_task(void *context)
{
    toggle_led((led_context_t *)context);
    coop_blue_count++;
}

static void workload_task(void *context)
{
    (void)context;

    /*
     * Deliberately occupy the CPU without returning.
     * Interrupts remain enabled, so SysTick continues.
     *
     * This is a scheduling experiment, not a recommended
     * way to implement ordinary application delays.
     */
    uint32_t milliseconds = coop_work_ms;

    if (milliseconds > 2000U)
    {
        milliseconds = 2000U;
    }

    uint32_t required_cycles =
        milliseconds * TIMEBASE_CYCLES_PER_MS;

    uint32_t start = timebase_cycles();

    while ((uint32_t)(timebase_cycles() - start) <
           required_cycles)
    {
        __asm volatile ("nop");
    }

    coop_work_count++;
}

int main(void)
{
    leds_init();
    coop_stage = 1U;

    if (!scheduler_selftest_run())
    {
        demo_fail(4U);
    }

    timebase_init();
    scheduler_init();

    if (!scheduler_add(
            green_task,
            &green_led,
            500U,
            500U,
            0))
    {
        demo_fail(1U);
    }

    if (!scheduler_add(
            blue_task,
            &blue_led,
            125U,
            125U,
            0))
    {
        demo_fail(2U);
    }

    if (!scheduler_add(
            workload_task,
            0,
            1000U,
            1000U,
            0))
    {
        demo_fail(3U);
    }

    coop_stage = 2U;

    __asm volatile ("cpsie i" ::: "memory");

    coop_stage = 3U;

    for (;;)
    {
        (void)scheduler_dispatch();
    }
}