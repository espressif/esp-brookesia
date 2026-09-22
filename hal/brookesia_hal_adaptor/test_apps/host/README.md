# HAL Host Regression Tests

This directory provides one entry point for HAL lifecycle, board cleanup, and dependency patch regressions. It compiles production sources with hardware test doubles on Linux; it does not flash a board.

## Requirements

- Python 3.9+, a C compiler, and a C++23 compiler (GCC 13 or later).
- Boost headers, Boost.Thread, and Boost.System development libraries for the core tests.
- Git, CMake, and an existing configured project's `managed_components` directory for dependency tests.

The runner uses `CXX` when set, otherwise `g++-13` or `c++`. Run these commands from the repository root.

## Core Regressions

```bash
python3 hal/brookesia_hal_adaptor/test_apps/host/run.py --suite core
```

This is the default suite for developers to run locally; it is not connected to CI. It covers ordered expansion events, callback removal, camera/encoder concurrency, failed cleanup ownership, and NAND/BQ27220/USB initialization rollback.

It also exercises the real Board Manager facade with arbitrary cleanup names, error/retry dispatch without allocation, and Mosaico startup registration retained from a static archive. Registration failures are checked under all three error-handling policies, with assertions both enabled and disabled.

## Dependency Regressions

Configure the firmware project normally so its dependencies and required patches are available, then run:

```bash
python3 hal/brookesia_hal_adaptor/test_apps/host/run.py --suite all \
    --managed-components examples/system/super/managed_components
```

Use `--suite dependencies` to run only the patch tests. They cover patch application, component aliases, repeated configuration, release errors, frame-mode stop, and UYVY/YUYV negotiation. Dependencies are supplied explicitly; the runner never downloads them or changes the project copies.

These cases were validated with Board Manager 0.5.15, esp_video 2.4.1, esp_capture 0.8.4, and av_processor 0.6.6. When upgrading Board Manager, run against the candidate project's resolved dependencies, review any patch/API differences, then rerun the core suite and hardware tests. Compilation failures are compatibility findings, not a reason to skip a case.

## Scope and Cleanup

Tests build into temporary directories and remove their generated sources and executables on exit. A failed compile or case produces a nonzero exit code. The core suite runs without ESP-IDF; dependency tests require the explicitly supplied component sources.

The tests simulate hardware failures and controlled thread interleavings. They do not validate sensor registers, display DMA timing, USB enumeration, or physical hot-plug behavior. Use the existing HAL test application and System Super for board verification; the Video Service test application retains its preview-format regressions.
