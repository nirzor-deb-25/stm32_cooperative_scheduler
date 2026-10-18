# Minimal Cooperative Scheduler on STM32

A small cooperative scheduler written from scratch in C for the STM32F407G-DISC1.

The scheduler runs periodic callbacks from the main loop. SysTick maintains a millisecond clock, while the Cortex-M4 cycle counter measures callback duration.

The project uses direct register access without STM32 HAL or FreeRTOS.

## Hardware and tools

- STM32F407G-DISC1 with onboard ST-LINK.
- Windows PowerShell.
- GNU Arm compiler and GNU Make.
- STM32CubeProgrammer CLI.
- Git.

No external wiring is required.

## How the scheduler works

The scheduler uses a fixed table with space for eight tasks.

Each task has:

- A callback and context pointer.
- A period in milliseconds.
- A next-release time.
- Execution and timing diagnostics.

scheduler_dispatch() scans tasks in registration order. It refreshes the current time before checking each task and executes each due callback at most once per scan.

Callbacks execute sequentially on the main stack. There are no separate task stacks, task priorities, or application-task context switches.

Interrupts can interrupt a callback, but another application callback cannot run until the current callback returns.

## Release policy

After a callback finishes, the scheduler chooses the next release on that task's original periodic timeline.

Releases reached while the task was late or executing are counted as skipped. The next release is strictly after the callback's finish time.

This avoids a burst of catch-up calls.

For example, a task with releases at 100, 200, 300, and 400 ms that first runs at 350 ms executes once, records two skipped releases, and schedules its next release at 400 ms.

A skipped release is not an execution. Start lateness measures how late the executed callback began relative to the release assigned to that invocation.

## Clock and timing

- CPU: reset-default 16 MHz HSI.
- SysTick: 1 ms interrupt.
- SysTick ISR: increments the millisecond counter only.
- DWT cycle counter: measures callback duration.

Callback duration includes interrupt time occurring between its start and finish measurements.

The main loop continuously polls the scheduler. Low-power idle behaviour is not implemented.

## Demonstration tasks

| ID | Callback | Period | Behaviour |
|---|---|---:|---|
| 0 | Green LED | 500 ms | Toggle LD4 on PD12 |
| 1 | Blue LED | 125 ms | Toggle LD6 on PD15 |
| 2 | Workload | 1000 ms | Simulate a configurable CPU workload |

With no workload, green completes one blink cycle per second and blue completes four cycles per second.

coop_work_ms controls the simulated workload. It defaults to zero and is bounded to 2000 ms.

The workload deliberately busy-waits with interrupts enabled. This demonstrates blocking between cooperative callbacks; it is not a recommended application delay mechanism.

## Scheduler API

- scheduler_init(): clear the task table before registration.
- scheduler_add(): register a periodic callback.
- scheduler_dispatch(): scan and execute due callbacks.

Registration rejects:

- A null callback.
- A zero period.
- A period of 0x80000000 ms or greater.
- An initial delay of 0x80000000 ms or greater.
- A full task table.
- Registration attempted while dispatch is active.

Recursive dispatch returns without executing callbacks.

Initialization and registration are startup operations. Do not call them from interrupts or modify the task table while the main loop dispatches tasks.

## Counter wraparound

Deadline comparisons use unsigned subtraction.

Periods and initial delays must remain below half the 32-bit millisecond counter range. Dispatch must occur often enough that deadline lateness remains below that half-range, approximately 24.9 days.

Individual callback durations must also remain below the range of the 32-bit CPU cycle counter for unambiguous duration measurements: approximately 268 seconds at 16 MHz.

The demonstration workload is limited to two seconds.

## Startup self-tests

Startup executes the actual scheduler implementation against a controlled millisecond counter before enabling SysTick.

The tests cover:

1. Polling before and exactly at a release.
2. Preventing duplicate execution of the same release.
3. Late dispatch and skipped releases.
4. Time advancing during a long callback.
5. A future release across millisecond-counter wraparound.
6. Counter wraparound during callback execution.
7. Invalid registration arguments.
8. Task-table capacity and assigned IDs.

The recorded result was:

    status = 2
    checks_passed = 54
    failure_line = 0

Status values:

| Value | Meaning |
|---:|---|
| 0 | Not run |
| 1 | Running |
| 2 | Passed |
| 3 | Failed |

Tests temporarily control the real timebase counter while interrupts are disabled. Their registrations are cleared afterward, and the real application starts with a fresh timebase.

The tests simulate elapsed milliseconds. They do not validate physical SysTick timing or real callback cycle duration.

## Build and flash

From the project root in PowerShell:

    . .\setup.ps1
    make -j4 --output-sync=target
    make flash

The Makefile selects ST-LINK serial number 0671FF495170535067222342. Override STLINK_SN for another board.

Output files:

    build/stm32_cooperative_scheduler.elf
    build/stm32_cooperative_scheduler.bin
    build/stm32_cooperative_scheduler.hex

Clean build:

    make clean
    make -j4 --output-sync=target

No RTOS submodule or external software library download is required.

## Find diagnostic addresses

Use the ELF matching the firmware installed on the board:

    arm-none-eabi-nm -n .\build\stm32_cooperative_scheduler.elf |
        Select-String 'coop_work_ms$|coop_failure_code$|coop_stage$|scheduler_diagnostics$|scheduler_selftest_diagnostics$'

The recorded build used:

| Symbol | Address |
|---|---|
| coop_work_ms | 0x2000001C |
| coop_failure_code | 0x20000020 |
| coop_stage | 0x20000024 |
| scheduler_diagnostics | 0x20000028 |
| scheduler_selftest_diagnostics | 0x200001B8 |

Addresses may change when source or build settings change.

## Diagnostic layout

Each task record contains eight consecutive 32-bit words:

| Offset | Field |
|---:|---|
| 0 | enabled |
| 4 | period_ms |
| 8 | next_release_ms |
| 12 | run_count |
| 16 | skipped_releases |
| 20 | max_lateness_ms |
| 24 | max_duration_cycles |
| 28 | last_duration_cycles |

The enabled field marks a registered task. Runtime enable/disable control is not implemented.

Live debugger reads are not atomic snapshots of all diagnostic fields.

## Reproduce the workload experiment

These commands use addresses from the recorded build.

Read all three task records:

    STM32_Programmer_CLI.exe -c port=SWD mode=HOTPLUG sn=0671FF495170535067222342 -r32 0x20000028 96

Set the workload to 700 ms:

    STM32_Programmer_CLI.exe -c port=SWD mode=HOTPLUG sn=0671FF495170535067222342 -w32 0x2000001C 0x000002BC

Wait at least five seconds, then compare two diagnostic reads.

Restore zero workload:

    STM32_Programmer_CLI.exe -c port=SWD mode=HOTPLUG sn=0671FF495170535067222342 -w32 0x2000001C 0x00000000

Wait at least two seconds for any existing invocation to finish, then compare two further reads.

Reset explicitly through ST-LINK:

    STM32_Programmer_CLI.exe -c port=SWD mode=UR sn=0671FF495170535067222342 -rst

Read startup self-test results:

    STM32_Programmer_CLI.exe -c port=SWD mode=HOTPLUG sn=0671FF495170535067222342 -r32 0x200001B8 12

Expected self-test words:

    00000002 00000036 00000000

## Recorded hardware results

### Normal scheduling

| Task | First run count | Second run count | Skipped releases | Maximum lateness |
|---|---:|---:|---:|---:|
| Green | 127 | 133 | 0 | 0 ms |
| Blue | 509 | 535 | 0 | 0 ms |
| Workload | 63 | 66 | 0 | 0 ms |

Application stage was 3 and the failure code was zero.

### Workload enabled

| Diagnostic | First read | Second read |
|---|---:|---:|
| Green skipped releases | 13 | 16 |
| Blue skipped releases | 59 | 72 |
| Green maximum lateness | 700 ms | 700 ms |
| Blue maximum lateness | 700 ms | 700 ms |

Maximum workload duration was 11200074 cycles, approximately 700 ms at the configured CPU frequency.

These workload readings came from a run whose counters were lower than the earlier baseline. They should not be treated as one uninterrupted timeline with that baseline.

### Recovery without reset

| Diagnostic | First read | Second read |
|---|---:|---:|
| Green run count | 129 | 144 |
| Blue run count | 492 | 550 |
| Green skipped releases | 53 | 53 |
| Blue skipped releases | 239 | 239 |
| Workload last duration | 73 cycles | 73 cycles |

Callbacks continued executing while skipped-release totals stopped increasing. Historical maxima remained recorded.

### Reset through ST-LINK

After reset:

- Workload setting was zero.
- Failure code was zero.
- Application stage was 3.
- Green, blue, and workload run counts were 31, 125, and 15 at the recorded read.
- Skipped releases and maximum lateness were zero.

After startup tests were added, all 54 checks passed and the application again reached stage 3 without a failure.

The workload and recovery experiments were recorded before adding the startup test harness. The scheduler implementation remained unchanged.

Read spacing includes command execution and user interaction. These observations are not precision frequency or throughput benchmarks.

## Failure indicators

Red LD5 indicates an application setup failure.

| Failure code | Meaning |
|---:|---|
| 1 | Green task registration failed |
| 2 | Blue task registration failed |
| 3 | Workload task registration failed |
| 4 | Startup self-test failed |

For code 4, inspect scheduler_selftest_diagnostics.failure_line.

This indicator does not implement a general HardFault capture mechanism.

## Scope and limitations

- Fixed registration order, without dynamic task priorities.
- Maximum eight tasks.
- Shared main stack.
- No task preemption by other callbacks.
- No heap allocation in scheduler code.
- No sleep-based idle loop.
- No task removal or runtime enable/disable API.
- Slow or non-returning callbacks delay all following callbacks.
- Long interrupt masking can cause SysTick time to undercount.
- Millisecond lateness has 1 ms resolution.
- Callback duration includes interrupt and measurement overhead.
- No external instrument verification of absolute timing.
- No exhaustive long-duration or fault-injection testing.
- Registration-order and recursive-dispatch behaviour are implemented but not directly covered by the startup assertions.

This is a learning and portfolio implementation of cooperative scheduling.

## References

- STM32F407G-DISC1: https://www.st.com/en/evaluation-tools/stm32f407g-disc1.html
- STM32F407 documentation: https://www.st.com/en/microcontrollers-microprocessors/stm32f407-417/documentation.html
- STM32CubeProgrammer: https://www.st.com/en/development-tools/stm32cubeprog.html
