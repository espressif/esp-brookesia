# Quick Start Tutorial

This tutorial walks through common service-console workflows: connecting Wi-Fi, loading emote assets, and playing audio.

## Table of Contents

- [Quick Start Tutorial](#quick-start-tutorial)
  - [Table of Contents](#table-of-contents)
  - [Boot the Device](#boot-the-device)
  - [Step 1: Connect to Wi-Fi](#step-1-connect-to-wi-fi)
  - [Step 2: Start Expression Emote](#step-2-start-expression-emote)
  - [Step 3: Play Audio](#step-3-play-audio)

## Boot the Device

Once the device boots successfully, press Enter to see the console prompt `esp32s3>`, `esp32p4>`, or `esp32c5>`. You can then start entering commands.

## Step 1: Connect to Wi-Fi

Network audio playback requires a Wi-Fi connection. Connect to Wi-Fi first.

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

1. **Turn on the display backlight**:

```bash
svc_call Display SetBacklightOnOff {"OutputId":1,"On":true}
```

> [!TIP]
> Use `svc_call Display GetOutputs` to check the current output id if needed.
> For more `Display` commands, see the [Display service README](../../../../service/media/brookesia_service_display/README.md).

2. **Load the emote assets**:

```bash
svc_call Emote LoadAssetsSource {"Source":{"source":"anim_icon","type":"PartitionLabel","flag_enable_mmap":false}}
```

3. **Display an emote emoji**:

```bash
svc_call Emote SetEmoji {"Emoji":"happy"}
```

You should now see the `happy` emote on the display. Other available emote names include `sad`, `angry`, `thinking`, `winking`, and `idle`.

> [!TIP]
> For more `Emote` commands, see [Emote - Service Interfaces](https://docs.espressif.com/projects/esp-brookesia/en/latest/service/expression/emote.html#helper-contract-service-emote-interfaces).

## Step 3: Play Audio

1. **Disable audio mute**:

```bash
svc_call AudioPlayback SetMute {"Enable":false}
```

2. **Subscribe to playback-state changes**:

```bash
svc_subscribe AudioPlayback PlayStateChanged
```

3. **Play local or network audio**:

```bash
# Play local audio
svc_call AudioPlayback Play {"Url":"file://littlefs/example.mp3"}

# Play network audio
svc_call AudioPlayback Play {"Url":"https://dl.espressif.com/AE/esp-brookesia/example.mp3"}

# Adjust volume
svc_call AudioPlayback SetVolume {"Volume":80}
```

> [!TIP]
> For more `Audio` commands, see [Audio - Service Interfaces](https://docs.espressif.com/projects/esp-brookesia/en/latest/service/audio.html#helper-contract-service-audio-interfaces).
