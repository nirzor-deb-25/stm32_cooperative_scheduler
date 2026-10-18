#include "timebase.h"
#include "stm32_regs.h"

#define TIMEBASE_SYST_CSR REG32(0xE000E010UL)
#define TIMEBASE_SYST_RVR REG32(0xE000E014UL)
#define TIMEBASE_SYST_CVR REG32(0xE000E018UL)

#define TIMEBASE_SCB_ICSR REG32(0xE000ED04UL)
#define TIMEBASE_SYST_PRIORITY REG8(0xE000ED23UL)

volatile uint32_t coop_milliseconds = 0U;

void timebase_init(void)
{
    /* Stop SysTick before configuring it. */
    TIMEBASE_SYST_CSR = 0U;

    /* Clear a pending SysTick exception. */
    TIMEBASE_SCB_ICSR = (1UL << 25);

    coop_milliseconds = 0U;

    /* Enable the CPU cycle counter for duration measurements. */
    CORE_DEMCR |= (1UL << 24);
    DWT_CYCCNT = 0U;
    DWT_CTRL |= 1UL;

    /*
     * CPU clock: 16 MHz.
     * 16000 CPU cycles produce one millisecond.
     */
    TIMEBASE_SYST_RVR = TIMEBASE_CYCLES_PER_MS - 1UL;
    TIMEBASE_SYST_CVR = 0U;

    /* Give SysTick the lowest configurable interrupt priority. */
    TIMEBASE_SYST_PRIORITY = 0xFFU;

    __asm volatile ("dsb" ::: "memory");
    __asm volatile ("isb" ::: "memory");

    /*
     * CLKSOURCE = 1: CPU clock.
     * TICKINT = 1: enable the SysTick interrupt.
     * ENABLE = 1: start counting.
     */
    TIMEBASE_SYST_CSR =
        (1UL << 2) |
        (1UL << 1) |
        (1UL << 0);
}

uint32_t timebase_millis(void)
{
    return coop_milliseconds;
}

uint32_t timebase_cycles(void)
{
    return DWT_CYCCNT;
}

void SysTick_Handler(void)
{
    /*
     * Only update time here.
     * Application tasks run from the main loop.
     */
    coop_milliseconds++;
}