# WiFi Service

The WiFi service provides WiFi connection, scanning, and management functionality, supporting AP scanning, connection, disconnection, and other operations.

Detailed interface descriptions please refer to [WiFi service helper header file](https://github.com/espressif/esp-brookesia/blob/master/service/brookesia_service_helper/include/brookesia/service_helper/wifi.hpp).

## Table of Contents

- [WiFi Service](#wifi-service)
  - [Table of Contents](#table-of-contents)
  - [Function Call Interface](#function-call-interface)
    - [General Operations](#general-operations)
      - [Control WiFi State](#control-wifi-state)
      - [Get WiFi State](#get-wifi-state)
      - [Reset Data](#reset-data)
    - [Station Operations](#station-operations)
      - [Set AP Information to Connect](#set-ap-information-to-connect)
      - [Connect to AP](#connect-to-ap)
      - [Get AP Name to Connect](#get-ap-name-to-connect)
      - [Get Connected AP Name List](#get-connected-ap-name-list)
      - [Disconnect](#disconnect)
      - [Set Scan Parameters](#set-scan-parameters)
      - [Start Scanning](#start-scanning)
      - [Stop Scanning](#stop-scanning)
    - [SoftAP Operations](#softap-operations)
      - [Set SoftAP Parameters](#set-softap-parameters)
      - [Start SoftAP](#start-softap)
      - [Stop SoftAP](#stop-softap)
      - [Start SoftAP Provisioning](#start-softap-provisioning)
      - [Stop SoftAP Provisioning](#stop-softap-provisioning)
      - [Get SoftAP Parameters](#get-softap-parameters)
  - [Event Subscription Interface](#event-subscription-interface)
    - [General Events](#general-events)
      - [General Action Triggered Event](#general-action-triggered-event)
      - [General Event Happened](#general-event-happened)
    - [Station Events](#station-events)
      - [Scan Results Updated Event](#scan-results-updated-event)
    - [SoftAP Events](#softap-events)
      - [SoftAP Event Happened](#softap-event-happened)

## Function Call Interface

### General Operations

#### Control WiFi State

```bash
svc_call Wifi TriggerGeneralAction {"Action":"Init"}
```

Parameter description:

- `Action`: General action, valid values are
  - `Init`: Initialize WiFi service
  - `Deinit`: Deinitialize WiFi service
  - `Start`: Start WiFi service
  - `Stop`: Stop WiFi service
  - `Connect`: Connect to AP
  - `Disconnect`: Disconnect from AP

#### Get WiFi State

```bash
svc_call Wifi GetGeneralState
```

#### Reset Data

Reset WiFi service data (clear saved AP information, etc.):

```bash
svc_call Wifi ResetData
```

### Station Operations

#### Set AP Information to Connect

```bash
svc_call Wifi SetConnectAp {"SSID":"your_ssid","Password":"your_password"}
```

Parameter description:

- `SSID`: WiFi name
- `Password`: WiFi password

#### Connect to AP

```bash
svc_call Wifi TriggerGeneralAction {"Action":"Connect"}
```

#### Get AP Name to Connect

```bash
svc_call Wifi GetConnectAp
```

#### Get Connected AP Name List

```bash
svc_call Wifi GetConnectedAps
```

#### Disconnect

```bash
svc_call Wifi TriggerGeneralAction {"Action":"Disconnect"}
```

> [!NOTE]
> After successful connection, AP information will be automatically saved to NVS. On next startup, it will automatically connect to the last connected AP.
> If the connection is disconnected, it will automatically attempt to reconnect to the previously connected AP.

#### Set Scan Parameters

```bash
svc_call Wifi SetScanParams {"Param":{"ap_count":10,"interval_ms":1000,"timeout_ms":30000}}
```

Parameter description:

- `Param`: Scan parameters object, containing the following fields:
  - `ap_count`: Maximum number of APs returned in scan events, default value is `20`
  - `interval_ms`: Scan interval (milliseconds), default value is `10000` (actual scan interval may be greater than this value)
  - `timeout_ms`: Scan timeout (milliseconds), default value is `60000`

> [!NOTE]
> Parameters will be saved to NVS, so you only need to set them once. Subsequent calls do not require manual specification.

#### Start Scanning

```bash
svc_call Wifi TriggerScanStart
```

#### Stop Scanning

```bash
svc_call Wifi TriggerScanStop
```

### SoftAP Operations

#### Set SoftAP Parameters

```bash
svc_call Wifi SetSoftApParams {"Param":{"ssid":"ssid","password":"password","channel":1}}
```

Parameter description:
- `Param`: SoftAP parameters object, containing the following fields:
  - `ssid`: WiFi name
  - `password`: WiFi password
  - `channel` (optional): WiFi channel. If not set, will automatically scan nearby APs and select the best channel

> [!NOTE]
> Parameters will be saved to NVS, so you only need to set them once. Subsequent calls do not require manual specification.

#### Start SoftAP

```bash
svc_call Wifi TriggerSoftApStart
```

#### Stop SoftAP

```bash
svc_call Wifi TriggerSoftApStop
```

#### Start SoftAP Provisioning

```bash
svc_call Wifi TriggerSoftApProvisionStart
```

#### Stop SoftAP Provisioning

```bash
svc_call Wifi TriggerSoftApProvisionStop
```

#### Get SoftAP Parameters

```bash
svc_call Wifi GetSoftApParams
```

## Event Subscription Interface

### General Events

#### General Action Triggered Event

Subscribe to WiFi general action triggered event (when Start, Stop, Connect, Disconnect actions are triggered):

```bash
svc_subscribe Wifi GeneralActionTriggered
```

Event parameters:

- `Action`: Triggered action type, valid values are `Start`, `Stop`, `Connect`, `Disconnect`

#### General Event Happened

Subscribe to WiFi general event happened (when Start, Stop, Connect, Disconnect actions complete):

```bash
svc_subscribe Wifi GeneralEventHappened
```

Event parameters:

- `Event`: Event type, valid values are
  - `Started`: WiFi service has started
  - `Stopped`: WiFi service has stopped
  - `Connected`: Connected to AP
  - `Disconnected`: Disconnected from AP
  - `GotIp`: IP address obtained
  - `LostIp`: IP address lost

### Station Events

#### Scan Results Updated Event

Subscribe to WiFi scan results updated event:

```bash
svc_subscribe Wifi ScanApInfosUpdated
```

Event parameters:

- `ApInfos`: List of scanned AP information, containing
  - `SSID`: WiFi name
  - `RSSI`: Signal strength
  - `AuthMode`: Authentication mode

### SoftAP Events

#### SoftAP Event Happened

Subscribe to WiFi SoftAP event happened:

```bash
svc_subscribe Wifi SoftApEventHappened
```

Event parameters:

- `Event`: Event type, valid values are
  - `Started`: SoftAP has started
  - `Stopped`: SoftAP has stopped
