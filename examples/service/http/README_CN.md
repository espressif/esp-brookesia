# HTTP 服务示例

本示例演示如何通过 ESP-Brookesia 的 `Http` Service 发起 HTTPS 请求，并验证 HTTP 服务对外暴露的函数接口与事件接口。示例会先使用 `Wifi` Service 连接到指定 AP，然后以 `https://echo.apifox.com` 作为默认目标，依次运行同步请求、多种 HTTP method、响应头、重试/错误路径、TLS 校验模式、异步请求、取消请求、状态查询和统计重置等流程。

## 目录

- [HTTP 服务示例](#http-服务示例)
  - [功能特性](#功能特性)
  - [硬件要求](#硬件要求)
  - [开发环境](#开发环境)
  - [如何使用](#如何使用)
  - [示例流程](#示例流程)
  - [接口覆盖](#接口覆盖)
  - [故障排除](#故障排除)
  - [技术支持与反馈](#技术支持与反馈)

## 功能特性

- **HTTPS GET 请求**：通过 `HttpHelper::FunctionId::Request` 同步访问 `https://echo.apifox.com/get`，校验状态码和响应体。
- **HTTP method 与请求体**：通过 Apifox Echo 端点验证 `POST`、`PUT`、`PATCH`、`DELETE` 和 `HEAD`，并在适用场景发送 JSON 请求体。
- **响应头解析**：读取 Apifox Echo 返回的自定义响应头。
- **TLS 校验模式**：分别验证 `Default`、`Verify` 和 `Skip` TLS 校验模式。
- **下载到文件**：将响应体下载到 `/littlefs/http_download.bin`，并验证生成的文件。
- **重试/错误路径**：向 HTTP 500 端点发送带 `retry_count` 的请求，验证事件链路保持一致。
- **异步请求与状态查询**：通过 `RequestAsync` 提交请求，再使用 `GetRequestState` 轮询请求状态。
- **请求取消**：提交异步请求后调用 `CancelRequest`，验证请求进入 `Canceled` 状态并触发取消事件。
- **失败路径验证**：将 `max_response_size` 设为较小值，主动触发 `BodyTooLarge`，验证失败事件与错误码；
  同时将 `max_file_size` 设为较小值，验证文件下载的 `FileTooLarge`。
- **事件订阅**：订阅 HTTP 服务的状态、进度、完成、失败和取消事件，确认事件载荷可被解析。
- **统计重置**：调用 `ResetStatistics`，验证服务统计信息重置接口可正常执行。

## 硬件要求

搭载 `ESP32-S3` 或 `ESP32-P4` 芯片、具备 Wi-Fi 联网能力的开发板。示例默认使用 HTTPS 和证书 bundle 校验，因此设备需要能正常访问公网并连接到 `https://echo.apifox.com`。

## 开发环境

请参考以下文档完成 ESP-IDF 与 ESP-Brookesia 开发环境配置：

- [ESP-Brookesia 编程指南 - 版本说明](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia 编程指南 - 开发环境搭建](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-dev-environment)
- [ESP-Brookesia 编程指南 - 如何使用示例工程](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-example-projects)

## 如何使用

示例启动时必须先连接 Wi-Fi。`main/CMakeLists.txt` 会读取以下环境变量并注入为编译宏：

```bash
export CI_TEST_WIFI_2_4G_AP1_SSID="your_ssid"
export CI_TEST_WIFI_2_4G_AP1_PSW="your_password"
export EXAMPLE_HTTP_BASE_URL="https://echo.apifox.com"
```

也可以直接在构建系统中定义：

```cmake
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    EXAMPLE_WIFI_SSID="your_ssid"
    EXAMPLE_WIFI_PSW="your_password"
    EXAMPLE_HTTP_BASE_URL="https://echo.apifox.com"
)
```

未配置 `EXAMPLE_WIFI_SSID` 时，示例会在启动阶段报错退出，避免后续 HTTP 测试在无网络状态下产生误判。

典型构建与运行流程：

```bash
cd examples/service/http
idf.py set-target esp32s3
idf.py build flash monitor
```

## 示例流程

固件启动后会按以下顺序执行，串口日志最终应打印 `=== Http Service Example Completed ===`：

1. 初始化并启动 `ServiceManager`。
2. 订阅 `Http` Service 的全部事件。
3. 绑定 `Wifi` Service 与 `Http` Service。
4. 等待 `Http` Service 进入 `Started` 状态。
5. 通过 `Wifi` Service 连接到指定 AP。
6. 绑定 `SNTP` Service，调用 `Start` 并等待系统时间同步成功。HTTPS 校验依赖准确时间，所以此步在任何 HTTPS 请求之前执行。
7. 调用 `ResetStatistics` 清空 HTTP 统计信息。
8. 同步请求 `https://echo.apifox.com/get`，验证 `RequestStarted`、`RequestProgress` 和 `RequestCompleted`。
9. 同步请求同一网址，但将 `max_response_size` 限制为 64 字节，验证 `BodyTooLarge` 和 `RequestFailed`。
10. 通过 Apifox Echo 端点验证 `POST`、`PUT`、`PATCH`、`DELETE` 和 `HEAD` method。
11. 验证自定义响应头解析、重试/错误行为和 TLS 校验模式。
12. 将响应体下载到 `/littlefs/http_download.bin`，验证文件路径、文件大小、进度事件和完成事件。
13. 使用较小的 `max_file_size` 下载文件，验证 `FileTooLarge` 和 `RequestFailed`。
14. 异步请求同一网址，轮询 `GetRequestState` 直到 `Completed`，并验证完成事件。
15. 异步提交请求后调用 `CancelRequest`，轮询直到 `Canceled`，并验证取消事件。
16. 再次调用 `ResetStatistics`，并验证启动阶段已收到 `Started` 状态事件。

## 接口覆盖

本示例覆盖 `Http` Service 当前对外暴露的全部函数接口：

| 函数接口 | 验证方式 |
| --- | --- |
| `Request` | 同步访问 `https://echo.apifox.com`，验证成功响应、method/body/header/TLS/retry 路径；再通过小响应体上限验证失败路径 |
| 带 `download_path` 的 `Request` | 下载到 `/littlefs/http_download.bin`，并在 `max_file_size` 过小时验证 `FileTooLarge` |
| `RequestAsync` | 提交异步 HTTPS 请求并获取 request id |
| `CancelRequest` | 对异步请求发起取消，并等待 `Canceled` 状态 |
| `GetRequestState` | 轮询异步请求状态，验证 `Completed` 和 `Canceled` |
| `ResetStatistics` | 示例开始和结束前各调用一次 |

本示例也覆盖 `Http` Service 当前定义的全部事件接口：

| 事件接口 | 验证方式 |
| --- | --- |
| `GeneralStateChanged` | 等待 `Started` 状态事件 |
| `RequestStarted` | 每次请求启动时记录 request id 和运行状态 |
| `RequestProgress` | 读取响应体时记录下载进度 |
| `RequestCompleted` | 成功请求结束时解析 `HttpResponse` |
| `RequestFailed` | `BodyTooLarge` 场景下解析失败响应 |
| `RequestCanceled` | 取消请求后解析取消响应 |

> [!NOTE]
> `main/CMakeLists.txt` 会通过 `littlefs_create_partition_image(littlefs_data ../littlefs FLASH_IN_PROJECT)` 为
> 示例创建可写 LittleFS 分区，并挂载到 `/littlefs`。

## 故障排除

**启动时报 `EXAMPLE_WIFI_SSID is empty`**

- 确认已通过环境变量或 CMake 编译宏配置 Wi-Fi SSID。
- 修改环境变量后需要重新运行 `idf.py reconfigure` 或清理后重新构建。

**Wi-Fi 连接失败或超时**

- 确认 `EXAMPLE_WIFI_SSID` 和 `EXAMPLE_WIFI_PSW` 填写正确。
- 确认开发板支持 2.4 GHz Wi-Fi，且 AP 信号稳定。
- 查看串口日志中的 Wi-Fi `GeneralEventHappened` 事件，确认连接过程停在哪个状态。

**HTTPS 请求失败**

- 确认网络可以访问 `https://echo.apifox.com` 或通过 `EXAMPLE_HTTP_BASE_URL` 配置的 URL。
- 确认系统时间、证书 bundle、DNS 和网关配置正常。
- 若日志显示证书相关错误，可先检查工程是否启用了 ESP-IDF 证书 bundle 配置。
- 示例对成功类用例的 `RequestFailed`（连接级抖动）会自动重试最多 3 次。如果当前网络访问默认的
  `https://echo.apifox.com` 不稳定，可以通过 `EXAMPLE_HTTP_BASE_URL` 指向一个更近、可访问的 echo
  服务，例如 `https://httpbingo.org`。该服务需提供 `/get`、`/post`、`/put`、`/patch`、
  `/delete`、`/response-headers`、`/status/{code}`、`/bytes/{n}` 和 `/delay/{n}` 等端点。

**同步成功请求返回体为空或状态码不是 200**

- 确认目标网址没有被网络环境重定向、拦截或替换。
- 若所在网络访问目标网址较慢，可适当增大 `main.cpp` 中的 `HTTP_WAIT_REQUEST_TIMEOUT_MS` 和请求 `timeout_ms`。

**取消请求偶发未进入 `Canceled`**

- 取消验证依赖请求尚未自然完成。如果网络非常快，请多运行一次，或临时降低网络质量、增大响应读取耗时后再观察。

## 技术支持与反馈

请通过以下渠道进行反馈：

- 有关技术问题，请访问 [esp32.com](https://esp32.com/viewforum.php?f=52&sid=86e7d3b29eae6d591c965ec885874da6) 论坛
- 有关功能请求或错误报告，请创建新的 [GitHub Issue](https://github.com/espressif/esp-brookesia/issues)

我们会尽快回复。
