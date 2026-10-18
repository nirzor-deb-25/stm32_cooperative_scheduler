#ifndef TIMEBASE_H
#define TIMEBASE_H

#include <stdint.h>

#define TIMEBASE_CPU_HZ 16000000UL
#define TIMEBASE_CYCLES_PER_MS (TIMEBASE_CPU_HZ / 1000UL)

/* Call once during startup, before running the scheduler. */
void timebase_init(void);

/* Millisecond counter; wraps after approximately 49.7 days. */
uint32_t timebase_millis(void);

/* CPU cycle counter for measuring short execution intervals. */
uint32_t timebase_cycles(void);

#endif