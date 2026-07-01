# HTTP Service Example

This example shows how to send HTTPS requests through the ESP-Brookesia `Http` service and verify the function and
event interfaces exposed by the HTTP service. The example first connects to the configured AP with the `Wifi` service,
then uses `https://echo.apifox.com` as the default target and runs synchronous requests, multiple HTTP methods, response
headers, retry/error paths, TLS verification modes, asynchronous requests, cancellation, state queries, and statistics
reset flows.

## Contents

- [HTTP Service Example](#http-service-example)
  - [Features](#features)
  - [Hardware Requirements](#hardware-requirements)
  - [Development Environment](#development-environment)
  - [How to Use](#how-to-use)
  - [Example Flow](#example-flow)
  - [Interface Coverage](#interface-coverage)
  - [Troubleshooting](#troubleshooting)
  - [Technical Support and Feedback](#technical-support-and-feedback)

## Features

- **HTTPS GET request**: Uses `HttpHelper::FunctionId::Request` to synchronously access
  `https://echo.apifox.com/get` and verify the status code and response body.
- **HTTP methods and request body**: Verifies `POST`, `PUT`, `PATCH`, `DELETE`, and `HEAD` through Apifox Echo
  endpoints, including JSON request bodies where applicable.
- **Response headers**: Reads a custom response header returned by Apifox Echo.
- **TLS verification modes**: Runs requests with `Default`, `Verify`, and `Skip` TLS verification modes.
- **Download to file**: Downloads a response body to `/littlefs/http_download.bin` and verifies the generated file.
- **Retry/error path**: Sends a request to an HTTP 500 endpoint with `retry_count` configured and verifies the event
  path remains consistent.
- **Asynchronous request and state query**: Submits a request with `RequestAsync`, then polls the request state with
  `GetRequestState`.
- **Request cancellation**: Calls `CancelRequest` after submitting an asynchronous request and verifies that the
  request enters the `Canceled` state and emits a cancellation event.
- **Failure path verification**: Sets `max_response_size` to a small value to trigger `BodyTooLarge` and verify the
  failure event and error code. It also sets a small `max_file_size` to trigger `FileTooLarge` for file downloads.
- **Event subscription**: Subscribes to HTTP service state, progress, completed, failed, and canceled events and checks
  that event payloads can be parsed.
- **Statistics reset**: Calls `ResetStatistics` and verifies that the service statistics reset API runs correctly.

## Hardware Requirements

Use a development board based on `ESP32-S3` or `ESP32-P4` with Wi-Fi connectivity. The example uses HTTPS and
certificate bundle verification by default, so the device must be able to access the public Internet and connect to
`https://echo.apifox.com`.

## Development Environment

Refer to the following documents to set up ESP-IDF and ESP-Brookesia:

- [ESP-Brookesia Programming Guide - Versioning](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia Programming Guide - Development Environment](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-dev-environment)
- [ESP-Brookesia Programming Guide - Example Projects](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-example-projects)

## How to Use

The example must connect to Wi-Fi before starting HTTP requests. `main/CMakeLists.txt` reads the following environment
variables and injects them as compile definitions:

```bash
export CI_TEST_WIFI_2_4G_AP1_SSID="your_ssid"
export CI_TEST_WIFI_2_4G_AP1_PSW="your_password"
export EXAMPLE_HTTP_BASE_URL="https://echo.apifox.com"
```

You can also define them directly in the build system:

```cmake
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    EXAMPLE_WIFI_SSID="your_ssid"
    EXAMPLE_WIFI_PSW="your_password"
    EXAMPLE_HTTP_BASE_URL="https://echo.apifox.com"
)
```

When `EXAMPLE_WIFI_SSID` is not configured, the example exits during startup to avoid misleading HTTP test results
while the device is offline.

Typical build and run flow:

```bash
cd examples/service/http
idf.py set-target esp32s3
idf.py build flash monitor
```

## Example Flow

After the firmware starts, it runs the following steps. The serial log should eventually print
`=== Http Service Example Completed ===`:

1. Initialize and start `ServiceManager`.
2. Subscribe to all `Http` service events.
3. Bind the `Wifi` service and the `Http` service.
4. Wait for the `Http` service to enter the `Started` state.
5. Connect to the configured AP through the `Wifi` service.
6. Bind the `SNTP` service, call `Start`, and wait for the system clock to sync. HTTPS requires accurate time
   so this step happens before any HTTPS traffic.
7. Call `ResetStatistics` to clear HTTP statistics.
8. Send a synchronous request to `https://echo.apifox.com/get` and verify `RequestStarted`, `RequestProgress`, and
   `RequestCompleted`.
9. Send another synchronous request to the same URL with `max_response_size` limited to 64 bytes, and verify
   `BodyTooLarge` and `RequestFailed`.
10. Verify `POST`, `PUT`, `PATCH`, `DELETE`, and `HEAD` method requests against Apifox Echo endpoints.
11. Verify custom response header parsing, retry/error behavior, and TLS verification modes.
12. Download a response body to `/littlefs/http_download.bin`, then verify the file path, file size, progress event,
    and completed event.
13. Download with a small `max_file_size` limit and verify `FileTooLarge` and `RequestFailed`.
14. Send an asynchronous request to the same URL, poll `GetRequestState` until `Completed`, and verify the completed
    event.
15. Submit an asynchronous request, call `CancelRequest`, poll until `Canceled`, and verify the canceled event.
16. Call `ResetStatistics` and verify the `Started` service state event observed during startup.

## Interface Coverage

This example covers all function interfaces currently exposed by the `Http` service:

| Function interface | Verification |
| --- | --- |
| `Request` | Synchronously accesses `https://echo.apifox.com`, verifies success, method/body/header/TLS/retry paths, and verifies the failure path with a small response body limit |
| `Request` with `download_path` | Downloads to `/littlefs/http_download.bin` and verifies `FileTooLarge` when `max_file_size` is too small |
| `RequestAsync` | Submits an asynchronous HTTPS request and obtains a request id |
| `CancelRequest` | Cancels an asynchronous request and waits for the `Canceled` state |
| `GetRequestState` | Polls asynchronous request state and verifies `Completed` and `Canceled` |
| `ResetStatistics` | Called once at the beginning and once near the end of the example |

This example also covers all event interfaces currently defined by the `Http` service:

| Event interface | Verification |
| --- | --- |
| `GeneralStateChanged` | Waits for the `Started` state event |
| `RequestStarted` | Records the request id and running state when each request starts |
| `RequestProgress` | Records download progress while reading the response body |
| `RequestCompleted` | Parses `HttpResponse` after a successful request finishes |
| `RequestFailed` | Parses the failed response in the `BodyTooLarge` scenario |
| `RequestCanceled` | Parses the canceled response after request cancellation |

> [!NOTE]
> `main/CMakeLists.txt` uses `littlefs_create_partition_image(littlefs_data ../littlefs FLASH_IN_PROJECT)` so the
> example has a writable LittleFS partition mounted at `/littlefs`.

## Troubleshooting

**Startup reports `EXAMPLE_WIFI_SSID is empty`**

- Make sure the Wi-Fi SSID is configured through environment variables or CMake compile definitions.
- After changing environment variables, run `idf.py reconfigure` or clean and rebuild the project.

**Wi-Fi connection fails or times out**

- Make sure `EXAMPLE_WIFI_SSID` and `EXAMPLE_WIFI_PSW` are correct.
- Make sure the board supports 2.4 GHz Wi-Fi and the AP signal is stable.
- Check the Wi-Fi `GeneralEventHappened` events in the serial log to see which state the connection stopped at.

**HTTPS request fails**

- Make sure the network can access `https://echo.apifox.com` or the URL configured with `EXAMPLE_HTTP_BASE_URL`.
- Check system time, certificate bundle, DNS, and gateway configuration.
- If the log shows certificate-related errors, first check whether the ESP-IDF certificate bundle is enabled.
- The example automatically retries `RequestFailed` (transient connection failures) up to 3 times for the success
  paths. If reaching the default `https://echo.apifox.com` is unreliable on the network, override
  `EXAMPLE_HTTP_BASE_URL` with a closer reachable echo service, such as `https://httpbingo.org`, that exposes
  `/get`, `/post`, `/put`, `/patch`, `/delete`, `/response-headers`, `/status/{code}`, `/bytes/{n}`, and
  `/delay/{n}` endpoints.

**The synchronous success request returns an empty body or a status code other than 200**

- Make sure the target URL is not redirected, blocked, or replaced by the network environment.
- If accessing the target URL is slow on the current network, increase `HTTP_WAIT_REQUEST_TIMEOUT_MS` and request
  `timeout_ms` in `main.cpp`.

**Cancellation occasionally does not enter `Canceled`**

- The cancellation check depends on the request not finishing naturally before cancellation is handled. If the network
  is very fast, run the example again, temporarily degrade network quality, or increase response read time before
  observing the result.

## Technical Support and Feedback

Use the following channels for feedback:

- For technical questions, visit the [esp32.com](https://esp32.com/viewforum.php?f=52&sid=86e7d3b29eae6d591c965ec885874da6) forum
- For feature requests or bug reports, create a new [GitHub Issue](https://github.com/espressif/esp-brookesia/issues)

We will reply as soon as possible.
