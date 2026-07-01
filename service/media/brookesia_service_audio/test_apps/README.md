# Audio Service Test App

This test app supports both ESP-IDF/Unity and PC/Boost.Test builds.

## PC Build

The PC build forces the Linux HAL media backend to `ffmpeg_portaudio` and uses a
generated `proot` launcher for transparent `/littlefs` absolute paths. The
easiest dependency setup is:

```bash
hal/brookesia_hal_linux/scripts/install_linux_deps.sh --full
```

Run from the repository root:

```bash
cmake -S service/media/brookesia_service_audio/test_apps \
      -B /tmp/service_audio_pc \
      -DBROOKESIA_SERVICE_AUDIO_TEST_APPS_FORCE_PC=ON
cmake --build /tmp/service_audio_pc -j4
ctest --test-dir /tmp/service_audio_pc --output-on-failure
```

`BROOKESIA_SERVICE_AUDIO_TEST_APPS_FORCE_PC=ON` is recommended when `IDF_PATH` is exported, otherwise the
top-level CMake file selects the ESP-IDF build path.

If system Boost.Test is unavailable, point to an esp-boost source tree:

```bash
cmake -S service/media/brookesia_service_audio/test_apps \
      -B /tmp/service_audio_pc \
      -DBROOKESIA_SERVICE_AUDIO_TEST_APPS_FORCE_PC=ON \
      -DBROOKESIA_TEST_BOOST_ROOT=/path/to/esp-boost/src
```

## PC Run

The PC CMake path reuses the shared defaults exported by
`service/system/brookesia_service_storage/cmake/pc_platform.cmake`, so the audio test
app inherits the same default system, KV, and file-system root directory
layout.

Prefer `ctest`, which uses the generated `proot` launcher:

```bash
ctest --test-dir /tmp/service_audio_pc --output-on-failure
ctest --test-dir /tmp/service_audio_pc -V
```

For manual runs, use the generated launcher:

```bash
bash /tmp/service_audio_pc/run_test_brookesia_service_audio.sh
```

Run selected Boost.Test cases:

```bash
bash /tmp/service_audio_pc/run_test_brookesia_service_audio.sh --run_test=play_single_url --log_level=test_suite
bash /tmp/service_audio_pc/run_test_brookesia_service_audio.sh --run_test=play_control --log_level=test_suite
bash /tmp/service_audio_pc/run_test_brookesia_service_audio.sh --run_test=codec_pcm --log_level=test_suite
```

The PC path uses `brookesia_hal_linux` with the real FFmpeg/PortAudio media backend. CMake configuration fails if those
development packages are unavailable. Playback and codec tests use the system default PortAudio output/input devices,
and the generated launcher maps `/littlefs` to the local `littlefs/` fixture directory through
`BROOKESIA_HAL_LINUX_FS_ROOT` plus `proot` bind mounts.
Each PC test case prints a `[TEST] Running: ...` banner before execution so the current manual demo step is always
visible, even without extra Boost.Test verbosity.

## Manual Audition Notes

- Full-playback demo cases:
  `play_single_url`, `play_multiple_urls`
- Control / interrupt demo cases:
  `play_single_url_with_config`, `play_control`
- Record and replay codec demo cases:
  `codec_pcm`, `codec_opus`, `codec_g711a`

`play_single_url` and `play_control` use `example.mp3`, so they are better for speaker audition than the short digit
fixtures. The codec demo captures a short microphone window, prints packet and byte statistics, then replays the
captured audio.

PC audio backends may print ALSA/JACK probing warnings while PortAudio searches for a working device. Those warnings do
not indicate failure by themselves; real failure is typically accompanied by `Failed to open PortAudio ...` logs.

Running the bare `test_brookesia_service_audio` binary is still useful for low-level debugging, but it does not
guarantee transparent `/littlefs/...` absolute-path access.

## ESP-IDF Build

Run from the repository root:

```bash
. /path/to/esp-idf/export.sh
idf.py -C service/media/brookesia_service_audio/test_apps set-target esp32s3
idf.py -C service/media/brookesia_service_audio/test_apps build
```

## ESP-IDF Pytest

After building/flashing in a CI-compatible environment:

```bash
pytest service/media/brookesia_service_audio/test_apps --target esp32s3 --env generic,octal-psram
```
