# Audio Service

The Audio service provides audio playback control, encoding/decoding, and AFE (Audio Front End) functionality, supporting playback of local files and network audio resources.

For detailed interface documentation, please refer to [Audio Service Helper Header File](https://github.com/espressif/esp-brookesia/blob/master/service/brookesia_service_helper/include/brookesia/service_helper/audio.hpp).

> [!NOTE]
> - Audio service requires supported development boards, please refer to [Prerequisites](../README.md#prerequisites) for the list of supported boards
> - Local files need to be saved in the [spiffs](../spiffs/) directory, file path format: `file://spiffs/filename`

## Table of Contents

- [Audio Service](#audio-service)
  - [Table of Contents](#table-of-contents)
  - [Function Call Interface](#function-call-interface)
    - [Play Audio](#play-audio)
    - [Play Multiple Audio Files](#play-multiple-audio-files)
    - [Playback Control](#playback-control)
    - [Set Volume](#set-volume)
    - [Get Volume](#get-volume)
    - [Encoder Control](#encoder-control)
    - [Decoder Control](#decoder-control)
  - [Event Subscription Interface](#event-subscription-interface)
    - [Playback State Changed Event](#playback-state-changed-event)
    - [AFE Event](#afe-event)

## Function Call Interface

### Play Audio

Play audio file (supports local files and network URLs):

```bash
svc_call Audio PlayUrl {"Url":"file://spiffs/example.mp3"}
```

Play network audio:

```bash
svc_call Audio PlayUrl {"Url":"https://dl.espressif.com/AE/esp-brookesia/example.mp3"}
```

Play audio file with playback configuration:

```bash
svc_call Audio PlayUrl {"Url":"file://spiffs/example.mp3","Config":{"interrupt":false,"delay_ms":1000,"loop_count":3,"loop_interval_ms":1000,"timeout_ms":10000}}
```

Configuration description:

- `interrupt`: Whether to interrupt current playback. If `false`, the audio file will be added to the playback queue and played automatically after the current playback completes
- `delay_ms`: Delay time before playback in milliseconds
- `loop_count`: Number of loop iterations. If `0`, no loop playback
- `loop_interval_ms`: Interval between loops in milliseconds
- `timeout_ms`: Playback timeout in milliseconds. Calculated from playback start (excluding delay and pause time), playback stops after this time

### Play Multiple Audio Files

Play multiple audio files:

```bash
svc_call Audio PlayUrls {"Urls":["file://spiffs/1.mp3","file://spiffs/2.mp3","file://spiffs/3.mp3"]}
```

Play multiple audio files with playback configuration:

```bash
svc_call Audio PlayUrls {"Urls":["file://spiffs/1.mp3","file://spiffs/2.mp3"],"Config":{"interrupt":true}}
```

### Playback Control

Pause playback:

```bash
svc_call Audio PlayControl {"Action":"Pause"}
```

Resume playback:

```bash
svc_call Audio PlayControl {"Action":"Resume"}
```

Stop playback:

```bash
svc_call Audio PlayControl {"Action":"Stop"}
```

### Set Volume

Set volume (0-100):

```bash
svc_call Audio SetVolume {"Volume":90}
```

### Get Volume

Get current volume:

```bash
svc_call Audio GetVolume
```

### Encoder Control

Start encoder:

```bash
svc_call Audio StartEncoder {"Config":{"type":"OPUS","general":{"channels":1,"sample_bits":16,"sample_rate":16000,"frame_duration":60}}}
```

Configuration description:

- `type`: Encoding format, valid values are `PCM`, `OPUS`, `G711A`
- `general`: General configuration
  - `channels`: Number of channels (1-4)
  - `sample_bits`: Sample bit depth (e.g., 8, 16, 24, 32)
  - `sample_rate`: Sample rate (e.g., 8000, 16000, 24000, 32000, 44100, 48000)
  - `frame_duration`: Frame duration in milliseconds

Stop encoder:

```bash
svc_call Audio StopEncoder
```

### Decoder Control

Start decoder:

```bash
svc_call Audio StartDecoder {"Config":{"type":"OPUS","general":{"channels":1,"sample_bits":16,"sample_rate":16000,"frame_duration":60}}}
```

Stop decoder:

```bash
svc_call Audio StopDecoder
```

## Event Subscription Interface

### Playback State Changed Event

Subscribe to playback state change events:

```bash
svc_subscribe Audio PlayStateChanged
```

Event parameters:

- `State`: Playback state, valid values are `Idle`, `Playing`, `Paused`

### AFE Event

Subscribe to AFE (Audio Front End) events:

```bash
svc_subscribe Audio AFE_EventHappened
```

Event parameters:

- `Event`: AFE event type, valid values are
  - `VAD_Start`: Voice Activity Detection started
  - `VAD_End`: Voice Activity Detection ended
  - `WakeStart`: Wake word detection started
  - `WakeEnd`: Wake word detection ended
