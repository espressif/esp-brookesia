# Quick Start Tutorial

This tutorial uses running the XiaoZhi Agent as an example to show how to use the service console example.

## Table of Contents

- [Quick Start Tutorial](#quick-start-tutorial)
  - [Table of Contents](#table-of-contents)
  - [Boot the Device](#boot-the-device)
  - [Step 1: Connect to Wi-Fi](#step-1-connect-to-wi-fi)
  - [Step 2: Start Expression Emote](#step-2-start-expression-emote)
  - [Step 3: Start the XiaoZhi Agent](#step-3-start-the-xiaozhi-agent)
  - [Step 4: Start a Conversation](#step-4-start-a-conversation)
  - [Common Control Commands](#common-control-commands)
  - [Play Audio (Optional)](#play-audio-optional)

## Boot the Device

Once the device boots successfully, press Enter to see the console prompt `esp32s3>`, `esp32p4>`, or `esp32c5>`. You can then start entering commands.

## Step 1: Connect to Wi-Fi

The XiaoZhi Agent requires a network connection. Connect to Wi-Fi first.

1. **Set the Wi-Fi credentials**:

```bash
svc_call Wifi SetConnectAp {"SSID":"ssid","Password":"password"}
```

Replace `ssid` and `password` with your actual Wi-Fi name and password.

2. **Connect to Wi-Fi**:

```bash
svc_call Wifi TriggerGeneralAction {"Action":"Connect"}
```

Wait until you see the log `Detected expected event: Connected`, which indicates a successful connection.

> [!TIP]
> For more `Wifi` commands, see [WiFi - Service Interfaces](https://docs.espressif.com/projects/esp-brookesia/en/latest/service/wifi.html#helper-contract-service-wifi-interfaces).

## Step 2: Start Expression Emote

1. **Load the emote assets**:

```bash
svc_call Emote LoadAssetsSource {"Source":{"source":"anim_icon","type":"PartitionLabel","flag_enable_mmap":false}}
```

> [!TIP]
> For more `Emote` commands, see [Emote - Service Interfaces](https://docs.espressif.com/projects/esp-brookesia/en/latest/expression/emote.html#helper-contract-service-emote-interfaces).

## Step 3: Start the XiaoZhi Agent

1. **Subscribe to the activation code event**:

```bash
svc_subscribe AgentXiaoZhi ActivationCodeReceived
```

> [!NOTE]
> The log may show a `Service is not bindable: AgentXiaoZhi` error at this point. This is expected — please ignore it.
> For more `AgentXiaoZhi` commands, see [XiaoZhi - Service Interfaces](https://docs.espressif.com/projects/esp-brookesia/en/latest/agent/xiaozhi.html#helper-contract-agent-xiaozhi-interfaces).

2. **Activate the Agent**:

```bash
svc_call AgentManager SetTargetAgent {"Name":"AgentXiaoZhi"}
svc_call AgentManager TriggerGeneralAction {"Action":"Activate"}
```

The log will prompt you to enter the device activation code. Enter the code in the [XiaoZhi Console](https://xiaozhi.me/console/agents) on your PC. The device will obtain authentication automatically after a short while.

Wait until you see the log `No activation code or challenge found, activate successfully`, which indicates that the activation code event has been subscribed successfully.

3. **Start the Agent**:

```bash
svc_call AgentManager TriggerGeneralAction {"Action":"Start"}
```

> [!TIP]
> For more `AgentManager` commands, see [Agent Manager - Service Interfaces](https://docs.espressif.com/projects/esp-brookesia/en/latest/agent/manager/index.html#helper-contract-agent-manager-interfaces).

## Step 4: Start a Conversation

You can now have a voice conversation with the XiaoZhi Agent.

> [!TIP]
> You can use the wake word (default: "Hi, ESP") to interrupt the Agent while it is speaking or to wake it from sleep.

## Common Control Commands

During a conversation, use the following commands to control the Agent:

| Action | Command |
|--------|---------|
| Interrupt the Agent | `svc_call AgentManager InterruptSpeaking` |
| Put the Agent to sleep | `svc_call AgentManager TriggerGeneralAction {"Action":"Sleep"}` |
| Wake up the Agent | `svc_call AgentManager TriggerGeneralAction {"Action":"WakeUp"}` |
| Stop the Agent | `svc_call AgentManager TriggerGeneralAction {"Action":"Stop"}` |
| Check the Agent state | `svc_call AgentManager GetGeneralState` |

## Play Audio (Optional)

You can also use the audio service to play local or network audio:

```bash
# Play local audio
svc_call Audio PlayUrl {"Url":"file://spiffs/example.mp3"}

# Play network audio
svc_call Audio PlayUrl {"Url":"https://dl.espressif.com/AE/esp-brookesia/example.mp3"}

# Adjust volume
svc_call Audio SetVolume {"Volume":80}
```

> [!TIP]
> For more `Audio` commands, see [Audio - Service Interfaces](https://docs.espressif.com/projects/esp-brookesia/en/latest/service/audio.html#helper-contract-service-audio-interfaces).
