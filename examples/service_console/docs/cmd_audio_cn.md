# 音频服务

音频服务提供了音频播放控制、编解码、AFE（音频前端）等功能，支持播放本地文件和网络音频资源。

详细的接口说明请参考 [音频服务帮助头文件](https://github.com/espressif/esp-brookesia/blob/master/service/brookesia_service_helper/include/brookesia/service_helper/audio.hpp)。

> [!NOTE]
> - 音频服务需要在支持的开发板上使用，请参考 [准备工作](../README_CN.md#准备工作) 章节了解默认支持的开发板列表
> - 本地文件需要保存到 [spiffs](../spiffs/) 目录下，文件路径格式为：`file://spiffs/文件名`

## 目录

- [音频服务](#音频服务)
  - [目录](#目录)
  - [函数调用接口](#函数调用接口)
    - [播放音频](#播放音频)
    - [播放多个音频](#播放多个音频)
    - [播放控制](#播放控制)
    - [设置音量](#设置音量)
    - [获取音量](#获取音量)
  - [事件订阅接口](#事件订阅接口)
    - [播放状态变化事件](#播放状态变化事件)
    - [AFE 事件](#afe-事件)

## 函数调用接口

### 播放音频

播放音频文件（支持本地文件和网络 URL）：

```bash
svc_call Audio PlayUrl {"Url":"file://spiffs/example.mp3"}
```

播放网络音频：

```bash
svc_call Audio PlayUrl {"Url":"https://dl.espressif.com/AE/esp-brookesia/example.mp3"}
```

播放音频文件，并配置播放参数：

```bash
svc_call Audio PlayUrl {"Url":"file://spiffs/example.mp3","Config":{"interrupt":false,"delay_ms":1000,"loop_count":3,"loop_interval_ms":1000,"timeout_ms":10000}}
```

参数说明：

- `Url`：音频文件 URL，支持本地文件和网络 URL
- `Config`：播放配置，可选参数如下：
  - `interrupt`：是否中断当前播放，如果为 `false`，则将音频文件添加到播放队列中，等待当前播放完成后自动播放
  - `delay_ms`：延迟播放时间，单位为毫秒
  - `loop_count`：循环播放次数，如果为 `0`，则不循环播放
  - `loop_interval_ms`：循环播放间隔时间，单位为毫秒
  - `timeout_ms`：播放超时时间，单位为毫秒。从播放开始计算（不包含延迟和暂停时间），超过该时间后，播放将停止

### 播放多个音频

播放多个音频文件：

```bash
svc_call Audio PlayUrls {"Urls":["file://spiffs/1.mp3","file://spiffs/2.mp3","file://spiffs/3.mp3"]}
```

播放多个音频文件，并配置播放参数：

```bash
svc_call Audio PlayUrls {"Urls":["file://spiffs/1.mp3","file://spiffs/2.mp3"],"Config":{"interrupt":true}}
```

### 播放控制

```bash
svc_call Audio PlayControl {"Action":"Pause"}
```

参数说明：

- `Action`：播放控制动作，可选值为
  - `Pause`：暂停播放
  - `Resume`：恢复播放
  - `Stop`：停止播放

### 设置音量

设置音量（0-100）：

```bash
svc_call Audio SetVolume {"Volume":90}
```

> [!NOTE]
> 参数会被保存到 NVS 中，因此只需设置一次即可，后续调用时无需手动指定。

### 获取音量

获取当前音量：

```bash
svc_call Audio GetVolume
```

## 事件订阅接口

### 播放状态变化事件

订阅播放状态变化事件：

```bash
svc_subscribe Audio PlayStateChanged
```

事件参数：

- `State`：播放状态，可选值为 `Idle`（空闲）、`Playing`（播放中）、`Paused`（暂停）

### AFE 事件

订阅 AFE（音频前端）事件：

```bash
svc_subscribe Audio AFE_EventHappened
```

事件参数：

- `Event`：AFE 事件类型，可选值为
  - `VAD_Start`：语音活动检测开始
  - `VAD_End`：语音活动检测结束
  - `WakeStart`：唤醒词检测开始
  - `WakeEnd`：唤醒词检测结束
