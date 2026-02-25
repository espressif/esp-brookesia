# WiFi 服务

WiFi 服务提供 WiFi 连接、扫描和管理功能，支持 AP 扫描、连接、断开等操作。

详细的接口说明请参考 [WiFi 服务帮助头文件](https://github.com/espressif/esp-brookesia/blob/master/service/brookesia_service_helper/include/brookesia/service_helper/wifi.hpp)。

## 目录

- [WiFi 服务](#wifi-服务)
  - [目录](#目录)
  - [函数调用接口](#函数调用接口)
    - [通用操作](#通用操作)
      - [控制 WiFi 状态](#控制-wifi-状态)
      - [获取 WiFi 状态](#获取-wifi-状态)
      - [重置数据](#重置数据)
    - [Station 操作](#station-操作)
      - [设置要连接的 AP 信息](#设置要连接的-ap-信息)
      - [连接到 AP](#连接到-ap)
      - [获取要连接的 AP 名称](#获取要连接的-ap-名称)
      - [获取已连接的 AP 名称列表](#获取已连接的-ap-名称列表)
      - [断开连接](#断开连接)
      - [设置扫描参数](#设置扫描参数)
      - [开始扫描](#开始扫描)
      - [停止扫描](#停止扫描)
    - [SoftAP 操作](#softap-操作)
      - [设置 SoftAP 参数](#设置-softap-参数)
      - [启动 SoftAP](#启动-softap)
      - [停止 SoftAP](#停止-softap)
      - [启动 SoftAP 配网](#启动-softap-配网)
      - [停止 SoftAP 配网](#停止-softap-配网)
      - [获取 SoftAP 参数](#获取-softap-参数)
  - [事件订阅接口](#事件订阅接口)
    - [通用事件](#通用事件)
      - [通用操作触发事件](#通用操作触发事件)
      - [通用操作完成事件](#通用操作完成事件)
    - [Station 事件](#station-事件)
      - [扫描结果更新事件](#扫描结果更新事件)
    - [SoftAP 事件](#softap-事件)
      - [SoftAP 完成事件](#softap-完成事件)

## 函数调用接口

### 通用操作

#### 控制 WiFi 状态

```bash
svc_call Wifi TriggerGeneralAction {"Action":"Init"}
```

参数说明：

- `Action`：通用操作，可选值为
  - `Init`：初始化 WiFi 服务
  - `Deinit`：释放 WiFi 服务
  - `Start`：启动 WiFi 服务
  - `Stop`：停止 WiFi 服务
  - `Connect`：连接到 AP
  - `Disconnect`：断开连接

#### 获取 WiFi 状态

```bash
svc_call Wifi GetGeneralState
```

#### 重置数据

重置 WiFi 服务数据（清除保存的 AP 信息等）

```bash
svc_call Wifi ResetData
```

### Station 操作

#### 设置要连接的 AP 信息

```bash
svc_call Wifi SetConnectAp {"SSID":"your_ssid","Password":"your_password"}
```

参数说明：

- `SSID`：WiFi 名称
- `Password`：WiFi 密码

#### 连接到 AP

```bash
svc_call Wifi TriggerGeneralAction {"Action":"Connect"}
```

#### 获取要连接的 AP 名称

```bash
svc_call Wifi GetConnectAp
```

#### 获取已连接的 AP 名称列表

```bash
svc_call Wifi GetConnectedAps
```

#### 断开连接

```bash
svc_call Wifi TriggerGeneralAction {"Action":"Disconnect"}
```

> [!NOTE]
> 连接成功后，会自动保存 AP 信息到 NVS 中，下次启动时会自动连接到上次连接的 AP。
> 如果连接断开后，会自动尝试连接之前连接的 AP。

#### 设置扫描参数

```bash
svc_call Wifi SetScanParams {"Param":{"ap_count":10,"interval_ms":1000,"timeout_ms":30000}}
```

参数说明：

- `Param`：扫描参数对象，包含以下字段：
  - `ap_count`：扫描事件返回的 AP 数量上限，默认值为 `20`
  - `interval_ms`：扫描间隔（毫秒），默认值为 `10000`（实际的扫描间隔可能大于这个值）
  - `timeout_ms`：扫描超时时间（毫秒），默认值为 `60000`

> [!NOTE]
> 参数会被保存到 NVS 中，因此只需设置一次即可，后续调用时无需手动指定。

#### 开始扫描

```bash
svc_call Wifi TriggerScanStart
```

#### 停止扫描

```bash
svc_call Wifi TriggerScanStop
```

### SoftAP 操作

#### 设置 SoftAP 参数

```bash
svc_call Wifi SetSoftApParams {"Param":{"ssid":"ssid","password":"password","channel":1}}
```

参数说明：
- `Param`：SoftAP 参数对象，包含以下字段：
  - `ssid`：WiFi 名称
  - `password`：WiFi 密码
  - `channel`（可选）：WiFi 通道，如果未设置，则会自动扫描附近 AP 并选择最佳通道

> [!NOTE]
> 参数会被保存到 NVS 中，因此只需设置一次即可，后续调用时无需手动指定。

#### 启动 SoftAP

```bash
svc_call Wifi TriggerSoftApStart
```

#### 停止 SoftAP

```bash
svc_call Wifi TriggerSoftApStop
```

#### 启动 SoftAP 配网

```bash
svc_call Wifi TriggerSoftApProvisionStart
```

#### 停止 SoftAP 配网

```bash
svc_call Wifi TriggerSoftApProvisionStop
```

#### 获取 SoftAP 参数

```bash
svc_call Wifi GetSoftApParams
```

## 事件订阅接口

### 通用事件

#### 通用操作触发事件

订阅 WiFi 通用操作触发事件（如 Start、Stop、Connect、Disconnect 操作被触发时）：

```bash
svc_subscribe Wifi GeneralActionTriggered
```

事件参数：

- `Action`：触发的操作类型，可选值为 `Start`、`Stop`、`Connect`、`Disconnect`

#### 通用操作完成事件

订阅 WiFi 通用操作完成事件（如 Start、Stop、Connect、Disconnect 操作完成时）：

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

### Station 事件

#### 扫描结果更新事件

订阅 WiFi 扫描结果更新事件：

```bash
svc_subscribe Wifi ScanApInfosUpdated
```

事件参数：

- `ApInfos`：扫描到的 AP 信息列表，包含
  - `SSID`：WiFi 名称
  - `RSSI`：信号强度
  - `AuthMode`：认证模式

### SoftAP 事件

#### SoftAP 完成事件

订阅 WiFi SoftAP 完成事件：

```bash
svc_subscribe Wifi SoftApEventHappened
```

事件参数：

- `Event`：事件类型，可选值为
  - `Started`：SoftAP 已启动
  - `Stopped`：SoftAP 已停止
