# 音频服务示例

[English Version](./README.md)

本示例演示了如何基于 ESP-Brookesia 框架使用音频服务（`brookesia_service_audio`），涵盖音频文件播放、播放控制、音量管理、编解码回环测试以及 AFE（音频前端）语音活动检测与唤醒词识别等功能。

## 📑 目录

- [音频服务示例](#音频服务示例)
  - [📑 目录](#-目录)
  - [✨ 功能特性](#-功能特性)
  - [🚩 快速入门](#-快速入门)
    - [硬件要求](#硬件要求)
    - [开发环境](#开发环境)
  - [🔨 编译与烧录](#-编译与烧录)
  - [🚀 运行说明](#-运行说明)
    - [音频文件播放](#音频文件播放)
    - [播放控制](#播放控制)
    - [音量管理](#音量管理)
    - [编解码回环测试](#编解码回环测试)
    - [AFE 音频前端](#afe-音频前端)
  - [🔍 故障排除](#-故障排除)
  - [💬 技术支持与反馈](#-技术支持与反馈)

## ✨ 功能特性

- 🎵 **音频文件播放**：支持播放单个或多个 URL（本地 SPIFFS / 网络），可配置循环次数、队列追加或打断当前播放
- ⏯️ **播放控制**：支持暂停、恢复、停止操作，并通过事件订阅实时获取播放状态变化
- 🔊 **音量管理**：运行时获取与设置播放音量（0–100），音量变更通过 NVS 自动持久化
- 🎙️ **编解码回环测试**：启动编码器录制麦克风输入，完成后通过解码器回放，支持 PCM、OPUS、G711A 三种格式
- 🧠 **AFE 音频前端**：集成 VAD（语音活动检测）与 WakeNet（唤醒词识别），通过事件订阅实时响应语音与唤醒事件

## 🚩 快速入门

### 硬件要求

搭载 `ESP32-S3`、`ESP32-P4` 或 `ESP32-C5` 芯片，`Flash >= 16MB, PSRAM >= 4MB`，并具备麦克风和扬声器的开发板。

本示例通过 `esp_board_manager` 组件管理硬件，官方支持以下开发板：

- ESP-VoCat V1.0
- ESP-VoCat V1.2
- ESP32-S3-BOX-3
- ESP32-S3-Korvo-2 V3
- ESP32-P4-Function-EV-Board
- ESP32-C5-Sensair-Shuttle

### 开发环境

- ESP-IDF `v5.5.2` TAG（推荐）或 `release/v5.5` 分支

> [!WARNING]
> 不推荐使用 VSCode 扩展插件安装 ESP-IDF 环境，可能导致编译失败。请参照 [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html) 设置开发环境。

## 🔨 编译与烧录

**1. 选择开发板**

```bash
idf.py gen-bmgr-config -b <board>
```

可选的 `<board>` 值包括：

- `esp_vocat_board_v1_0`
- `esp_vocat_board_v1_2`
- `esp_box_3`
- `esp32_s3_korvo2_v3`
- `esp32_p4_function_ev`
- `esp_sensair_shuttle`

> [!NOTE]
> 如需添加自定义开发板，请参考 [esp_board_manager 组件文档](https://github.com/espressif/esp-gmf/blob/main/packages/esp_board_manager/README_CN.md)。

**2. 编译烧录**

```bash
idf.py -p PORT build flash monitor
```

按 `Ctrl-]` 退出串口监视。

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

### 音量管理

演示音量的读取、修改与验证：

```
[Demo: Volume Control]
  读取当前音量
  设置音量为 90，验证生效
  播放 8.mp3、9.mp3
  恢复原始音量并验证
```

> [!NOTE]
> 音量变更通过 NVS 自动持久化，断电后保留。

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

- 检查扬声器连接与音量设置，确认音量大于 0。
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
