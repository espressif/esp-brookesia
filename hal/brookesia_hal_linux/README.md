# Brookesia HAL Linux

`brookesia_hal_linux` is the Linux host backend for `brookesia_hal_interface`.
It replaces the previous host HAL component and exports the CMake
alias `brookesia::hal_linux`.

## Install Dependencies

Run the helper script from this directory:

```bash
./scripts/install_linux_deps.sh --full
```

Use `--dry-run` to preview the package command. `--minimal` installs only the
build, Boost, and OpenSSL dependencies. `--full` also installs the `proot`
launcher helper used by the PC test apps for transparent absolute mount paths.
`--media`, `--video`, `--display`, and `--network` install the optional
dependency groups for FFmpeg/PortAudio/V4L2, FFmpeg/V4L2-only video, SDL2, and
NetworkManager/UPower.

The script supports `apt`, `dnf`, and `pacman`. Other distributions print the
dependency groups so they can be installed manually.

## Build Host Test

```bash
cmake -S host_test -B host_test/build -DBROOKESIA_HAL_LINUX_MEDIA_BACKEND=auto
cmake --build host_test/build
ctest --test-dir host_test/build --output-on-failure
```

To try the non-deterministic real backend smoke test, opt in explicitly and pass
the desired backend variables:

```bash
cmake -S host_test -B host_test/build-real \
  -DBROOKESIA_HAL_LINUX_HOST_TEST_ENABLE_REAL_SMOKE=ON \
  -DBROOKESIA_HAL_LINUX_DISPLAY_BACKEND=sdl2 \
  -DBROOKESIA_HAL_LINUX_POWER_BACKEND=upower \
  -DBROOKESIA_HAL_LINUX_WIFI_BACKEND=networkmanager \
  -DBROOKESIA_HAL_LINUX_VIDEO_BACKEND=ffmpeg_v4l2
cmake --build host_test/build-real
ctest --test-dir host_test/build-real --output-on-failure
```

The default host test build forces stub backends unless you explicitly pass a
backend variable. This keeps CI deterministic. The backend options accept:

- `BROOKESIA_HAL_LINUX_MEDIA_BACKEND=auto|stub|ffmpeg_portaudio`
- `BROOKESIA_HAL_LINUX_VIDEO_BACKEND=auto|stub|ffmpeg_v4l2`
- `BROOKESIA_HAL_LINUX_WIFI_BACKEND=auto|stub|networkmanager`
- `BROOKESIA_HAL_LINUX_DISPLAY_BACKEND=auto|stub|sdl2`
- `BROOKESIA_HAL_LINUX_POWER_BACKEND=auto|stub|upower`

Outside `host_test`, `auto` prefers the real backend when the optional
development package is present. Runtime failures such as no display session, no
camera, missing `nmcli`, no battery, or insufficient NetworkManager permissions
fall back to the deterministic stub path with a warning log.

## Runtime Data

The Linux storage backend persists KV data under:

1. `BROOKESIA_HAL_LINUX_KV_DIR`, when set. Missing directories are created automatically.
2. The current executable directory under `.brookesia/kv`.
3. `$XDG_STATE_HOME/.brookesia/kv`, when set.
4. `$HOME/.brookesia/kv`.
5. The system temporary directory as a final fallback.

Set `BROOKESIA_HAL_LINUX_FS_ROOT` to override the root used for the exposed
`/spiffs`, `/littlefs`, `/fatfs`, and `/sdcard` file-system entries. The Linux
backend models those mounts as subdirectories under the host root. When
`BROOKESIA_HAL_LINUX_FS_ROOT` is unset, the default root is `.brookesia/fs`
under the same default system directory:

- `spiffs/` -> `/spiffs`
- `littlefs/` -> `/littlefs`
- `fatfs/` -> `/fatfs`
- `sdcard/` -> `/sdcard`

For transparent absolute-path access on PC, the repository test apps run their
binaries through a generated `proot` launcher. Within that supported run mode,
`std::filesystem("/littlefs/...")`, `ifstream("/sdcard/...")`, and similar
absolute-path access behave like the ESP target layout.

## Backend Notes

- HTTP uses Boost.Beast and OpenSSL.
- SNTP uses the Linux host network stack.
- Storage uses the local file system.
- Audio uses FFmpeg and PortAudio for playback, capture, encode, and decode
  when `ffmpeg_portaudio` is selected or auto-detected.
- Video uses FFmpeg `libavdevice`/V4L2 for `/dev/video*` capture and FFmpeg
  decode for MJPEG/H264 when `ffmpeg_v4l2` is selected or auto-detected.
- Display uses SDL2 for a real window, bitmap updates, mouse/touch sampling, and
  backlight brightness simulation.
- Power reads Linux power-supply state from `/sys/class/power_supply`; charger
  control is reported unsupported on the real path.
- Wi-Fi uses NetworkManager via `nmcli` for scan/connect/disconnect/status.
  SoftAP is best-effort and may fail when the Wi-Fi device or permissions do not
  support AP mode.
