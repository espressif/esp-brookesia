# Emote 服务

Emote（表情）服务提供了表情、动画及事件消息的显示功能。

详细的接口说明请参考 [Emote 服务帮助头文件](https://github.com/espressif/esp-brookesia/blob/master/service/brookesia_service_helper/include/brookesia/service_helper/expression/emote.hpp)。

> [!NOTE]
> - Emote 服务需要在支持的开发板上使用，请参考 [准备工作](../README_CN.md#准备工作) 章节了解默认支持的开发板列表
> - 表情和动画资源请参考 [esp_emote_assets](https://github.com/espressif2022/esp_emote_assets)

## 目录

- [Emote 服务](#emote-服务)
  - [目录](#目录)
  - [函数调用接口](#函数调用接口)
    - [加载资源](#加载资源)
    - [表情](#表情)
      - [显示表情](#显示表情)
      - [隐藏表情](#隐藏表情)
    - [动画](#动画)
      - [设置动画](#设置动画)
      - [插入动画](#插入动画)
      - [停止动画](#停止动画)
      - [等待动画帧完成](#等待动画帧完成)
    - [事件消息](#事件消息)
      - [设置事件消息](#设置事件消息)
      - [隐藏事件消息](#隐藏事件消息)
    - [二维码](#二维码)
      - [显示二维码](#显示二维码)
      - [隐藏二维码](#隐藏二维码)
  - [事件订阅接口](#事件订阅接口)
    - [刷新就绪事件](#刷新就绪事件)

## 函数调用接口

### 加载资源

从分区标签加载资源：

```bash
svc_call Emote LoadAssetsSource {"Source":{"source":"anim_icon","type":"PartitionLabel","flag_enable_mmap":false}}
```

从文件路径加载资源：

```bash
svc_call Emote LoadAssetsSource {"Source":{"source":"/spiffs/anim_icon.bin","type":"Path","flag_enable_mmap":false}}
```

参数说明：

- `source`：源路径或分区标签名称
- `type`：源类型，可选值为 `Path`（文件路径）或 `PartitionLabel`（分区标签）
- `flag_enable_mmap`：是否启用内存映射，可选值为 `true` 或 `false`（默认为 `false`）

> [!WARNING]
> - 当前示例默认从分区标签 `anim_icon` 加载资源
> - 当 `flag_enable_mmap` 为 `true` 且动画运行时，不能操作文件系统，否则会导致程序异常

### 表情

#### 显示表情

```bash
svc_call Emote SetEmoji {"Emoji":"angry"}
```

参数说明：

- `Emoji`：表情名称，请参考 [360_360/emote.json](https://github.com/espressif2022/esp_emote_assets/blob/main/360_360/emote.json) 文件中的 `emote` 字段

> [!NOTE]
> 显示表情时，动画将被立即隐藏

#### 隐藏表情

```bash
svc_call Emote HideEmoji
```

### 动画

#### 设置动画

```bash
svc_call Emote SetAnimation {"Animation":"winking"}
```

参数说明：

- `Animation`：动画名称

> [!NOTE]
> 设置动画时，表情将被立即隐藏

#### 插入动画

插入一个动画到队列中，动画将会在指定时间后自动停止：

```bash
svc_call Emote InsertAnimation {"Animation":"sleepy","DurationMs":3000}
```

参数说明：

- `Animation`：动画名称
- `DurationMs`：动画持续时间（毫秒），到期后自动停止

#### 停止动画

```bash
svc_call Emote StopAnimation
```

#### 等待动画帧完成

等待动画每一帧完成：

```bash
svc_call Emote WaitAnimationFrameDone {"TimeoutMs":3000}
```

参数说明：

- `TimeoutMs`：等待超时时间（毫秒），`0` 表示永久等待

### 事件消息

#### 设置事件消息

设置空闲状态消息：

```bash
svc_call Emote SetEventMessage {"Event":"Idle"}
```

设置说话状态消息：

```bash
svc_call Emote SetEventMessage {"Event":"Speak"}
```

设置聆听状态消息：

```bash
svc_call Emote SetEventMessage {"Event":"Listen"}
```

设置系统消息：

```bash
svc_call Emote SetEventMessage {"Event":"System","Message":"系统消息内容"}
```

设置用户消息：

```bash
svc_call Emote SetEventMessage {"Event":"User","Message":"用户消息内容"}
```

设置电池状态消息：

```bash
# 格式为："charging,percent"，如 "1,75" 表示充电中，电量 75%
svc_call Emote SetEventMessage {"Event":"Battery","Message":"1,75"}
# "0,30" 表示未充电，电量 30%
svc_call Emote SetEventMessage {"Event":"Battery","Message":"0,30"}
```

设置二维码消息：

```bash
svc_call Emote SetEventMessage {"Event":"QRCode","Message":"https://www.espressif.com"}
```

参数说明：

- `Event`：事件类型，可选值为
  - `Idle`：空闲状态
  - `Speak`：说话状态
  - `Listen`：聆听状态
  - `System`：系统消息
  - `User`：用户消息
  - `Battery`：电池状态（格式：`"充电状态,电量百分比"`）
  - `QRCode`：二维码
- `Message`：消息内容（可选，默认为空字符串）

> [!NOTE]
> 电池信息仅在 `Idle` 状态下显示

#### 隐藏事件消息

```bash
svc_call Emote HideEventMessage
```

### 二维码

#### 显示二维码

```bash
svc_call Emote SetQrcode {"Qrcode":"https://www.espressif.com"}
```

参数说明：

- `Qrcode`：二维码内容

> [!NOTE]
> 显示二维码时，表情和动画将被立即隐藏

#### 隐藏二维码

```bash
svc_call Emote HideQrcode
```

> [!NOTE]
> 隐藏二维码后，表情将被立即显示

## 事件订阅接口

### 刷新就绪事件

订阅刷新就绪事件（用于显示驱动回调）：

```bash
svc_subscribe Emote FlushReady
```

事件参数：

- `Param`：刷新参数对象，包含
  - `x_start`：起始 X 坐标
  - `y_start`：起始 Y 坐标
  - `x_end`：结束 X 坐标
  - `y_end`：结束 Y 坐标
  - `data`：显示数据指针

> [!NOTE]
> 此事件通常用于底层显示驱动集成，普通用户无需订阅
