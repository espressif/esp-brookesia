# Debug Commands

This example provides debug commands for analyzing and monitoring system performance.

## Table of Contents

- [Debug Commands](#debug-commands)
  - [Table of Contents](#table-of-contents)
  - [Memory Profiler](#memory-profiler)
  - [Thread Profiler](#thread-profiler)
  - [Time Profiler](#time-profiler)

## Memory Profiler

Print memory profiler information:

```bash
debug_mem
```

Example output:

```
==================== Memory Profiler Snapshot ====================
Timestamp: 2026-02-03 10:04:02
+-------------------+-------------+-------------+--------------+--------+
| Heap Type         |  Total (KB) |   Free (KB) | Largest (KB) | Used % |
+-------------------+-------------+-------------+--------------+--------+
| Internal (SRAM)   |         278 |         158 |           92 |    44% |
+-------------------+-------------+-------------+--------------+--------+
| External (PSRAM)  |       11061 |        9620 |         9472 |    14% |
+-------------------+-------------+-------------+--------------+--------+
| Total             |       11340 |        9778 |         9472 |    14% |
+-------------------+-------------+-------------+--------------+--------+
========================== Statistics ============================
+---------------------------+--------------------+
| Field                     |              Value |
+---------------------------+--------------------+
| Sample Count              |                 23 |
+---------------------------+--------------------+
| Min Inter Free            |         158.000 KB |
+---------------------------+--------------------+
| Min Inter Free Pct        |                56% |
+---------------------------+--------------------+
| Min Inter Largest Free    |          92.000 KB |
+---------------------------+--------------------+
| Min Exter Free            |        9619.000 KB |
+---------------------------+--------------------+
| Min Exter Free Pct        |                86% |
+---------------------------+--------------------+
| Min Exter Largest Free    |        9472.000 KB |
+---------------------------+--------------------+
| Min Total Free            |        9778.000 KB |
+---------------------------+--------------------+
| Min Total Free Pct        |                86% |
+---------------------------+--------------------+
| Min Total Largest Free    |        9472.000 KB |
+---------------------------+--------------------+
==================================================================
```

## Thread Profiler

Print thread profiler information:

```bash
debug_thread
```

Use custom sorting and sampling duration:

```bash
# Set primary sort (none|core, default: core)
debug_thread -p core

# Set secondary sort (cpu|priority|stack|name, default: cpu)
debug_thread -s cpu

# Set sampling duration in milliseconds (default: 1000)
debug_thread -d 2000

# Combined usage
debug_thread -p core -s cpu -d 2000
```

Parameter description:

| Param | Description | Options | Default |
| ----- | ----------- | ------- | ------- |
| `-p` | Primary sort | `none`, `core` | `core` |
| `-s` | Secondary sort | `cpu`, `priority`, `stack`, `name` | `cpu` |
| `-d` | Sampling duration (ms) | Any positive integer | `1000` |

Example output:

```
==================== Thread Profiler Snapshot ====================
Tasks: 13 (Running: 1, Blocked: 7, Suspended: 3)
Total CPU: 96%, Sampling Duration: 1048ms
+--------------------+--------+-------+----------+-------+-------+----------+-----------+
| Name               | CoreId |  CPU% | Priority |   HWM | Stack | Run Time | State     |
+--------------------+--------+-------+----------+-------+-------+----------+-----------+
| tiT                |     -1 |    0% |       18 |  2320 | Intr  |      149 | Blocked   |
+--------------------+--------+-------+----------+-------+-------+----------+-----------+
| console_repl       |     -1 |    0% |        2 |  8352 | Intr  |        0 | Blocked   |
+--------------------+--------+-------+----------+-------+-------+----------+-----------+
| Tmr Svc            |     -1 |    0% |        1 |  1160 | Intr  |       44 | Blocked   |
+--------------------+--------+-------+----------+-------+-------+----------+-----------+
| IDLE0              |      0 |   46% |        0 |   664 | Intr  |   981892 | Ready     |
+--------------------+--------+-------+----------+-------+-------+----------+-----------+
| ProfilerWorker     |      0 |    1% |        1 |  3440 | Extr  |    28916 | Running   |
+--------------------+--------+-------+----------+-------+-------+----------+-----------+
| SvcMgrWorker0      |      0 |    1% |        5 | 12664 | Extr  |    37930 | Blocked   |
+--------------------+--------+-------+----------+-------+-------+----------+-----------+
| esp_timer          |      0 |    0% |       22 |  3248 | Intr  |        0 | Suspended |
+--------------------+--------+-------+----------+-------+-------+----------+-----------+
| sys_evt            |      0 |    0% |       20 |  3656 | Intr  |        0 | Blocked   |
+--------------------+--------+-------+----------+-------+-------+----------+-----------+
| ipc0               |      0 |    0% |       24 |   432 | Intr  |        0 | Suspended |
+--------------------+--------+-------+----------+-------+-------+----------+-----------+
| IDLE1              |      1 |   47% |        0 |   672 | Intr  |   996040 | Ready     |
+--------------------+--------+-------+----------+-------+-------+----------+-----------+
| SvcMgrWorker1      |      1 |    1% |        5 | 12660 | Extr  |    38623 | Blocked   |
+--------------------+--------+-------+----------+-------+-------+----------+-----------+
| gfx_core           |      1 |    0% |        5 |  6872 | Extr  |    14407 | Blocked   |
+--------------------+--------+-------+----------+-------+-------+----------+-----------+
| ipc1               |      1 |    0% |       24 |   440 | Intr  |        0 | Suspended |
+--------------------+--------+-------+----------+-------+-------+----------+-----------+
==================================================================
```

## Time Profiler

Print time profiler report:

```bash
debug_time_report
```

Clear all time profiler data:

```bash
debug_time_clear
```

Example output:

```
=== Performance Tree Report ===
(Unit: ms)
Name                             |  calls |      total |       self |        avg |        min |        max | %parent |  %total
------------------------------------------------------------------------------------------------------------------------------
|- app_main                      |      1 |     922.31 |     922.31 |     922.31 |     922.31 |     922.31 |   92.67% |   92.67%
`- console_start                 |      1 |      72.93 |      72.93 |      72.93 |      72.93 |      72.93 |    7.33% |    7.33%
===============================
```

> [!NOTE]
> Time profiler data needs to be collected by explicitly calling interfaces in the code:
> - `BROOKESIA_TIME_PROFILER_SCOPE(name)`: Scope timing
> - `BROOKESIA_TIME_PROFILER_START_EVENT(name)`: Start timing
> - `BROOKESIA_TIME_PROFILER_END_EVENT(name)`: End timing
