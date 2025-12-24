# RPC Service

The RPC (Remote Procedure Call) service allows you to call service functions on remote devices over the network and subscribe to remote service events.

## Table of Contents

- [RPC Service](#rpc-service)
  - [Table of Contents](#table-of-contents)
  - [Complete Example](#complete-example)
    - [Device A (RPC Server)](#device-a-rpc-server)
    - [Device B (RPC Client)](#device-b-rpc-client)
  - [Server Commands](#server-commands)
    - [Start RPC Server](#start-rpc-server)
    - [Connect/Disconnect Services](#connectdisconnect-services)
  - [Client Commands](#client-commands)
    - [Call Remote Service Functions](#call-remote-service-functions)
    - [Subscribe to Remote Service Events](#subscribe-to-remote-service-events)
    - [Unsubscribe from Remote Service Events](#unsubscribe-from-remote-service-events)
  - [Notes](#notes)
  - [Use Cases](#use-cases)
  - [Troubleshooting](#troubleshooting)

## Complete Example

The following is a complete example of RPC server and client setup:

### Device A (RPC Server)

```bash
# 1. Connect to WiFi
svc_call Wifi SetConnectAp {"SSID":"ssid","Password":"password"}
svc_call Wifi TriggerGeneralAction {"Action":"Connect"}

# 2. Check IP address (e.g., 192.168.1.100)

# 3. Start RPC server on port 65500
svc_rpc_server start

# 4. Connect all services to RPC server
svc_rpc_server connect

# Or connect only specific services
svc_rpc_server connect -s Wifi

# 5. Start a service (e.g., NVS)
svc_funcs NVS
```

### Device B (RPC Client)

```bash
# 1. Connect to WiFi
svc_call Wifi SetConnectAp {"SSID":"ssid","Password":"password"}
svc_call Wifi TriggerGeneralAction {"Action":"Connect"}

# 2. Subscribe to remote service events
svc_rpc_subscribe 192.168.1.100 Wifi ScanApInfosUpdated

# 3. Call remote service functions
svc_rpc_call 192.168.1.100 Wifi TriggerScanStart

# 4. Unsubscribe from remote service events
svc_rpc_unsubscribe 192.168.1.100 Wifi ScanApInfosUpdated
```

## Server Commands

### Start RPC Server

Start RPC server on the default port (65500):

```bash
svc_rpc_server start
```

Start RPC server on a custom port:

```bash
svc_rpc_server start -p 9000
```

Stop RPC server:

```bash
svc_rpc_server stop
```

### Connect/Disconnect Services

Connect all services to RPC server:

```bash
svc_rpc_server connect
```

Connect specific services (comma-separated):

```bash
svc_rpc_server connect -s NVS,Wifi
```

Disconnect all services:

```bash
svc_rpc_server disconnect
```

Disconnect specific services:

```bash
svc_rpc_server disconnect -s NVS,Wifi
```

## Client Commands

### Call Remote Service Functions

Call a remote function without parameters:

```bash
svc_rpc_call 192.168.1.100 NVS List
```

Call a remote function with parameters:

```bash
svc_rpc_call 192.168.1.100 NVS Set {"KeyValuePairs":{"key1":"value1"}}
```

Call with custom port and timeout (10 seconds):

```bash
svc_rpc_call 192.168.1.100 NVS List -p 9000 -t 10000
```

Call on localhost:

```bash
svc_rpc_call 127.0.0.1 NVS List
```

### Subscribe to Remote Service Events

Subscribe to a remote event (default port 65500, default timeout 2000ms):

```bash
svc_rpc_subscribe 192.168.1.100 Wifi ScanApInfosUpdated
```

Subscribe with custom port:

```bash
svc_rpc_subscribe 192.168.1.100 Wifi ScanApInfosUpdated -p 9000
```

Subscribe with custom timeout (5 seconds):

```bash
svc_rpc_subscribe 192.168.1.100 Wifi GeneralEventHappened -t 5000
```

Subscribe with custom port and timeout:

```bash
svc_rpc_subscribe 192.168.1.100 Wifi ScanApInfosUpdated -p 9000 -t 5000
```

### Unsubscribe from Remote Service Events

Unsubscribe from a remote event (default port 65500, default timeout 2000ms):

```bash
svc_rpc_unsubscribe 192.168.1.100 Wifi ScanApInfosUpdated
```

Unsubscribe with custom port:

```bash
svc_rpc_unsubscribe 192.168.1.100 Wifi ScanApInfosUpdated -p 9000
```

Unsubscribe with custom timeout:

```bash
svc_rpc_unsubscribe 192.168.1.100 Wifi ScanApInfosUpdated -t 5000
```

Unsubscribe with custom port and timeout:

```bash
svc_rpc_unsubscribe 192.168.1.100 Wifi ScanApInfosUpdated -p 9000 -t 5000
```

## Notes

**Important**: RPC clients automatically reuse the same `host:port` combination. When you subscribe to events or call functions on the same remote server, the connection is shared to improve performance.

## Use Cases

- Inter-device service calls: Share service functionality across multiple devices
- Centralized management: Manage multiple devices from a single device
- Remote debugging: Call service functions remotely for debugging
- Event distribution: Subscribe to events from remote devices to implement event-driven applications

## Troubleshooting

- **Connection failed**: Ensure both devices are on the same WiFi network
- **Timeout error**: Increase the timeout parameter `-t`
- **Service not found**: Ensure the host service is started and connected to the RPC server (use `svc_rpc_server connect`)
