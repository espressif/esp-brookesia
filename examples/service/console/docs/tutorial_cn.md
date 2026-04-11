# 快速入门教程

本教程以运行 XiaoZhi Agent 为例，介绍如何使用服务控制台示例。

## 目录

- [快速入门教程](#快速入门教程)
  - [目录](#目录)
  - [启动设备](#启动设备)
  - [第一步：连接 WiFi](#第一步连接-wifi)
  - [第二步：启动 XiaoZhi Agent](#第二步启动-xiaozhi-agent)
  - [第三步：开始对话](#第三步开始对话)
  - [常用操作命令](#常用操作命令)
  - [播放音频（可选）](#播放音频可选)

## 启动设备

设备成功启动后，您敲击回车键将看到控制台提示符 `esp32s3>`、`esp32p4>` 或 `esp32c5>`，此时可以开始输入命令。

## 第一步：连接 WiFi

XiaoZhi Agent 需要联网才能使用，首先需要连接 WiFi。

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

## 第二步：启动 XiaoZhi Agent

1. **订阅激活码事件**：

```bash
svc_subscribe AgentXiaoZhi ActivationCodeReceived
```

>[!NOTE]
> 此时 LOG 会出现 `Service is not bindable: AgentXiaoZhi` 错误，属于正常现象，请忽略此错误。

2. **激活 XiaoZhi Agent**：

```bash
svc_call AgentManager ActivateAgent {"Name":"AgentXiaoZhi"}
```

此时 LOG 会提示输入设备激活码，请在 PC 端将激活码输入到 [XiaoZhi 控制台](https://xiaozhi.me/console/agents)，设备会在一段时间后自动获取认证。

等待看到 `No activation code or challenge found, activate successfully` 日志表示激活码事件已订阅成功。

3. **启动 Agent**：

```bash
svc_call AgentManager TriggerGeneralAction {"Action":"Start"}
```

## 第三步：开始对话

现在您可以通过语音与 XiaoZhi Agent 进行对话了。

> [!TIP]
> 您可以通过唤醒词（默认为 "Hi,ESP"）打断 Agent 说话或将 Agent 从休眠状态唤醒。

## 常用操作命令

在对话过程中，您可以使用以下命令进行控制：

| 操作 | 命令 |
|------|------|
| 打断 Agent 说话 | `svc_call AgentManager InterruptSpeaking` |
| 让 Agent 休眠 | `svc_call AgentManager TriggerGeneralAction {"Action":"Sleep"}` |
| 唤醒 Agent | `svc_call AgentManager TriggerGeneralAction {"Action":"WakeUp"}` |
| 停止 Agent | `svc_call AgentManager TriggerGeneralAction {"Action":"Stop"}` |
| 查看 Agent 状态 | `svc_call AgentManager GetGeneralState` |

## 播放音频（可选）

您也可以使用音频服务播放本地或网络音频：

```bash
# 播放本地音频
svc_call Audio PlayUrl {"Url":"file://spiffs/example.mp3"}

# 播放网络音频
svc_call Audio PlayUrl {"Url":"https://dl.espressif.com/AE/esp-brookesia/example.mp3"}

# 调节音量
svc_call Audio SetVolume {"Volume":80}
```
