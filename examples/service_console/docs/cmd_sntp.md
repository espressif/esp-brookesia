# SNTP Service

The SNTP (Simple Network Time Protocol) service provides network time synchronization functionality.

Detailed interface descriptions please refer to [SNTP service helper header file](https://github.com/espressif/esp-brookesia/blob/master/service/brookesia_service_helper/include/brookesia/service_helper/sntp.hpp).

## Table of Contents

- [SNTP Service](#sntp-service)
  - [Table of Contents](#table-of-contents)
  - [Set NTP Servers](#set-ntp-servers)
  - [Set Timezone](#set-timezone)
  - [Start/Stop SNTP](#startstop-sntp)
  - [Get Configuration Information](#get-configuration-information)
  - [Check Time Synchronization Status](#check-time-synchronization-status)
  - [Reset Data](#reset-data)
  - [Notes](#notes)

## Set NTP Servers

Set NTP server list:

```bash
svc_call SNTP SetServers {"Servers":["pool.ntp.org","time.apple.com"]}
```

You can set multiple NTP servers, and the system will try to connect to them in order. However, the number of servers cannot exceed the configured value of `CONFIG_LWIP_SNTP_MAX_SERVERS`.

## Set Timezone

Set timezone:

```bash
svc_call SNTP SetTimezone {"Timezone":"CST-8"}
```

Timezone format examples:

- `CST-8`: China Standard Time (UTC+8)
- `EST-5`: Eastern Standard Time (UTC-5)
- `PST-8`: Pacific Standard Time (UTC-8)

## Start/Stop SNTP

Start SNTP service:

```bash
svc_call SNTP Start
```

Stop SNTP service:

```bash
svc_call SNTP Stop
```

## Get Configuration Information

Get currently configured NTP servers:

```bash
svc_call SNTP GetServers
```

Get currently configured timezone:

```bash
svc_call SNTP GetTimezone
```

## Check Time Synchronization Status

Check if time is synchronized:

```bash
svc_call SNTP IsTimeSynced
```

## Reset Data

Reset SNTP service data:

```bash
svc_call SNTP ResetData
```

## Notes

- SNTP service requires WiFi connection
- Default NTP server is `pool.ntp.org`
- The number of servers cannot exceed the configured value of `CONFIG_LWIP_SNTP_MAX_SERVERS`
- Initial synchronization may take some time
- It is recommended to use multiple NTP servers to improve reliability
- Timezone settings affect system time display
