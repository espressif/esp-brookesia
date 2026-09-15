# Brookesia USB CLI

`brookesia-usb` 通过单一串行传输控制 ESP-Brookesia USB service：USB Serial/JTAG 的 CDC-ACM 通道，或面向只引出 USB 转串口桥的板子的 UART。协议版本为 `1`。

## 安装

```bash
python -m pip install brookesia-usb
```

安装会自动拉取 `pyserial`。查看已安装版本：

```bash
brookesia-usb --version
```

## 端口选择

CLI 会自动发现端口并用 `hello` 命令验证设备。它优先 USB Serial/JTAG，当不存在 Serial/JTAG 端口时回退到 USB 转 UART 桥：

1. 若存在**恰好一个** USB Serial/JTAG 候选（Espressif VID:PID `0x303A:0x1001` 或端口描述匹配），直接使用该端口，不在发现阶段探测；`hello` 在命令执行时只做一次。
2. 若存在**多个** USB Serial/JTAG 候选，逐个用 `hello` 探测，直到有一个回报 `serial_jtag` 传输。
3. 若无 USB Serial/JTAG 候选，或其候选全部探测失败，则探测 USB 转 UART 候选（常见桥接芯片 VID:PID、`/dev/ttyUSB*` 或非 Serial/JTAG 的 `/dev/ttyACM*`），接受 `serial_jtag` 或 `uart` 传输。

若只存在一个 USB Serial/JTAG 端口但它并非目标设备（例如 USJ 线缆和 USB 转 UART 桥同时插着），CLI 会选中它并报错；请用 `--port` 显式指定实际设备所在的端口。

```bash
brookesia-usb devices
brookesia-usb status
brookesia-usb --port /dev/ttyACM0 status
brookesia-usb --port /dev/ttyUSB0 --baudrate 921600 status
```

- `--port`：Serial/JTAG（`/dev/ttyACM*`）或 USB 转 UART（`/dev/ttyUSB*`）路径。省略时自动发现。
- `--baudrate`：USB Serial/JTAG 不使用物理波特率，该参数被忽略；UART 传输时必须与设备控制台波特率一致（例如 `CONFIG_ESP_CONSOLE_UART_BAUDRATE`）。默认 `115200`。
- `--timeout`：每次读写的超时时间（秒）。默认 `10`。

存在多个匹配设备时，请用 `devices` 输出的路径显式指定 `--port`。单个 Serial/JTAG CDC 配置通常只暴露 `/dev/ttyACM0`；不存在 `/dev/ttyACM1` 是正常的。

打开 USB 转 UART 端口会复位设备，因此 CLI 会重试 `hello` 直到服务就绪。执行命令前请关闭 `idf.py monitor`、minicom 或其他读取同一端口的程序。

## 控制会话

需要控制会话的命令会发送 `hello`，校验协议版本 `1`、设备回报的传输类型（`serial_jtag` 或 `uart`）和 `exclusive` 会话状态，并在退出时发送 `goodbye`。会话期间设备日志被抑制，避免干扰 JSON 响应或文件帧。

## 常用命令

### 列出设备

列出所有串口设备并识别 Serial/JTAG 候选：

```bash
brookesia-usb devices
```

该命令不建立控制会话；找不到串口设备时返回非零状态。

### 查询服务状态

查询 USB service、传输连接、会话状态和活动传输：

```bash
brookesia-usb status
brookesia-usb --port /dev/ttyACM0 status
```

### 调用服务函数

调用任意已注册的 Brookesia service 函数，参数必须是符合服务 JSON schema 的对象。可用 `Manager` 发现服务与函数：

```bash
brookesia-usb call Manager GetServiceNames '{}'
brookesia-usb call Manager GetServiceSchema '{"Name":"Storage"}'
brookesia-usb call SystemCore GetSystemInfo '{}'
brookesia-usb call SystemCore GetStorageLayout '{}'
brookesia-usb call Storage FSStat '{"Path":"/littlefs"}'
brookesia-usb call Storage FSList '{"Path":"/littlefs"}'
```

JSON 参数必须是对象，参数名和类型必须匹配函数 schema。ServiceManager 会在设备侧校验必选参数、默认值、未知参数和参数类型。需要 `RawBuffer` 参数的函数不能通过 JSON 接口调用；文件与软件包数据请使用 `put` 或 `install`。调用 `Usb` service 自身会被拒绝，以避免递归进入活动控制会话。

### 上传文件

上传本地文件到设备配置的上传根目录下的相对路径（默认 `/littlefs/usb`）：

```bash
brookesia-usb put ./logs/session.bin logs/session.bin
```

CLI 计算文件大小和 SHA-256 摘要，通过 CRC 保护的 16 KiB 或更小的帧传输，并在每个数据帧后等待 ACK。绝对路径、`..` 路径分量、符号链接逃逸和上传根目录外的路径都会被拒绝。默认不覆盖已有文件；显式传入 `--overwrite` 才会替换：

```bash
brookesia-usb put ./config/device.json config/device.json --overwrite
```

设备接受的最大文件大小默认为 8 MiB。传输先写入临时文件，仅在大小和 SHA-256 校验通过后才提交到目标路径。

### 安装 BPK 应用

发送完整的 BPK 运行时应用包到设备进行校验和安装：

```bash
brookesia-usb install ./build/my_app.bpk
```

软件包先暂存到 USB 临时目录，再由 system_core 桥执行 manifest 校验、ZIP 路径安全检查、暂存、替换和回滚。主机不能直接选择应用安装目录。连接中断或结果不明确时，CLI 不会自动重试安装；重试前请先检查设备状态。

### 终止传输

按 request ID 终止活动传输：

```bash
brookesia-usb abort 42
```

`abort` 是紧急命令，不建立新的控制会话。request `42` 活动时设备会删除临时文件并返回 `aborted`；未知 request ID 返回设备错误和非零状态。

## 错误与退出状态

CLI 仅在设备操作成功后返回 `0`；传输、协议、校验或设备错误返回 `1`。常见错误码：

- `invalid_command`：JSON 格式错误或操作不支持；
- `busy`：已有控制会话或传输正在进行；
- `bad_frame`：帧 CRC、类型或序列错误；
- `size_mismatch` / `hash_mismatch`：声明的元数据与实际数据不一致；
- `path_denied`：路径不安全或未显式允许覆盖；
- `storage_full`：无法创建或写入临时存储；
- `install_failed`：system_core 拒绝或未能安装软件包；
- `timeout`：超时时间内无主机活动；
- `aborted`：主机或设备取消了传输。

找不到设备时，先检查 Serial/JTAG Type-C 数据线并列出可用设备：

```bash
ls /dev/ttyACM*
brookesia-usb devices
```

运行控制命令前请关闭 `idf.py monitor`、minicom 或其他读取 `/dev/ttyACM0` 的程序；Serial-JTAG 只有一个共享 CDC 通道，无法安全地多路复用多个读取方。不存在 `/dev/ttyACM1` 是正常的。

## 开发

从源码安装并带上测试依赖，然后运行 lint 和单元测试：

```bash
python -m pip install -e "tools/brookesia_usb[test]"
python -m flake8 --config=.flake8 tools/brookesia_usb/src tools/brookesia_usb/tests
python -m pytest tools/brookesia_usb/tests
```

## 发布

软件包通过 `twine` 手动发布。

1. 修改 `tools/brookesia_usb/pyproject.toml` 中的 `version`。
2. 构建并校验发行包：

   ```bash
   python -m pip install --upgrade build twine
   python -m build tools/brookesia_usb
   python -m twine check tools/brookesia_usb/dist/*
   ```

3. 使用 PyPI API token（或交互式提示）上传：

   ```bash
   python -m twine upload tools/brookesia_usb/dist/*
   ```

4. 打发布标签，例如 `usb-cli-v0.2.0`。
