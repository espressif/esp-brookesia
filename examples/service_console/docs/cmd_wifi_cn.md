# WiFi 服务

WiFi 服务提供了 WiFi 连接、扫描和管理功能。

详细的接口说明请参考 [WiFi 服务帮助头文件](https://github.com/espressif/esp-brookesia/blob/master/service/brookesia_service_helper/include/brookesia/service_helper/wifi.hpp)。

## 目录

- [WiFi 服务](#wifi-服务)
  - [目录](#目录)
  - [订阅/取消订阅 WiFi 事件](#订阅取消订阅-wifi-事件)
  - [启动/停止 WiFi](#启动停止-wifi)
  - [扫描 AP](#扫描-ap)
  - [连接/断开 AP](#连接断开-ap)
  - [获取已连接的 AP](#获取已连接的-ap)
  - [重置数据](#重置数据)

## 订阅/取消订阅 WiFi 事件

订阅 WiFi 通用操作触发事件：

```bash
svc_subscribe Wifi GeneralActionTriggered
```

订阅 WiFi 通用事件：

```bash
svc_subscribe Wifi GeneralEventHappened
```

订阅 WiFi 扫描结果更新事件：

```bash
svc_subscribe Wifi ScanApInfosUpdated
```

取消订阅事件：

```bash
svc_unsubscribe Wifi GeneralActionTriggered
svc_unsubscribe Wifi GeneralEventHappened
svc_unsubscribe Wifi ScanApInfosUpdated
```

## 启动/停止 WiFi

启动 WiFi：

```bash
svc_call Wifi TriggerGeneralAction {"Action":"Start"}
```

停止 WiFi：

```bash
svc_call Wifi TriggerGeneralAction {"Action":"Stop"}
```

## 扫描 AP

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

## 连接/断开 AP

设置要连接的 AP 信息：

```bash
svc_call Wifi SetConnectAp {"SSID":"ssid","Password":"password"}
```

连接到 AP：

```bash
svc_call Wifi TriggerGeneralAction {"Action":"Connect"}
```

断开连接：

```bash
svc_call Wifi TriggerGeneralAction {"Action":"Disconnect"}
```

## 获取已连接的 AP

查看当前连接的 AP 信息：

```bash
svc_call Wifi GetConnectedAps
```

## 重置数据

重置 WiFi 服务数据：

```bash
svc_call Wifi ResetData
```
