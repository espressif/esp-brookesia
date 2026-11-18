# HAL Host Regression Tests

This directory provides one entry point for HAL lifecycle, board cleanup, and dependency patch regressions. It compiles production sources with hardware test doubles on Linux; it does not flash a board.

## Requirements

- Python 3.9+, a C compiler, and a C++23 compiler (GCC 13 or later).
- Boost headers, Boost.Thread, and Boost.System development libraries for the core tests.
- Dependency tests additionally require Git, CMake 3.22+, PyYAML, an existing configured project's `managed_components` directory, and a `c++` on `PATH` with C++20 support. These extra requirements do not apply to the core suite.

The core suite uses `CXX` when set, otherwise `g++-13` or `c++`. The dependency audio tests call `c++` directly and do not use `CXX`. Run these commands from the repository root.

## Core Regressions

```bash
python3 hal/brookesia_hal_adaptor/test_apps/host/run.py --suite core
```

This is the default suite for developers to run locally; it is not connected to CI. It covers ordered expansion events, callback removal, camera/encoder concurrency, failed cleanup ownership, and NAND/BQ27220/USB initialization rollback.

It also exercises the real Board Manager facade with arbitrary cleanup names, error/retry dispatch without allocation, and Mosaico startup registration retained from a static archive. Registration failures are checked under all three error-handling policies, with assertions both enabled and disabled.

## Dependency Regressions

Configure [System Super](../../../../examples/system/super/README.md#-how-to-use) normally so its resolved dependencies and required patches are available under `examples/system/super/build/dependencies/managed_components`. From the repository root, prepare an isolated Python environment and run only the dependency suite:

```bash
python3 -m venv /tmp/brookesia-hal-host-venv
source /tmp/brookesia-hal-host-venv/bin/activate
python3 -m pip install PyYAML "cmake>=3.22"
python3 hal/brookesia_hal_adaptor/test_apps/host/run.py --suite dependencies \
    --managed-components examples/system/super/build/dependencies/managed_components
deactivate
```

Use `--suite all` instead when the core-suite prerequisites are also available. The dependency tests check playback/recording formats, playback priority, callbacks, and open-failure rollback against real av_processor headers, as well as patch application, component aliases, repeated configuration, release errors, frame-mode stop, and UYVY/YUYV negotiation. Dependencies are supplied explicitly; the runner never downloads them or changes the project copies.

The shared-I2S cases execute the resolved codec library's pairing, format checks, `set_fmt`, and `esp_codec_dev_open` functions with driver/OS and analogue codec stubs. They cover 16/48 kHz, default 16/16 kHz, and channel and bit-depth mismatches, and verify that driver formats agree with the manager metadata. They do not validate physical I2S clocks or audio quality.

Run the dependency cases with Board Manager 0.5.15, esp_video 2.4.1, esp_capture 1.0.2, and av_processor 0.7.0. When upgrading Board Manager, run them against the candidate project's resolved dependencies, review patch and API differences, then run the core suite and hardware tests.

The dependency suite also executes the production HAL CMake registration and the official ESP-DL generator for local/managed aliases, P4/S3/S31/C5, additional model requirement unions, and the SR version boundary. These checks do not validate inference output, audio quality, or hardware latency.

## Scope and Cleanup

Tests build into temporary directories and remove their generated sources and executables on exit. A failed compile or case produces a nonzero exit code. The core suite runs without ESP-IDF; dependency tests require the explicitly supplied component sources.

The tests simulate hardware failures and controlled thread interleavings. They do not validate sensor registers, display DMA timing, USB enumeration, or physical hot-plug behavior. Use the existing HAL test application and System Super for board verification; the Video Service test application retains its preview-format regressions.
