# Emote Service

The Emote service provides functionality for displaying expressions, animations, and event messages.

For detailed interface documentation, please refer to [Emote Service Helper Header File](https://github.com/espressif/esp-brookesia/blob/master/service/brookesia_service_helper/include/brookesia/service_helper/expression/emote.hpp).

> [!NOTE]
> - Emote service requires supported development boards, please refer to [Prerequisites](../README.md#prerequisites) for the list of supported boards
> - For expression and animation resources, please refer to [esp_emote_assets](https://github.com/espressif2022/esp_emote_assets)

## Table of Contents

- [Emote Service](#emote-service)
  - [Table of Contents](#table-of-contents)
  - [Function Call Interface](#function-call-interface)
    - [Load Assets](#load-assets)
    - [Emoji](#emoji)
      - [Show Emoji](#show-emoji)
      - [Hide Emoji](#hide-emoji)
    - [Animation](#animation)
      - [Set Animation](#set-animation)
      - [Insert Animation](#insert-animation)
      - [Stop Animation](#stop-animation)
      - [Wait for Animation Frame Completion](#wait-for-animation-frame-completion)
    - [Event Message](#event-message)
      - [Set Event Message](#set-event-message)
      - [Hide Event Message](#hide-event-message)
    - [QR Code](#qr-code)
      - [Show QR Code](#show-qr-code)
      - [Hide QR Code](#hide-qr-code)
  - [Event Subscription Interface](#event-subscription-interface)
    - [Flush Ready Event](#flush-ready-event)

## Function Call Interface

### Load Assets

Load assets from partition label:

```bash
svc_call Emote LoadAssetsSource {"Source":{"source":"anim_icon","type":"PartitionLabel","flag_enable_mmap":false}}
```

Load assets from file path:

```bash
svc_call Emote LoadAssetsSource {"Source":{"source":"/spiffs/anim_icon.bin","type":"Path","flag_enable_mmap":false}}
```

Parameter description:

- `source`: Source path or partition label name
- `type`: Source type, valid values are `Path` (file path) or `PartitionLabel` (partition label)
- `flag_enable_mmap`: Whether to enable memory mapping, valid values are `true` or `false` (default is `false`)

> [!WARNING]
> - The current example loads assets from partition label `anim_icon` by default
> - When `flag_enable_mmap` is `true` and animation is running, do not operate the file system, otherwise it will cause program exceptions

### Emoji

#### Show Emoji

```bash
svc_call Emote SetEmoji {"Emoji":"angry"}
```

Parameter description:

- `Emoji`: Emoji name, please refer to the `emote` field in [360_360/emote.json](https://github.com/espressif2022/esp_emote_assets/blob/main/360_360/emote.json)

> [!NOTE]
> When showing emoji, animation will be hidden immediately

#### Hide Emoji

```bash
svc_call Emote HideEmoji
```

### Animation

#### Set Animation

```bash
svc_call Emote SetAnimation {"Animation":"winking"}
```

Parameter description:

- `Animation`: Animation name

> [!NOTE]
> When setting animation, emoji will be hidden immediately

#### Insert Animation

Insert an animation into the queue, the animation will automatically stop after the specified duration:

```bash
svc_call Emote InsertAnimation {"Animation":"sleepy","DurationMs":3000}
```

Parameter description:

- `Animation`: Animation name
- `DurationMs`: Animation duration in milliseconds, automatically stops after expiration

#### Stop Animation

```bash
svc_call Emote StopAnimation
```

#### Wait for Animation Frame Completion

Wait for each animation frame to complete:

```bash
svc_call Emote WaitAnimationFrameDone {"TimeoutMs":3000}
```

Parameter description:

- `TimeoutMs`: Wait timeout in milliseconds, `0` means wait forever

### Event Message

#### Set Event Message

Set idle state message:

```bash
svc_call Emote SetEventMessage {"Event":"Idle"}
```

Set speaking state message:

```bash
svc_call Emote SetEventMessage {"Event":"Speak"}
```

Set listening state message:

```bash
svc_call Emote SetEventMessage {"Event":"Listen"}
```

Set system message:

```bash
svc_call Emote SetEventMessage {"Event":"System","Message":"System message content"}
```

Set user message:

```bash
svc_call Emote SetEventMessage {"Event":"User","Message":"User message content"}
```

Set battery status message:

```bash
# Format: "charging,percent", e.g., "1,75" means charging, 75% battery
svc_call Emote SetEventMessage {"Event":"Battery","Message":"1,75"}
# "0,30" means not charging, 30% battery
svc_call Emote SetEventMessage {"Event":"Battery","Message":"0,30"}
```

Parameter description:

- `Event`: Event type, valid values are
  - `Idle`: Idle state
  - `Speak`: Speaking state
  - `Listen`: Listening state
  - `System`: System message
  - `User`: User message
  - `Battery`: Battery status (format: `"charging_state,battery_percentage"`)
- `Message`: Message content (optional, defaults to empty string)

> [!NOTE]
> Battery information is only displayed in `Idle` state

#### Hide Event Message

```bash
svc_call Emote HideEventMessage
```

### QR Code

#### Show QR Code

```bash
svc_call Emote SetQrcode {"Qrcode":"https://www.espressif.com"}
```

Parameter description:

- `Qrcode`: QR code content

> [!NOTE]
> When showing QR code, emoji and animation will be hidden immediately

#### Hide QR Code

```bash
svc_call Emote HideQrcode
```

> [!NOTE]
> After hiding QR code, emoji will be shown immediately

## Event Subscription Interface

### Flush Ready Event

Subscribe to flush ready event (for display driver callback):

```bash
svc_subscribe Emote FlushReady
```

Event parameters:

- `Param`: Flush parameter object, containing
  - `x_start`: Start X coordinate
  - `y_start`: Start Y coordinate
  - `x_end`: End X coordinate
  - `y_end`: End Y coordinate
  - `data`: Display data pointer

> [!NOTE]
> This event is typically used for low-level display driver integration, regular users do not need to subscribe
