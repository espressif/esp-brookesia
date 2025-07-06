# GMF AI Audio

- [English](./README.md)

`GMF AI Audio` 是一个人工智能语音处理模块，它在 [GMF](https://github.com/espressif/esp-gmf) 层面为用户提供方便、易用的语音唤醒、命令词识别和回声消除等常用智能语音处理算法。目前基于 `esp-sr` 提供以下模块：

- [esp_gmf_afe_manager](./src/esp_gmf_afe_manager.c): `audio front end(afe)` 管理器
- [esp_gmf_aec](./src/esp_gmf_aec.c): 回声消除
- [esp_gmf_wn](./src/esp_gmf_wn.c): 独立的唤醒词检测模块，不依赖AFE
- [esp_gmf_afe](./src/esp_gmf_afe.c): 基于 `esp-sr` 中 `audio front end (afe)` 实现的易用接口，提供了语音唤醒，命令词识别，人声检测等语音识别的功能

| 名称 | 标签 | 功能 | 函数方法 | 输入通道 | 输出通道 | 依赖模型数据分区 | 输入帧长 | 备注 |
|:----:|:----:|:----:|:----:|:----:|:----:|:----:|:----:|:----|
| esp_gmf_afe | ai_afe | 语音前处理：唤醒词检测、命令词识别、人声增强、回声消除、降噪、自动增益控制 | `start_vcmd_det` | 1-4 | 1 | 是 | 256(sample) | 目前最多支持2通道麦克风+1个扬声器的回采信号，剩余通道选择记为N，同时需要按照命令词检测步骤操作 |
| esp_gmf_aec | ai_aec | 回声消除：消除音频中的回声干扰，提升语音质量 | 无 | 1-4 | 1 | 否 | 256(sample) | 输入通道可以设置多个麦克风，采用第一个麦克风通道和回采通道进行计算，且必须包含回采信号 |
| esp_gmf_wn | ai_wn | 独立唤醒词检测：轻量级唤醒词检测，不依赖AFE，资源占用小 | 无 | 1-4 | 1 | 是 | 256(sample) | 最高支持3通道麦克风，输入格式中的麦克风通道数需要与工作模式对应 |

## AFE 管理器 `esp_gmf_afe_manager`

### 功能

- 管理音频前端（Audio Front-End, AFE）的数据通路与任务调度
- 支持动态启用/禁用特性（如唤醒、AEC、VAD）
- 提供多任务协同机制，确保实时音频流处理

### 关键特性

- 任务管理：创建独立的 `feed_task` 和 `fetch_task`，分别负责音频数据输入和结果处理
- 特性控制：通过 `esp_afe_manager_enable_features` 动态开关算法模块
- 事件驱动：支持挂起/恢复操作 `esp_afe_manager_suspend`，适配低功耗场景

### 系统框图

  ```mermaid
  graph LR
    subgraph AFE管理器
        DATA_CB[原始数据]
        FEED_TASK[feed_task<br>音频输入任务]
        FETCH_TASK[fetch_task<br>结果处理任务]
        AFE_CORE[AFE核心<br>esp_afe_sr_iface]
        EVENT_CB[结果回调]
        CTRL_IF[控制接口]
    end

    FEED_TASK -->|数据回调| DATA_CB
    FEED_TASK -->|原始音频数据| AFE_CORE
    AFE_CORE -->|处理结果| FETCH_TASK
    FETCH_TASK -->|结果回调<br>（数据/唤醒/VAD/命令词）| EVENT_CB
    CTRL_IF -->|挂起/恢复接口| FEED_TASK
    CTRL_IF -->|挂起/恢复接口| FETCH_TASK
    CTRL_IF -->|特性使能接口<br>（AEC/VAD/SE）| AFE_CORE
  ```

## 回声消除 `esp_gmf_aec`

### 功能

- 消除音频中的回声干扰，提升语音质量
- 支持多通道输入，单通道输出

### 技术规格

- 支持硬件：`ESP32`、`ESP32-S3` 和 `ESP32-P4`
- 输入格式
  - 采样率：16 kHz
  - 位宽：16-bit PCM
- 通道配置：字符串标识（如 `MMNR`）
  - `M`：麦克风通道
  - `R`：回采信号通道
  - `N`：无效信号
- 输出格式：16-bit 单通道 PCM

## 唤醒词检测 `esp_gmf_wn`

### 功能

- 独立运行，不依赖AFE，资源占用小
- 支持多通道输入，单通道输出
- 通过回调函数通知唤醒词检测结果

### 技术规格

- 支持硬件：`ESP32`、`ESP32-S3`、`ESP32-C3`、`ESP32-C5` 和 `ESP32-P4`
- 输入格式
  - 采样率：8 kHz, 16 kHz
  - 位宽：16-bit PCM
- 通道配置：字符串标识（如 `MMNR`）
  - `M`：麦克风通道
  - `R`：回采信号通道
  - `N`：无效信号
- 输出格式：16-bit 单通道 PCM

## 语音识别 `esp_gmf_afe`

### 功能

- 语音唤醒（Wake Word Detection）
- 命令词识别（Voice Command Detection）
- 人声检测（VAD）与状态管理

### 关键特性

- 支持硬件: `esp32`, `esp32s3` 和 `esp32p4`
- 输入与输出同上文中的 `esp_gmf_aec`
- 统一事件接口：通过回调函数通知事件（唤醒开始/结束、VAD 状态变化、命令词触发）
- 状态机管理：自动维护唤醒、语音、静音等状态切换

### 系统框图

  ```mermaid
  graph TD
    subgraph 音频前级处理
      RAW[音频数据]
    end

    subgraph GMF_AFE
        AFE_MGR[AFE管理器<br>esp_gmf_afe_manager]
        INPUT_RB[输入缓存]
        OUTPUT_RB[输出缓存]
        STATE_MACHINE[唤醒/VAD状态机]
        MN_DET[命令词检测]
        DELAY[延时机制]
    end

    subgraph 应用层
      EVENT_CB[事件回调]
      OUT_PORT[数据出口]
    end

    RAW -->| 音频前级处理后的数据| INPUT_RB
    INPUT_RB -->| 数据回调 | AFE_MGR
    AFE_MGR -->| 结果回调<br>数据 | OUTPUT_RB
    AFE_MGR -->| 结果回调<br>唤醒/VAD事件 | STATE_MACHINE
    AFE_MGR -->| 结果回调<br>数据 | MN_DET
    OUTPUT_RB --> DELAY --> OUT_PORT
    STATE_MACHINE -->| 事件通知 | EVENT_CB
    MN_DET -->| 事件通知 | EVENT_CB
  ```

### `esp_gmf_afe_cfg_t` 高级配置参数说明

  | 参数 | 描述 | 默认值 |
  | --- | --- | --- |
  | **wakeup_time**   | 唤醒功能启用后，若未检测到人声事件，则在等待 `wakeup_time` 毫秒后触发唤醒结束事件 | 10000（ms）  |
  | **wakeup_end**    | 唤醒功能启用后，若依次检测到人声和静音，则在静音事件触发后等待 `wakeup_end` 毫秒，触发唤醒结束事件 | 2000（ms） |
  | **vcmd_timeout**  | 命令词检测的超时时间，超时后需重新调用接口以开启新一轮命令词检测 | 5760（ms） |
  | **delay_samples** | 输出数据延迟，用于补偿 VAD 检测滞后导致的数据输出与事件回调不同步的问题。该参数指定的数据长度（以采样点为单位）换算为时间后，应大于等于初始化 AFE 管理器时配置的 `afe_config_t.vad_min_speech_ms` | 2048（samples） |

### 唤醒与VAD状态机

以下列举了功能使能情况下的状态切换，`/` 字符之后为触发的用户事件

- **使能唤醒，不使能VAD**

  该场景主要关注唤醒词的检测，同时可以配置（wakeup_time），在一段时间后触发结束事件

  修改示例 [wwe](./examples/wwe/main/main.c) 中的配置，从而使用该场景

  ```c
  #define WAKENET_ENABLE (true)
  #define VAD_ENABLE     (false)
  ```

  ```mermaid
  stateDiagram-v2
      [*] --> IDLE

      IDLE --> WAKEUP: 唤醒词 / （WAKEUP_START）

      WAKEUP --> IDLE: 定时器（wakeup_time）超时 / （WAKEUP_END）
  ```

- **使VAD，不使能唤醒词**

  该场景主要关注人声的检测，在检测到人说话的前后节点触发相应事件

  修改示例 [wwe](./examples/wwe/main/main.c) 中的配置，从而使用该场景

  ```c
  #define WAKENET_ENABLE (false)
  #define VAD_ENABLE     (true)
  ```

  ```mermaid
  stateDiagram-v2
      [*] --> IDLE

      IDLE --> SPEECHING: 检测到人声 / （VAD_START）

      SPEECHING --> IDLE: 检测到噪声或静音 / （VAD_END）
  ```

- **使能唤醒和VAD**

  该场景结合唤醒词与人声检测，避免在唤醒间隔之外频繁触发人声检测事件

  修改示例 [wwe](./examples/wwe/main/main.c) 中的配置，从而使用该场景

  ```c
  #define WAKENET_ENABLE (true)
  #define VAD_ENABLE     (true)
  ```

  ```mermaid
  stateDiagram-v2
      [*] --> IDLE

      IDLE --> WAKEUP: 唤醒词 / （WAKEUP_START）

      WAKEUP --> SPEECHING: 检测到人声 / （VAD_START）
      WAKEUP --> IDLE: 定时器（wakeup_time）超时 / （WAKEUP_END）

      SPEECHING --> WAIT_FOR_SLEEP: 检测到噪声或静音 / （VAD_END）

      WAIT_FOR_SLEEP --> SPEECHING: 检测到人声 / （VAD_START）
      WAIT_FOR_SLEEP --> IDLE: 定时器（wakeup_end）超时 / （WAKEUP_END）
  ```

### 命令词检测

用户需决定何时开始检测命令词，如典型的使用场景：在状态机推送（WAKEUP_START）事件之后，开启检测，并在回调函数中根据检测到的命令词序号决定下一步操作

- 命令词检测与唤醒状态机独立
- 命令词支持连续检测，直到超时

## 示例

示例代码，请参阅 [示例](./examples) 文件夹中的唤醒词检测和音频回声消除程序。
