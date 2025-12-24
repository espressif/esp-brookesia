# RPC 服务

RPC（Remote Procedure Call，远程过程调用）服务允许您通过网络调用远程设备上的服务函数，并订阅远程服务事件。

## 目录

- [RPC 服务](#rpc-服务)
  - [目录](#目录)
  - [完整示例](#完整示例)
    - [设备 A（RPC 服务器）](#设备-arpc-服务器)
    - [设备 B（RPC 客户端）](#设备-brpc-客户端)
  - [服务器端命令](#服务器端命令)
    - [启动 RPC 服务器](#启动-rpc-服务器)
    - [连接/断开服务](#连接断开服务)
  - [客户端命令](#客户端命令)
    - [调用远程服务函数](#调用远程服务函数)
    - [订阅远程服务事件](#订阅远程服务事件)
    - [取消订阅远程服务事件](#取消订阅远程服务事件)
  - [注意事项](#注意事项)
  - [使用场景](#使用场景)
  - [故障排除](#故障排除)

## 完整示例

以下是一个完整的 RPC 服务器和客户端设置示例：

### 设备 A（RPC 服务器）

```bash
# 1. 连接到 WiFi
svc_call Wifi SetConnectAp {"SSID":"ssid","Password":"password"}
svc_call Wifi TriggerGeneralAction {"Action":"Connect"}

# 2. 检查 IP 地址（例如：192.168.1.100）

# 3. 在端口 65500 上启动 RPC 服务器
svc_rpc_server start

# 4. 将所有服务连接到 RPC 服务器
svc_rpc_server connect

# 或者只连接特定服务
svc_rpc_server connect -s Wifi

# 5. 启动服务（例如 NVS）
svc_funcs NVS
```

### 设备 B（RPC 客户端）

```bash
# 1. 连接到 WiFi
svc_call Wifi SetConnectAp {"SSID":"ssid","Password":"password"}
svc_call Wifi TriggerGeneralAction {"Action":"Connect"}

# 2. 订阅远程服务事件
svc_rpc_subscribe 192.168.1.100 Wifi ScanApInfosUpdated

# 3. 调用远程服务函数
svc_rpc_call 192.168.1.100 Wifi TriggerScanStart

# 4. 取消订阅远程服务事件
svc_rpc_unsubscribe 192.168.1.100 Wifi ScanApInfosUpdated
```

## 服务器端命令

### 启动 RPC 服务器

在默认端口（65500）上启动 RPC 服务器：

```bash
svc_rpc_server start
```

在自定义端口上启动 RPC 服务器：

```bash
svc_rpc_server start -p 9000
```

停止 RPC 服务器：

```bash
svc_rpc_server stop
```

### 连接/断开服务

将所有服务连接到 RPC 服务器：

```bash
svc_rpc_server connect
```

连接特定服务（逗号分隔）：

```bash
svc_rpc_server connect -s NVS,Wifi
```

断开所有服务：

```bash
svc_rpc_server disconnect
```

断开特定服务：

```bash
svc_rpc_server disconnect -s NVS,Wifi
```

## 客户端命令

### 调用远程服务函数

调用无参数的远程函数：

```bash
svc_rpc_call 192.168.1.100 NVS List
```

调用带参数的远程函数：

```bash
svc_rpc_call 192.168.1.100 NVS Set {"KeyValuePairs":{"key1":"value1"}}
```

使用自定义端口和超时时间（10 秒）调用：

```bash
svc_rpc_call 192.168.1.100 NVS List -p 9000 -t 10000
```

在本地主机上调用：

```bash
svc_rpc_call 127.0.0.1 NVS List
```

### 订阅远程服务事件

订阅远程事件（默认端口 65500，默认超时 2000ms）：

```bash
svc_rpc_subscribe 192.168.1.100 Wifi ScanApInfosUpdated
```

使用自定义端口订阅：

```bash
svc_rpc_subscribe 192.168.1.100 Wifi ScanApInfosUpdated -p 9000
```

使用自定义超时时间（5 秒）订阅：

```bash
svc_rpc_subscribe 192.168.1.100 Wifi GeneralEventHappened -t 5000
```

使用自定义端口和超时时间订阅：

```bash
svc_rpc_subscribe 192.168.1.100 Wifi ScanApInfosUpdated -p 9000 -t 5000
```

### 取消订阅远程服务事件

取消订阅远程事件（默认端口 65500，默认超时 2000ms）：

```bash
svc_rpc_unsubscribe 192.168.1.100 Wifi ScanApInfosUpdated
```

使用自定义端口取消订阅：

```bash
svc_rpc_unsubscribe 192.168.1.100 Wifi ScanApInfosUpdated -p 9000
```

使用自定义超时时间取消订阅：

```bash
svc_rpc_unsubscribe 192.168.1.100 Wifi ScanApInfosUpdated -t 5000
```

使用自定义端口和超时时间取消订阅：

```bash
svc_rpc_unsubscribe 192.168.1.100 Wifi ScanApInfosUpdated -p 9000 -t 5000
```

## 注意事项

**重要提示**：RPC 客户端会自动重用相同的 `host:port` 组合。当您订阅事件或在同一远程服务器上调用函数时，连接会被共享以提高性能。

## 使用场景

- 设备间服务调用：在多个设备之间共享服务功能
- 集中管理：从一台设备管理多台设备
- 远程调试：远程调用服务函数进行调试
- 事件分发：订阅远程设备的事件，实现事件驱动的应用

## 故障排除

- **连接失败**：确保两台设备在同一 WiFi 网络
- **超时错误**：增加超时时间参数 `-t`
- **服务未找到**：确保主机服务以启动，并且已连接到 RPC 服务器（使用 `svc_rpc_server connect`）
