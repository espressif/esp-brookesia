# Agent 服务

Agent 服务提供了 AI 代理管理功能，支持 Coze、OpenAI、Xiaozhi 等代理服务。

详细的接口说明请参考 [Agent 服务帮助头文件](https://github.com/espressif/esp-brookesia/blob/master/agent/brookesia_agent_helper/include/brookesia/agent_helper)。

> [!NOTE]
> - Agent 服务需要在支持的开发板上使用，请参考 [准备工作](../README_CN.md#准备工作) 章节了解默认支持的开发板列表
> - 部分 Agent（如 Coze、OpenAI）需要在 menuconfig 中配置 API 密钥
> - 开启 Agent 功能前需要确保已通过 WiFi 连接到互联网

## 目录

- [Agent 服务](#agent-服务)
  - [目录](#目录)
  - [通用 Agent 功能](#通用-agent-功能)
    - [函数调用接口](#函数调用接口)
      - [激活 Agent](#激活-agent)
      - [触发通用操作](#触发通用操作)
      - [挂起/恢复](#挂起恢复)
      - [中断 Agent 说话](#中断-agent-说话)
      - [设置聊天模式](#设置聊天模式)
      - [获取聊天模式](#获取聊天模式)
      - [手动开始/停止倾听](#手动开始停止倾听)
      - [重置数据](#重置数据)
      - [获取 Agent 属性](#获取-agent-属性)
      - [获取活动 Agent](#获取活动-agent)
      - [获取状态](#获取状态)
      - [设置 Agent 信息](#设置-agent-信息)
    - [事件订阅接口](#事件订阅接口)
      - [通用操作事件](#通用操作事件)
      - [通用发生事件](#通用发生事件)
      - [挂起状态变化事件](#挂起状态变化事件)
      - [说话状态变化事件](#说话状态变化事件)
      - [倾听状态变化事件](#倾听状态变化事件)
      - [Agent 说话文本事件](#agent-说话文本事件)
      - [用户说话文本事件](#用户说话文本事件)
      - [表情事件](#表情事件)
  - [特殊 Agent 功能](#特殊-agent-功能)
    - [Xiaozhi](#xiaozhi)
      - [事件订阅接口](#事件订阅接口-1)
    - [Coze](#coze)
      - [函数调用接口](#函数调用接口-1)
      - [事件订阅接口](#事件订阅接口-2)

## 通用 Agent 功能

### 函数调用接口

#### 激活 Agent

激活指定的 Agent：

```bash
svc_call AgentManager ActivateAgent {"Name":"AgentXiaoZhi"}
```

参数说明：

- `Name`：Agent 名称，可选值为
  - `AgentXiaoZhi`
  - `AgentCoze`（需要配置 API 密钥）
  - `AgentOpenai`（需要配置 API 密钥）

> [!NOTE]
> 参数会被保存到 NVS 中，因此只需设置一次即可，后续调用时无需手动指定。

#### 触发通用操作

启动 Agent：

```bash
svc_call AgentManager TriggerGeneralAction {"Action":"Start"}
```

停止 Agent：

```bash
svc_call AgentManager TriggerGeneralAction {"Action":"Stop"}
```

让 Agent 进入睡眠状态：

```bash
svc_call AgentManager TriggerGeneralAction {"Action":"Sleep"}
```

唤醒 Agent：

```bash
svc_call AgentManager TriggerGeneralAction {"Action":"WakeUp"}
```

#### 挂起/恢复

触发挂起：

```bash
svc_call AgentManager Suspend
```

触发恢复：

```bash
svc_call AgentManager Resume
```

#### 中断 Agent 说话

```bash
svc_call AgentManager InterruptSpeaking
```

#### 设置聊天模式

```bash
svc_call AgentManager SetChatMode {"Mode":"Manual"}
```

参数说明：

- `Mode`：聊天模式，可选值为
  - `Manual`：手动模式，Agent 需要手动开始和停止倾听
  - `RealTime`：实时模式，Agent 会在说话时倾听

> [!NOTE]
> 参数会被保存到 NVS 中，因此只需设置一次即可，后续调用时无需手动指定。

#### 获取聊天模式

```bash
svc_call AgentManager GetChatMode
```

#### 手动开始/停止倾听

```bash
svc_call AgentManager ManualStartListening
svc_call AgentManager ManualStopListening
```

#### 重置数据

重置 Agent 服务数据：

```bash
svc_call AgentManager ResetData
```

> [!WARNING]
> 重置数据会清除所有 Agent 的配置信息，包括 API 密钥、聊天模式等。

#### 获取 Agent 属性

获取所有 Agent 的属性：

```bash
svc_call AgentManager GetAgentAttributes
```

获取指定 Agent 的属性：

```bash
svc_call AgentManager GetAgentAttributes {"Name":"AgentCoze"}
```

#### 获取活动 Agent

查看当前激活的 Agent：

```bash
svc_call AgentManager GetActiveAgent
```

#### 获取状态

获取通用状态：

```bash
svc_call AgentManager GetGeneralState
```

获取挂起状态：

```bash
svc_call AgentManager GetSuspendStatus
```

获取说话状态：

```bash
svc_call AgentManager GetSpeakingStatus
```

获取倾听状态：

```bash
svc_call AgentManager GetListeningStatus
```

#### 设置 Agent 信息

设置 Agent 信息：

```bash
svc_call AgentManager SetAgentInfo {...}
```

> [!NOTE]
> 对于需要鉴权的 Agent（如 `Coze` 和 `OpenAI`），在运行 `svc_call AgentManager TriggerGeneralAction {"Action":"Start"}` 前，需要先运行此命令设置 Agent 鉴权信息。
> 示例中可以通过 menuconfig 配置 Coze 和 OpenAI 的 API 密钥，因此设置无需手动执行。
> 参数会被保存到 NVS 中，因此只需设置一次即可，后续调用时无需手动指定。


### 事件订阅接口

#### 通用操作事件

```bash
svc_subscribe AgentManager GeneralActionTriggered
```

#### 通用发生事件

```bash
svc_subscribe AgentManager GeneralEventHappened
```

#### 挂起状态变化事件

```bash
svc_subscribe AgentManager SuspendStatusChanged
```

#### 说话状态变化事件

```bash
svc_subscribe AgentManager SpeakingStatusChanged
```

#### 倾听状态变化事件

```bash
svc_subscribe AgentManager ListeningStatusChanged
```

#### Agent 说话文本事件

```bash
svc_subscribe AgentManager AgentSpeakingTextGot
```

#### 用户说话文本事件

```bash
svc_subscribe AgentManager UserSpeakingTextGot
```

#### 表情事件

```bash
svc_subscribe AgentManager EmoteGot
```

## 特殊 Agent 功能

### Xiaozhi

Xiaozhi Agent 是小智 AI 平台的智能体实现，无需配置 API 密钥，开箱即用。

#### 事件订阅接口

订阅激活码接收事件（用于设备激活绑定）：

```bash
svc_subscribe AgentXiaoZhi ActivationCodeReceived
```

### Coze

#### 函数调用接口

- 设置活动机器人索引

```bash
svc_call AgentCoze SetActiveRobotIndex {"Index":0}
```

- 获取活动机器人索引

```bash
svc_call AgentCoze GetActiveRobotIndex
```

- 获取机器人信息

```bash
svc_call AgentCoze GetRobotInfos
```

#### 事件订阅接口

- Coze 事件

```bash
svc_subscribe AgentCoze CozeEventHappened
```
