| Supported Chips | ESP32-S3 | ESP32-P4 |
| :-------------: | :------: | :------: |
|                 |    ✅     |    ✅     |

# WiFi Service Example

[中文版本](./README_CN.md)

This example demonstrates how to use the WiFi service in the ESP-Brookesia framework for wireless network management, covering event subscription, STA scanning, STA connect/disconnect, auto-reconnect, SoftAP provisioning, and credential reset.

## 📑 Table of Contents

- [WiFi Service Example](#wifi-service-example)
  - [📑 Table of Contents](#-table-of-contents)
  - [✨ Features](#-features)
  - [🚩 Getting Started](#-getting-started)
    - [Hardware Requirements](#hardware-requirements)
    - [Development Environment](#development-environment)
  - [🔨 Build and Flash](#-build-and-flash)
  - [🚀 Quick Start](#-quick-start)
  - [📖 Example Details](#-example-details)
    - [Event Subscription Demo](#event-subscription-demo)
    - [STA Scan Demo](#sta-scan-demo)
    - [STA Connect Demo](#sta-connect-demo)
    - [Auto-Reconnect Demo](#auto-reconnect-demo)
    - [SoftAP Provisioning Demo](#softap-provisioning-demo)
    - [Reset Data Demo](#reset-data-demo)
  - [🔍 Troubleshooting](#-troubleshooting)
  - [💬 Technical Support and Feedback](#-technical-support-and-feedback)

## ✨ Features

- 📡 **Event Subscription**: Listen to all WiFi events (general actions, state changes, scan results, SoftAP events) via RAII-style subscription handles
- 🔍 **STA Scan**: Configure scan parameters (AP count, interval, timeout) for periodic automatic scanning; receive AP lists via events
- 🔗 **STA Connect**: Set a target AP (SSID/password), trigger connection, wait for connect/disconnect events, and verify state
- 🔄 **Auto-Reconnect**: Restart WiFi and automatically reconnect to a previously saved AP, then verify successful reconnection
- 📶 **SoftAP Provisioning**: Start a SoftAP hotspot; after a phone connects and submits home WiFi credentials via the provisioning page, the hotspot is stopped automatically
- 🗑️ **Credential Reset**: Clear all WiFi credentials stored in NVS and verify the result

## 🚩 Getting Started

### Hardware Requirements

A development board equipped with an `ESP32-S3` or `ESP32-P4` chip, `Flash >= 4 MB`, with WiFi capability.

### Development Environment

- ESP-IDF `v5.5.2` TAG (recommended) or `release/v5.5` branch

## 🔨 Build and Flash

```bash
idf.py -p PORT build flash monitor
```

Press `Ctrl-]` to exit the serial monitor.

> [!NOTE]
> To run the STA connect and auto-reconnect demos, uncomment and fill in the WiFi credentials at the top of `main/main.cpp`:
>
> ```c
> #define EXAMPLE_WIFI_SSID "your_ssid"
> #define EXAMPLE_WIFI_PSW  "your_password"
> ```
>
> If credentials are not defined, the STA connect and auto-reconnect demos are skipped; only the scan, SoftAP provisioning, and reset data demos will run.

## 🚀 Quick Start

After flashing, the program automatically runs the following demos in sequence. All output is printed via the serial monitor:

1. **Event Subscription Demo**: Registers all WiFi event callbacks; subsequent demo events are printed to the serial monitor in real time
2. **STA Scan Demo**: Automatically scans nearby APs and prints each round's AP list (SSID, signal strength, etc.)
3. **STA Connect Demo** (requires credentials): Connects to the specified AP, verifies success, then disconnects
4. **Auto-Reconnect Demo** (requires credentials): Restarts WiFi and verifies automatic reconnection to the saved AP
5. **SoftAP Provisioning Demo**: Starts a hotspot and waits for a phone to connect and complete provisioning (120-second timeout)
6. **Reset Data Demo**: Clears all saved credentials and verifies the result

## 📖 Example Details

### Event Subscription Demo

Registers the following 5 event types via `WifiHelper::subscribe_event()`. Subscription handles are stored RAII-style in a `global_connections` vector, keeping subscriptions alive for the lifetime of `app_main`:

| Event | Description |
|-------|-------------|
| `GeneralActionTriggered` | Fires when a general action (Init/Start/Stop/Connect/Disconnect) is triggered |
| `GeneralEventHappened` | Fires when the WiFi state changes (Started/Connected/Disconnected, etc.); carries an "unexpected" flag |
| `ScanStateChanged` | Fires when a scan task starts or ends; carries a boolean indicating whether scanning is in progress |
| `ScanApInfosUpdated` | Fires when the AP list is updated after each scan round; carries an array of AP info objects |
| `SoftApEventHappened` | Fires when the SoftAP hotspot state changes (Started/Stopped, etc.) |

### STA Scan Demo

Configures scan parameters and starts a periodic scan, waiting for completion via `EventMonitor`:

```
ScanParams:
  ap_count    = 10       # Maximum APs returned per round
  interval_ms = 10000    # Interval between scan rounds
  timeout_ms  = 20000    # Auto-stop after total timeout
```

After scanning completes, the last round's results are read and the full AP list (SSID, RSSI, encryption, etc.) is printed to the serial monitor. The demo requires at least 1 and at most 2 successful scan rounds.

### STA Connect Demo

> [!NOTE]
> Only runs if `EXAMPLE_WIFI_SSID` and `EXAMPLE_WIFI_PSW` are defined in `main.cpp`.

Flow: Set target AP → Trigger connect → Wait for `Connected` event (10 s timeout) → Verify target AP is in the connected AP list → Trigger disconnect → Wait for `Disconnected` event (2 s timeout) → Verify state returns to `Started`.

### Auto-Reconnect Demo

> [!NOTE]
> Only runs if `EXAMPLE_WIFI_SSID` and `EXAMPLE_WIFI_PSW` are defined in `main.cpp`, and must run after the STA Connect demo (so credentials are already saved).

Flow: Stop WiFi → Restart → Wait for `Connected` event (10 s timeout) → Verify automatic reconnection succeeded.

### SoftAP Provisioning Demo

Flow:

1. If currently connected, trigger disconnect first
2. Call `TriggerSoftApProvisionStart` and wait for the `SoftApEvent::Started` event
3. Read and print the SoftAP SSID and password (serial prompt: connect your phone to this hotspot)
4. Wait for the `Connected` event after the phone submits home WiFi credentials via the provisioning page (120-second timeout)
5. On success, print the connected AP name; on timeout, print a warning and continue
6. Call `TriggerSoftApProvisionStop` and wait for the `SoftApEvent::Stopped` event

### Reset Data Demo

Calls `ResetData` to clear all WiFi credentials (connected AP history) from NVS, then verifies that the connected AP list is empty.

## 🔍 Troubleshooting

**STA connection fails or times out**

- Verify that `EXAMPLE_WIFI_SSID` and `EXAMPLE_WIFI_PSW` are set correctly.
- Check that the target AP is within range (its SSID should appear in the scan demo output).
- Check the `GeneralEventHappened` events in the serial log to see if a connection-failure event was fired.

**SoftAP provisioning timeout**

- Confirm your phone has successfully connected to the hotspot SSID printed in the serial monitor.
- Manually open the provisioning page in a browser (usually `192.168.4.1`).
- After the 120-second timeout, the program continues with the remaining demos without interruption.

**No scan results**

- Confirm there are WiFi APs available nearby.
- Check the serial log for `ScanApInfosUpdated` events; if none appear, verify that WiFi started successfully.

## 💬 Technical Support and Feedback

Please use the following channels for feedback:

- For technical questions, visit the [esp32.com](https://esp32.com/viewforum.php?f=52&sid=86e7d3b29eae6d591c965ec885874da6) forum
- For feature requests or bug reports, open a new [GitHub Issue](https://github.com/espressif/esp-brookesia/issues)

We will respond as soon as possible.
