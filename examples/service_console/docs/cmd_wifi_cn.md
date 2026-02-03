# WiFi 服务

WiFi 服务提供 WiFi 连接、扫描和管理功能，支持 AP 扫描、连接、断开等操作。

详细的接口说明请参考 [WiFi 服务帮助头文件](https://github.com/espressif/esp-brookesia/blob/master/service/brookesia_service_helper/include/brookesia/service_helper/wifi.hpp)。

## 目录

- [WiFi 服务](#wifi-服务)
  - [目录](#目录)
  - [函数调用接口](#函数调用接口)
    - [启动/停止 WiFi](#启动停止-wifi)
    - [扫描 AP](#扫描-ap)
    - [连接/断开 AP](#连接断开-ap)
    - [获取已连接的 AP](#获取已连接的-ap)
    - [重置数据](#重置数据)
  - [事件订阅接口](#事件订阅接口)
    - [通用操作触发事件](#通用操作触发事件)
    - [通用事件](#通用事件)
    - [扫描结果更新事件](#扫描结果更新事件)

## 函数调用接口

### 启动/停止 WiFi

启动 WiFi：

```bash
svc_call Wifi TriggerGeneralAction {"Action":"Start"}
```

停止 WiFi：

```bash
svc_call Wifi TriggerGeneralAction {"Action":"Stop"}
```

### 扫描 AP

设置扫描参数：

```bash
svc_call Wifi SetScanParams {"ApCount":10,"IntervalMs":1000,"TimeoutMs":30000}
```

参数说明：

- `ApCount`：扫描的 AP 数量上限
- `IntervalMs`：扫描间隔（毫秒）
- `TimeoutMs`：扫描超时时间（毫秒）

开始扫描：

```bash
svc_call Wifi TriggerScanStart {}
```

停止扫描：

```bash
svc_call Wifi TriggerScanStop {}
```

### 连接/断开 AP

设置要连接的 AP 信息：

```bash
svc_call Wifi SetConnectAp {"SSID":"your_ssid","Password":"your_password"}
```

参数说明：

- `SSID`：WiFi 名称
- `Password`：WiFi 密码

连接到 AP：

```bash
svc_call Wifi TriggerGeneralAction {"Action":"Connect"}
```

断开连接：

```bash
svc_call Wifi TriggerGeneralAction {"Action":"Disconnect"}
```

> [!NOTE]
> 连接成功后，会自动保存 AP 信息到 NVS 中，下次启动时会自动连接到上次连接的 AP。
> 如果连接断开后，会自动尝试连接之前连接的 AP。

### 获取已连接的 AP

查看当前连接的 AP 信息：

```bash
svc_call Wifi GetConnectedAps
```

### 重置数据

重置 WiFi 服务数据（清除保存的 AP 信息）：

```bash
svc_call Wifi ResetData
```

## 事件订阅接口

### 通用操作触发事件

订阅 WiFi 通用操作触发事件（如 Start、Stop、Connect、Disconnect 操作被触发时）：

```bash
svc_subscribe Wifi GeneralActionTriggered
```

事件参数：

- `Action`：触发的操作类型，可选值为 `Start`、`Stop`、`Connect`、`Disconnect`

### 通用事件

订阅 WiFi 通用事件（如连接状态变化）：

```bash
svc_subscribe Wifi GeneralEventHappened
```

事件参数：

- `Event`：事件类型，可选值为
  - `Started`：WiFi 服务已启动
  - `Stopped`：WiFi 服务已停止
  - `Connected`：已连接到 AP
  - `Disconnected`：已断开连接
  - `GotIp`：已获取 IP 地址
  - `LostIp`：已丢失 IP 地址

### 扫描结果更新事件

订阅 WiFi 扫描结果更新事件：

```bash
svc_subscribe Wifi ScanApInfosUpdated
```

事件参数：

- `ApInfos`：扫描到的 AP 信息列表，包含
  - `SSID`：WiFi 名称
  - `RSSI`：信号强度
  - `AuthMode`：认证模式
