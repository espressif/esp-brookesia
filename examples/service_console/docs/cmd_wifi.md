# WiFi Service

The WiFi service provides WiFi connection, scanning, and management functionality.

Detailed interface descriptions please refer to [WiFi service helper header file](https://github.com/espressif/esp-brookesia/blob/master/service/brookesia_service_helper/include/brookesia/service_helper/wifi.hpp).

## Table of Contents

- [WiFi Service](#wifi-service)
  - [Table of Contents](#table-of-contents)
  - [Subscribe/Unsubscribe to WiFi Events](#subscribeunsubscribe-to-wifi-events)
  - [Start/Stop WiFi](#startstop-wifi)
  - [Scan AP](#scan-ap)
  - [Connect/Disconnect to AP](#connectdisconnect-to-ap)
  - [Get Connected APs](#get-connected-aps)
  - [Reset Data](#reset-data)

## Subscribe/Unsubscribe to WiFi Events

Subscribe to WiFi general action triggered event:

```bash
svc_subscribe Wifi GeneralActionTriggered
```

Subscribe to WiFi general event:

```bash
svc_subscribe Wifi GeneralEventHappened
```

Subscribe to WiFi scan results updated event:

```bash
svc_subscribe Wifi ScanApInfosUpdated
```

Unsubscribe from events:

```bash
svc_unsubscribe Wifi GeneralActionTriggered
svc_unsubscribe Wifi GeneralEventHappened
svc_unsubscribe Wifi ScanApInfosUpdated
```

## Start/Stop WiFi

Start WiFi:

```bash
svc_call Wifi TriggerGeneralAction {"Action":"Start"}
```

Stop WiFi:

```bash
svc_call Wifi TriggerGeneralAction {"Action":"Stop"}
```

## Scan AP

Set scan parameters:

```bash
svc_call Wifi SetScanParams {"ApCount":10,"IntervalMs":1000,"TimeoutMs":30000}
```

Parameter descriptions:
- `ApCount`: Maximum number of APs to scan
- `IntervalMs`: Scan interval (milliseconds)
- `TimeoutMs`: Scan timeout (milliseconds)

Start scanning:

```bash
svc_call Wifi TriggerScanStart {}
```

Stop scanning:

```bash
svc_call Wifi TriggerScanStop {}
```

## Connect/Disconnect to AP

Set the AP information to connect to:

```bash
svc_call Wifi SetConnectAp {"SSID":"ssid","Password":"password"}
```

Connect to AP:

```bash
svc_call Wifi TriggerGeneralAction {"Action":"Connect"}
```

Disconnect:

```bash
svc_call Wifi TriggerGeneralAction {"Action":"Disconnect"}
```

## Get Connected APs

View currently connected AP information:

```bash
svc_call Wifi GetConnectedAps
```

## Reset Data

Reset WiFi service data:

```bash
svc_call Wifi ResetData
```
