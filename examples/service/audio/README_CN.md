# 音频服务示例

[English Version](./README.md)

本示例演示了如何基于 ESP-Brookesia 框架使用音频服务（`brookesia_service_audio`），涵盖音频文件播放、播放控制、编解码回环测试以及 AFE（音频前端）语音活动检测与唤醒词识别等功能。

## 📑 目录

- [音频服务示例](#音频服务示例)
  - [📑 目录](#-目录)
  - [✨ 功能特性](#-功能特性)
  - [🚩 快速入门](#-快速入门)
    - [硬件要求](#硬件要求)
    - [开发环境](#开发环境)
  - [🔨 如何使用](#-如何使用)
  - [🚀 运行说明](#-运行说明)
    - [音频文件播放](#音频文件播放)
    - [播放控制](#播放控制)
    - [编解码回环测试](#编解码回环测试)
    - [AFE 音频前端](#afe-音频前端)
  - [🔍 故障排除](#-故障排除)
  - [💬 技术支持与反馈](#-技术支持与反馈)

## ✨ 功能特性

- 🎵 **音频文件播放**：支持播放单个或多个 URL（本地 SPIFFS / 网络），可配置循环次数、队列追加或打断当前播放
- ⏯️ **播放控制**：支持暂停、恢复、停止操作，并通过事件订阅实时获取播放状态变化
- 🎙️ **编解码回环测试**：启动编码器录制麦克风输入，完成后通过解码器回放，支持 PCM、OPUS、G711A 三种格式
- 🧠 **AFE 音频前端**：集成 VAD（语音活动检测）与 WakeNet（唤醒词识别），通过事件订阅实时响应语音与唤醒事件

## 🚩 快速入门

### 硬件要求

本示例通过 [brookesia_hal_boards](https://components.espressif.com/components/espressif/brookesia_hal_boards) 组件管理硬件，支持符合以下条件的开发板：

- Flash >= 16MB
- PSRAM >= 8MB
- 支持以下接口：

  - `AudioCodecPlayer`
  - `AudioCodecRecorder`
  - `StorageFs`

请参考 [ESP-Brookesia 编程指南 - 支持的开发板](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/hal/boards/index.html#hal-boards-sec-02) 获取支持的开发板列表。

### 开发环境

请参考以下文档：

- [ESP-Brookesia 编程指南 - 版本说明](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia 编程指南 - 开发环境搭建](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-dev-environment)

## 🔨 如何使用

请参考 [ESP-Brookesia 编程指南 - 如何使用示例工程](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-example-projects)。

## 🚀 运行说明

示例烧录后，程序将按顺序自动执行以下演示场景，所有输出通过串口打印。

### 音频文件播放

示例预置了若干 MP3 音频文件于 SPIFFS 分区，演示两种播放模式：

**单文件播放**：依次播放三个文件，第二次播放使用 `interrupt: false` 追加到队列，第三次播放使用默认的打断模式：

```
[Demo: Play Single URL]
  播放 1.mp3
  播放 2.mp3（追加，循环 5 次）
  播放 3.mp3（打断前一个播放）
```

**多文件播放**：将多个 URL 合并为一个播放列表，支持延迟启动、循环与指定超时：

```
[Demo: Play Multiple URLs]
  播放列表 [4.mp3, non_exist.mp3]（不存在的文件自动跳过）
  播放列表 [5.mp3, 6.mp3]（延迟 2s 后打断，循环 2 次）
```

两个场景均通过事件订阅实时打印播放状态（`PlayStateChanged` 事件）。

### 播放控制

演示播放过程中的暂停、恢复、停止操作：

```
[Demo: Play Control]
  开始播放 7.mp3（循环 10 次）
  3s 后暂停
  3s 后恢复
  3s 后停止
```

### 编解码回环测试

依次测试 PCM、OPUS、G711A（非 ESP32-C5）三种格式，每种格式流程如下：

```
[Demo: Encode → Decode (Format: <format>)]
  启动编码器，录制麦克风 10 秒（请对麦克风说话）
  停止编码器，输出录制字节数
  启动解码器，分块回放录制音频
  停止解码器
```

> [!NOTE]
> 若录制期间无声音输入，将跳过解码回放步骤。

### AFE 音频前端

演示 VAD 与唤醒词检测：

```
[Demo: AFE]
  配置 AFE（启用 VAD + WakeNet 默认参数）
  启动编码器，使能 AFE 处理
  监听 AFE 事件 120 秒（请说出唤醒词并制造噪音）
  停止编码器
```

运行期间，每次触发 VAD 或唤醒词事件都会通过串口打印对应的事件名称。

## 🔍 故障排除

**无声音输出**

- 确认 SPIFFS 分区已成功烧录（`idf.py flash` 包含分区表和分区内容）。

**编解码回环无声音**

- 确认麦克风正常工作，录制期间对着麦克风说话或制造声音。
- 检查串口日志中的"recorded X bytes"，若为 0 说明录音未成功。

**AFE 无事件触发**

- 确认 `model` 分区已成功烧录唤醒词模型。
- 靠近麦克风说出完整唤醒词，避免背景噪音过大。

**编译失败（VSCode）**

使用命令行安装 ESP-IDF，参考 [开发环境](#开发环境)。

## 💬 技术支持与反馈

请通过以下渠道进行反馈：

- 有关技术问题，请访问 [esp32.com](https://esp32.com/viewforum.php?f=52&sid=86e7d3b29eae6d591c965ec885874da6) 论坛
- 有关功能请求或错误报告，请创建新的 [GitHub 问题](https://github.com/espressif/esp-brookesia/issues)

我们会尽快回复。
