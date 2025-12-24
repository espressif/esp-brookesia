# 音频服务

音频服务提供了音频播放控制功能，支持播放本地文件和网络音频资源。

详细的接口说明请参考 [音频服务帮助头文件](https://github.com/espressif/esp-brookesia/blob/master/service/brookesia_service_helper/include/brookesia/service_helper/audio.hpp)。

## 目录

- [音频服务](#音频服务)
  - [目录](#目录)
  - [播放音频](#播放音频)
  - [播放控制](#播放控制)
  - [设置音量](#设置音量)
  - [订阅播放状态事件](#订阅播放状态事件)
  - [注意事项](#注意事项)

## 播放音频

播放音频文件（支持本地文件和网络 URL）：

```bash
svc_call Audio PlayUrl {"Url":"file://spiffs/example.mp3"}
```

播放网络音频：

```bash
svc_call Audio PlayUrl {"Url":"https://dl.espressif.com/AE/esp-brookesia/example.mp3"}
```

## 播放控制

暂停播放：

```bash
svc_call Audio PlayControl {"Action":"Pause"}
```

恢复播放：

```bash
svc_call Audio PlayControl {"Action":"Resume"}
```

停止播放：

```bash
svc_call Audio PlayControl {"Action":"Stop"}
```

## 设置音量

设置音量（0-100）：

```bash
svc_call Audio SetVolume {"Volume":90}
```

## 订阅播放状态事件

订阅播放状态变化事件：

```bash
svc_subscribe Audio PlayStateChanged
```

当播放状态发生变化时，您将收到事件通知。

## 注意事项

- 本地文件需要保存到 [spiffs](../spiffs/) 目录下，文件路径格式为：`file://spiffs/文件名`
- 网络音频 URL 需要是合法的 HTTP 或 HTTPS URL，例如：`https://dl.espressif.com/AE/esp-brookesia/example.mp3`
