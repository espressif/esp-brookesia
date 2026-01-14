# Emote Service

The Emote service provides functionality for displaying expressions, animations, and event messages.

For detailed interface documentation, please refer to the [Emote Service Helper Header File](https://github.com/espressif/esp-brookesia/blob/master/service/brookesia_service_helper/include/brookesia/service_helper/expression/emote.hpp).

## Table of Contents

- [Emote Service](#emote-service)
  - [Table of Contents](#table-of-contents)
  - [Set Emoji](#set-emoji)
  - [Set Animation](#set-animation)
  - [Insert Animation](#insert-animation)
  - [Stop Animation](#stop-animation)
  - [Wait for Animation Frame Completion](#wait-for-animation-frame-completion)
  - [Set Event Message](#set-event-message)
  - [Load Assets](#load-assets)

## Set Emoji

Set an emoji:

```bash
svc_call Emote SetEmoji {"Emoji":"angry"}
```

Parameter description:

- `Emoji`: Emoji name. For 360x360 image resources, please refer to the `emote` field in the [360_360/emote.json](https://github.com/espressif2022/esp_emote_assets/blob/main/360_360/emote.json) file.

## Set Animation

Set an animation:

```bash
svc_call Emote SetAnimation {"Animation":"angry"}
```

Parameter description:

- `Animation`: Animation name

## Insert Animation

Insert an animation into the queue. The animation will automatically stop after the specified duration:

```bash
svc_call Emote InsertAnimation {"Animation":"angry","DurationMs":3000}
```

Parameter description:

- `Animation`: Animation name
- `DurationMs`: Animation duration in milliseconds

## Stop Animation

Stop the currently playing animation:

```bash
svc_call Emote StopAnimation
```

## Wait for Animation Frame Completion

Wait for an animation frame to complete:

```bash
svc_call Emote WaitAnimationFrameDone {"TimeoutMs":3000}
```

Parameter description:

- `TimeoutMs`: Wait time in milliseconds. 0 means wait forever

## Set Event Message

Set a message for a specified event:

```bash
svc_call Emote SetEventMessage {"Event":"Idle"}
svc_call Emote SetEventMessage {"Event":"Speak"}
svc_call Emote SetEventMessage {"Event":"Listen"}
svc_call Emote SetEventMessage {"Event":"System","Message":"system_message"}
svc_call Emote SetEventMessage {"Event":"User","Message":"user_message"}

// Battery information is only displayed in `Idle` state
// Set battery message, format: "charging,percent" e.g. "1,75" or "0,30"
svc_call Emote SetEventMessage {"Event":"Battery","Message":"1,75"}
svc_call Emote SetEventMessage {"Event":"Battery","Message":"0,30"}
```

Event type description:

- `Idle`: Idle state
- `Speak`: Speaking state
- `Listen`: Listening state
- `System`: System event
- `User`: User event
- `Battery`: Battery event

Parameter description:

- `Event`: Event type. Valid values: `Idle`, `Speak`, `Listen`, `System`, `User`, `Battery`
- `Message`: Message content (optional, defaults to empty string)

## Load Assets

Load from partition label:

```bash
svc_call Emote LoadAssetsSource {"Source":{"source":"anim_icon","type":"PartitionLabel","flag_enable_mmap":false}}
```

Or load assets from file path:

```bash
svc_call Emote LoadAssetsSource {"Source":{"source":"/spiffs/anim_icon.bin","type":"Path","flag_enable_mmap":false}}
```

Parameter description:

- `source`: Source path or partition label name
- `type`: Source type. Valid values: `Path` (file path) or `PartitionLabel` (partition label)
- `flag_enable_mmap`: Whether to enable memory mapping. Valid values: `true` or `false` (default is `false`).

> [!WARNING]
> - The current example loads assets from partition label `anim_icon` by default.
> - When `flag_enable_mmap` is `true` and animation is running, do not operate the file system, otherwise it will cause program exceptions.
