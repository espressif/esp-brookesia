# SNTP 服务

SNTP（Simple Network Time Protocol，简单网络时间协议）服务提供了网络时间同步功能。

详细的接口说明请参考 [SNTP 服务帮助头文件](https://github.com/espressif/esp-brookesia/blob/master/service/brookesia_service_helper/include/brookesia/service_helper/sntp.hpp)。

## 目录

- [SNTP 服务](#sntp-服务)
  - [目录](#目录)
  - [设置 NTP 服务器](#设置-ntp-服务器)
  - [设置时区](#设置时区)
  - [启动/停止 SNTP](#启动停止-sntp)
  - [获取配置信息](#获取配置信息)
  - [检查时间同步状态](#检查时间同步状态)
  - [重置数据](#重置数据)
  - [注意事项](#注意事项)

## 设置 NTP 服务器

设置 NTP 服务器列表：

```bash
svc_call SNTP SetServers {"Servers":["pool.ntp.org","time.apple.com"]}
```

可以设置多个 NTP 服务器，系统会按顺序尝试连接，但是服务器的数量不能超过 `CONFIG_LWIP_SNTP_MAX_SERVERS` 的配置值。

## 设置时区

设置时区：

```bash
svc_call SNTP SetTimezone {"Timezone":"CST-8"}
```

时区格式示例：

- `CST-8`：中国标准时间（UTC+8）
- `EST-5`：美国东部标准时间（UTC-5）
- `PST-8`：太平洋标准时间（UTC-8）

## 启动/停止 SNTP

启动 SNTP 服务：

```bash
svc_call SNTP Start
```

停止 SNTP 服务：

```bash
svc_call SNTP Stop
```

## 获取配置信息

获取当前配置的 NTP 服务器：

```bash
svc_call SNTP GetServers
```

获取当前配置的时区：

```bash
svc_call SNTP GetTimezone
```

## 检查时间同步状态

检查时间是否已同步：

```bash
svc_call SNTP IsTimeSynced
```

## 重置数据

重置 SNTP 服务数据：

```bash
svc_call SNTP ResetData
```

## 注意事项

- SNTP 服务需要 WiFi 连接
- 默认使用 `pool.ntp.org` 作为 NTP 服务器
- 服务器数量不能超过 `CONFIG_LWIP_SNTP_MAX_SERVERS` 的配置值
- 首次同步可能需要一些时间
- 建议使用多个 NTP 服务器以提高可靠性
- 时区设置会影响系统时间显示
