# Brookesia HAL Linux

`brookesia_hal_linux` 是 `brookesia_hal_interface` 的 Linux host 后端。
它替代旧 host HAL 组件，CMake alias 为
`brookesia::hal_linux`。

## 安装依赖

在当前目录执行：

```bash
./scripts/install_linux_deps.sh --full
```

可使用 `--dry-run` 预览安装命令。`--minimal` 只安装构建、Boost 和
OpenSSL 依赖。`--full` 还会安装 PC 测试 app 用于透明绝对路径挂载的
`proot` launcher。`--media`、`--video`、`--display`、`--network` 分别安装
FFmpeg/PortAudio/V4L2、仅视频所需 FFmpeg/V4L2、SDL2、NetworkManager/UPower
依赖组。

脚本支持 `apt`、`dnf` 和 `pacman`。其他发行版会打印依赖组，便于手动安装。

## 构建 Host Test

```bash
cmake -S host_test -B host_test/build -DBROOKESIA_HAL_LINUX_MEDIA_BACKEND=auto
cmake --build host_test/build
ctest --test-dir host_test/build --output-on-failure
```

如需尝试非确定性的真实后端 smoke test，需要显式打开并传入目标后端变量：

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

默认 host test 构建会强制使用 stub 后端，除非显式传入 backend 变量，以保证 CI
确定性。后端选项为：

- `BROOKESIA_HAL_LINUX_MEDIA_BACKEND=auto|stub|ffmpeg_portaudio`
- `BROOKESIA_HAL_LINUX_VIDEO_BACKEND=auto|stub|ffmpeg_v4l2`
- `BROOKESIA_HAL_LINUX_WIFI_BACKEND=auto|stub|networkmanager`
- `BROOKESIA_HAL_LINUX_DISPLAY_BACKEND=auto|stub|sdl2`
- `BROOKESIA_HAL_LINUX_POWER_BACKEND=auto|stub|upower`

在 host test 之外，`auto` 会优先选择依赖已安装的真实后端。运行时如果没有图形会话、
摄像头、`nmcli`、电池，或 NetworkManager 权限不足，会打印 warning 并降级到
deterministic stub 路径。

## 运行时数据

Linux storage 后端会持久化 KV 数据。默认路径按
以下优先级选择：

1. `BROOKESIA_HAL_LINUX_KV_DIR`，即使目录不存在也会自动创建
2. 当前可执行文件目录下的 `.brookesia/kv`
3. `$XDG_STATE_HOME/.brookesia/kv`
4. `$HOME/.brookesia/kv`
5. 系统临时目录

可通过 `BROOKESIA_HAL_LINUX_FS_ROOT` 覆盖 `/spiffs`、`/littlefs`、`/fatfs`
和 `/sdcard` 对应的本地文件系统根目录。Linux 后端会在 host 目录下按如下子目录
组织这些挂载。未设置 `BROOKESIA_HAL_LINUX_FS_ROOT` 时，默认使用同一默认系统目录下的
`.brookesia/fs`：

- `spiffs/` -> `/spiffs`
- `littlefs/` -> `/littlefs`
- `fatfs/` -> `/fatfs`
- `sdcard/` -> `/sdcard`

对于 PC 上的透明绝对路径访问，仓库内 test app 会通过生成的 `proot`
launcher 启动二进制。在这种受支持的运行方式下，
`std::filesystem("/littlefs/...")`、`ifstream("/sdcard/...")`
等访问方式会表现得和 ESP 目标一致。

## 后端说明

- HTTP 使用 Boost.Beast 和 OpenSSL。
- SNTP 使用 Linux host 网络栈。
- Storage 使用本地文件系统。
- Audio 在选择或自动探测到 `ffmpeg_portaudio` 时，使用 FFmpeg 和 PortAudio
  实现播放、采集、编码和解码。
- Video 在选择或自动探测到 `ffmpeg_v4l2` 时，使用 FFmpeg `libavdevice`/V4L2
  从 `/dev/video*` 采集，并使用 FFmpeg 解码 MJPEG/H264。
- Display 使用 SDL2 创建窗口、更新 bitmap、读取鼠标/触摸事件，并模拟背光亮度。
- Power 从 `/sys/class/power_supply` 读取真实电池和外接电源状态；真实路径不支持
  充电控制时会返回不支持。
- Wi-Fi 通过 NetworkManager `nmcli` 实现扫描、连接、断开和状态查询。SoftAP
  是 best-effort，设备或权限不支持 AP mode 时会失败并记录日志。
