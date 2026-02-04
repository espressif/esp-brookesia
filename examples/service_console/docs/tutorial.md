# Quick Start Tutorial

This tutorial demonstrates how to use the service console example with XiaoZhi Agent.

## Table of Contents

- [Quick Start Tutorial](#quick-start-tutorial)
  - [Table of Contents](#table-of-contents)
  - [Boot Device](#boot-device)
  - [Step 1: Connect to WiFi](#step-1-connect-to-wifi)
  - [Step 2: Start XiaoZhi Agent](#step-2-start-xiaozhi-agent)
  - [Step 3: Start Conversation](#step-3-start-conversation)
  - [Common Commands](#common-commands)
  - [Play Audio (Optional)](#play-audio-optional)

## Boot Device

After the device boots successfully, you will see the console prompt `esp32s3>` or `esp32p4>`, indicating that you can start entering commands.

## Step 1: Connect to WiFi

XiaoZhi Agent requires internet access. First, connect to WiFi.

1. **Set WiFi credentials**:

```bash
svc_call Wifi SetConnectAp {"SSID":"ssid","Password":"password"}
```

Replace `ssid` and `password` with your actual WiFi name and password.

2. **Connect to WiFi**:

```bash
svc_call Wifi TriggerGeneralAction {"Action":"Connect"}
```

Wait for the `WiFi Connect finished` log to indicate successful connection.

> [!TIP]
> For detailed WiFi commands, refer to [WiFi Service Documentation](./cmd_wifi.md).

## Step 2: Start XiaoZhi Agent

1. **Activate XiaoZhi Agent**:

```bash
svc_call AgentManager ActivateAgent {"Name":"AgentXiaoZhi"}
```

The Agent will display a device activation code. Enter this code on the [XiaoZhi Console](https://xiaozhi.me/console/agents) from your PC. The device will automatically authenticate after a short while.

2. **Start the Agent**:

```bash
svc_call AgentManager TriggerGeneralAction {"Action":"Start"}
```

## Step 3: Start Conversation

Now you can have voice conversations with XiaoZhi Agent.

> [!TIP]
> You can use the wake word (default: "Hi,ESP") to interrupt the Agent while speaking or wake it up from sleep mode.

## Common Commands

During conversation, you can use the following commands for control:

| Action | Command |
|--------|---------|
| Interrupt Agent speaking | `svc_call AgentManager InterruptSpeaking` |
| Put Agent to sleep | `svc_call AgentManager TriggerGeneralAction {"Action":"Sleep"}` |
| Wake up Agent | `svc_call AgentManager TriggerGeneralAction {"Action":"WakeUp"}` |
| Stop Agent | `svc_call AgentManager TriggerGeneralAction {"Action":"Stop"}` |
| Check Agent status | `svc_call AgentManager GetGeneralState` |

> [!TIP]
> For detailed Agent commands, refer to [Agent Service Documentation](./cmd_agent.md).

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
> For detailed audio commands, refer to [Audio Service Documentation](./cmd_audio.md).
