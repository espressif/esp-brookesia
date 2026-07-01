# 快速入门教程

本教程通过连接 WiFi、加载表情资源和播放音频，介绍服务控制台示例的常用流程。

## 目录

- [快速入门教程](#快速入门教程)
  - [目录](#目录)
  - [启动设备](#启动设备)
  - [第一步：连接 WiFi](#第一步连接-wifi)
  - [第二步：启动 Expression Emote](#第二步启动-expression-emote)
  - [第三步：播放音频](#第三步播放音频)

## 启动设备

设备成功启动后，您敲击回车键将看到控制台提示符 `esp32s3>`、`esp32p4>` 或 `esp32c5>`，此时可以开始输入命令。

## 第一步：连接 WiFi

网络 RPC 或网络音频播放等命令需要 WiFi 连接，首先需要连接 WiFi。

1. **设置要连接的 WiFi 信息**：

```bash
svc_call Wifi SetConnectAp {"SSID":"ssid","Password":"password"}
```

请将 `ssid` 和 `password` 替换为实际的 WiFi 名称和密码。

2. **连接 WiFi**：

```bash
svc_call Wifi TriggerGeneralAction {"Action":"Connect"}
```

等待看到 `Detected expected event: Connected` 日志表示连接成功。

> [!TIP]
> 更多 `Wifi` 相关命令，请参考 [WiFi - 服务接口](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/service/wifi.html#helper-contract-service-wifi-interfaces)。

## 第二步：启动 Expression Emote

1. **开启屏幕背光**：

```bash
svc_call Display SetBacklightOnOff {"OutputId":1,"On":true}
```

> [!TIP]
> 如需确认当前输出 ID，可先执行 `svc_call Display GetOutputs`。
> 更多 `Display` 相关命令，请参考 [Display 服务 README](../../../../service/media/brookesia_service_display/README_CN.md)。

2. **加载表情资源**：

```bash
svc_call Emote LoadAssetsSource {"Source":{"source":"anim_icon","type":"PartitionLabel","flag_enable_mmap":false}}
```

3. **显示一个表情**：

```bash
svc_call Emote SetEmoji {"Emoji":"happy"}
```

此时屏幕上应显示 `happy` 表情。可用的其它表情名称包括 `sad`、`angry`、`thinking`、`winking` 和 `idle`。

> [!TIP]
> 更多 `Emote` 相关命令，请参考 [表情 - 服务接口](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/expression/emote.html#helper-contract-service-emote-interfaces)。

## 第三步：播放音频

1. **关闭音频静音**：

```bash
svc_call AudioPlayback SetMute {"Enable":false}
```

2. **订阅播放状态事件**：

```bash
svc_subscribe AudioPlayback PlayStateChanged
```

3. **播放本地或网络音频**：

```bash
# 播放本地音频
svc_call AudioPlayback Play {"Url":"file://littlefs/example.mp3"}

# 播放网络音频
svc_call AudioPlayback Play {"Url":"https://dl.espressif.com/AE/esp-brookesia/example.mp3"}

# 调节音量
svc_call AudioPlayback SetVolume {"Volume":80}
```

> [!TIP]
> 更多 `Audio` 相关命令，请参考 [音频 - 服务接口](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/service/audio.html#helper-contract-service-audio-interfaces)。
