# Emote 服务

Emote（表情）服务提供了表情、动画及事件消息的显示功能。

详细的接口说明请参考 [Emote 服务帮助头文件](https://github.com/espressif/esp-brookesia/blob/master/service/brookesia_service_helper/include/brookesia/service_helper/expression/emote.hpp)。

## 目录

- [Emote 服务](#emote-服务)
  - [目录](#目录)
  - [设置表情](#设置表情)
  - [设置动画](#设置动画)
  - [插入动画](#插入动画)
  - [停止动画](#停止动画)
  - [等待动画一帧完成](#等待动画一帧完成)
  - [设置事件消息](#设置事件消息)
  - [加载资源](#加载资源)

## 设置表情

设置一个表情：

```bash
svc_call Emote SetEmoji {"Emoji":"angry"}
```

参数说明：

- `Emoji`：表情名称，以 360x360 的图片资源为例，请参考 [360_360/emote.json](https://github.com/espressif2022/esp_emote_assets/blob/main/360_360/emote.json) 文件中的 `emote` 字段。

## 设置动画

设置一个动画：

```bash
svc_call Emote SetAnimation {"Animation":"angry"}
```

参数说明：

- `Animation`：动画名称

## 插入动画

插入一个动画到队列中, 动画将会在指定时间后自动停止：

```bash
svc_call Emote InsertAnimation {"Animation":"angry","DurationMs":3000}
```

参数说明：

- `Animation`：动画名称
- `DurationMs`：动画持续时间（毫秒）

## 停止动画

停止当前正在播放的动画：

```bash
svc_call Emote StopAnimation
```

## 等待动画一帧完成

等待动画的一帧完成：

```bash
svc_call Emote WaitAnimationFrameDone {"TimeoutMs":3000}
```

参数说明：

- `TimeoutMs`：等待时间（毫秒），0 表示等待 forever

## 设置事件消息

为指定事件设置消息：

```bash
svc_call Emote SetEventMessage {"Event":"Idle"}
svc_call Emote SetEventMessage {"Event":"Speak"}
svc_call Emote SetEventMessage {"Event":"Listen"}
svc_call Emote SetEventMessage {"Event":"System","Message":"system_message"}
svc_call Emote SetEventMessage {"Event":"User","Message":"user_message"}

// 电池信息仅在 `Idle` 状态下显示
// 设置电池消息，格式为："charging,percent" e.g. "1,75" or "0,30"
svc_call Emote SetEventMessage {"Event":"Battery","Message":"1,75"}
svc_call Emote SetEventMessage {"Event":"Battery","Message":"0,30"}
```

事件类型说明：

- `Idle`：空闲状态
- `Speak`：说话状态
- `Listen`：聆听状态
- `System`：系统事件
- `User`：用户事件
- `Battery`：电池事件

参数说明：

- `Event`：事件类型，可选值：`Idle`、`Speak`、`Listen`、`System`、`User`、`Battery`
- `Message`：消息内容（可选，默认为空字符串）

## 加载资源

从分区标签加载：

```bash
svc_call Emote LoadAssetsSource {"Source":{"source":"anim_icon","type":"PartitionLabel","flag_enable_mmap":false}}
```

或者从文件路径加载资源：

```bash
svc_call Emote LoadAssetsSource {"Source":{"source":"/spiffs/anim_icon.bin","type":"Path","flag_enable_mmap":false}}
```

参数说明：

- `source`：源路径或分区标签名称
- `type`：源类型，可选值：`Path`（文件路径）或 `PartitionLabel`（分区标签）
- `flag_enable_mmap`：是否启用内存映射，可选值：`true`或 `false`（默认值为 `false`）。

> [!WARNING]
> - 当前示例默认从分区标签 `anim_icon` 加载资源。
> - 当 `flag_enable_mmap` 为 `true` 且动画运行时，不能操作文件系统，否则会导致程序异常。
