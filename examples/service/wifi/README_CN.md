| 支持的芯片 | ESP32-S3 | ESP32-P4 |
| :--------: | :------: | :------: |
|            |    ✅     |    ✅     |

# WiFi 服务示例

[English Version](./README.md)

本示例演示了如何使用 ESP-Brookesia 的 WiFi 服务进行无线网络管理，涵盖事件订阅、STA 扫描、STA 连接与断开、自动重连、SoftAP 配网以及凭证重置等功能。

## 📑 目录

- [WiFi 服务示例](#wifi-服务示例)
  - [📑 目录](#-目录)
  - [✨ 功能特性](#-功能特性)
  - [🚩 快速入门](#-快速入门)
    - [硬件要求](#硬件要求)
    - [开发环境](#开发环境)
  - [🔨 编译烧录](#-编译烧录)
  - [🚀 快速体验](#-快速体验)
  - [📖 示例说明](#-示例说明)
    - [事件订阅演示](#事件订阅演示)
    - [STA 扫描演示](#sta-扫描演示)
    - [STA 连接演示](#sta-连接演示)
    - [自动重连演示](#自动重连演示)
    - [SoftAP 配网演示](#softap-配网演示)
    - [重置数据演示](#重置数据演示)
  - [🔍 故障排除](#-故障排除)
  - [💬 技术支持与反馈](#-技术支持与反馈)

## ✨ 功能特性

- 📡 **事件订阅**：通过 RAII 风格的订阅机制监听通用动作、状态变化、扫描结果及 SoftAP 等各类 WiFi 事件
- 🔍 **STA 扫描**：配置扫描参数（AP 数量、间隔、超时），自动周期扫描并通过事件获取周边 AP 列表
- 🔗 **STA 连接**：设置目标 AP（SSID/密码）并触发连接，通过事件等待连接或断开结果，支持状态验证
- 🔄 **自动重连**：重启 WiFi 后自动连接已保存的 AP 凭证，验证恢复连接
- 📶 **SoftAP 配网**：开启 SoftAP 热点，手机扫码或连接后通过配网页面填入家庭 WiFi 凭证，完成后自动停止热点
- 🗑️ **凭证重置**：清除 NVS 中保存的所有 WiFi 凭证，并验证重置结果

## 🚩 快速入门

### 硬件要求

搭载 `ESP32-S3` 或 `ESP32-P4` 芯片且 `Flash >= 4MB` 的开发板，具备 WiFi 功能。

### 开发环境

- ESP-IDF `v5.5.2` TAG（推荐）或 `release/v5.5` 分支

## 🔨 编译烧录

```bash
idf.py -p PORT build flash monitor
```

按 `Ctrl-]` 退出串口监视。

> [!NOTE]
> 如需演示 STA 连接与自动重连功能，请在 `main/main.cpp` 顶部取消注释并填写 WiFi 凭证：
>
> ```c
> #define EXAMPLE_WIFI_SSID "your_ssid"
> #define EXAMPLE_WIFI_PSW  "your_password"
> ```
>
> 未定义凭证时，STA 连接与自动重连演示将被跳过，仅运行扫描、SoftAP 配网与重置数据三个场景。

## 🚀 快速体验

固件烧录成功后，程序将自动按顺序执行以下演示，所有输出通过串口打印：

1. **事件订阅演示**：注册所有 WiFi 事件回调，后续演示中的事件均通过串口实时输出
2. **STA 扫描演示**：自动扫描周边 AP，串口打印每轮扫描到的 AP 列表（SSID、信号强度等）
3. **STA 连接演示**（需配置凭证）：连接指定 AP，验证连接成功后断开
4. **自动重连演示**（需配置凭证）：重启 WiFi，验证自动连接保存的 AP
5. **SoftAP 配网演示**：开启热点，等待手机连接并完成配网（超时 120 秒）
6. **重置数据演示**：清除所有已保存凭证，验证清除结果

## 📖 示例说明

### 事件订阅演示

通过 `WifiHelper::subscribe_event()` 注册以下 5 类事件，订阅连接使用 RAII 风格管理（存储在 `global_connections` 向量中，生命周期与 `app_main` 一致）：

| 事件 | 说明 |
|------|------|
| `GeneralActionTriggered` | 触发了 Init/Start/Stop/Connect/Disconnect 等通用动作时回调 |
| `GeneralEventHappened` | WiFi 状态发生变化（Started/Connected/Disconnected 等）时回调，携带是否为意外事件标志 |
| `ScanStateChanged` | 扫描任务启动或结束时回调，携带当前是否正在扫描的布尔值 |
| `ScanApInfosUpdated` | 每轮扫描完成、AP 列表更新时回调，携带 AP 信息数组 |
| `SoftApEventHappened` | SoftAP 热点状态变化（Started/Stopped 等）时回调 |

### STA 扫描演示

配置扫描参数并启动周期扫描，通过 `EventMonitor` 等待扫描完成：

```
ScanParams:
  ap_count = 10          # 每轮最多返回 AP 数量
  interval_ms = 10000    # 扫描轮次间隔
  timeout_ms  = 20000    # 总超时后自动停止
```

扫描完成后，读取最后一轮的扫描结果并通过串口打印完整 AP 列表（SSID、RSSI、加密方式等）。示例要求至少成功完成 1 轮、最多 2 轮扫描。

### STA 连接演示

> [!NOTE]
> 需在 `main.cpp` 中定义 `EXAMPLE_WIFI_SSID` 和 `EXAMPLE_WIFI_PSW` 才会执行。

流程：设置目标 AP → 触发连接 → 等待 `Connected` 事件（超时 10s）→ 验证已连接 AP 列表中包含目标 AP → 触发断开 → 等待 `Disconnected` 事件（超时 2s）→ 验证状态恢复为 `Started`。

### 自动重连演示

> [!NOTE]
> 需在 `main.cpp` 中定义 `EXAMPLE_WIFI_SSID` 和 `EXAMPLE_WIFI_PSW` 才会执行，且须在 STA 连接演示之后运行（凭证已保存）。

流程：停止 WiFi → 重新启动 → 等待 `Connected` 事件（超时 10s）→ 验证自动连接成功。

### SoftAP 配网演示

流程：

1. 若当前已连接则先触发断开
2. 调用 `TriggerSoftApProvisionStart`，等待 `SoftApEvent::Started` 事件
3. 读取并打印 SoftAP 的 SSID 与密码（串口提示：使用手机连接该热点）
4. 等待手机通过配网页面填入家庭 WiFi 凭证后的 `Connected` 事件（超时 120 秒）
5. 连接成功后打印已连接 AP 名称；超时则打印警告并继续
6. 调用 `TriggerSoftApProvisionStop`，等待 `SoftApEvent::Stopped` 事件

### 重置数据演示

调用 `ResetData` 清除 NVS 中存储的所有 WiFi 凭证（已连接 AP 历史记录），并验证调用后已连接 AP 列表为空。

## 🔍 故障排除

**STA 连接失败或超时**

- 确认 `EXAMPLE_WIFI_SSID` 和 `EXAMPLE_WIFI_PSW` 填写正确。
- 检查目标 AP 是否在信号覆盖范围内（扫描演示中应能看到该 SSID）。
- 查看串口日志中的 `GeneralEventHappened` 事件，确认是否触发了连接失败事件。

**SoftAP 配网超时**

- 确认手机已成功连接到示例打印的热点 SSID。
- 在浏览器中手动访问配网页面（通常为 `192.168.4.1`）。
- 超时（120 秒）后程序会继续执行后续演示，不影响整体流程。

**扫描无结果**

- 确认周围有可用的 WiFi AP。
- 检查串口日志中 `ScanApInfosUpdated` 事件是否触发；若未触发，检查 WiFi 是否成功启动。

## 💬 技术支持与反馈

请通过以下渠道进行反馈：

- 有关技术问题，请访问 [esp32.com](https://esp32.com/viewforum.php?f=52&sid=86e7d3b29eae6d591c965ec885874da6) 论坛
- 有关功能请求或错误报告，请创建新的 [GitHub 问题](https://github.com/espressif/esp-brookesia/issues)

我们会尽快回复。
