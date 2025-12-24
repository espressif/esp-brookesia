# Audio Service

The Audio service provides audio playback control functionality, supporting playback of local files and network audio resources.

Detailed interface descriptions please refer to [Audio service helper header file](https://github.com/espressif/esp-brookesia/blob/master/service/brookesia_service_helper/include/brookesia/service_helper/audio.hpp).

## Table of Contents

- [Audio Service](#audio-service)
  - [Table of Contents](#table-of-contents)
  - [Play Audio](#play-audio)
  - [Playback Control](#playback-control)
  - [Set Volume](#set-volume)
  - [Subscribe to Playback State Events](#subscribe-to-playback-state-events)
  - [Notes](#notes)

## Play Audio

Play audio files (supports local files and network URLs):

```bash
svc_call Audio PlayUrl {"Url":"file://spiffs/example.mp3"}
```

Play network audio:

```bash
svc_call Audio PlayUrl {"Url":"https://dl.espressif.com/AE/esp-brookesia/example.mp3"}
```

## Playback Control

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

## Set Volume

Set volume (0-100):

```bash
svc_call Audio SetVolume {"Volume":90}
```

## Subscribe to Playback State Events

Subscribe to playback state change events:

```bash
svc_subscribe Audio PlayStateChanged
```

You will receive event notifications when the playback state changes.

## Notes

- Local files need to be saved in the [spiffs](../spiffs/) directory, and the file path format is: `file://spiffs/filename`
- Network audio URLs must be valid HTTP or HTTPS URLs, for example: `https://dl.espressif.com/AE/esp-brookesia/example.mp3`
