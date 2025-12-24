# Agent 服务

Agent 服务提供了 AI 代理管理功能，支持 Coze 和 OpenAI 等代理服务。

详细的接口说明请参考 [Agent 服务帮助头文件](https://github.com/espressif/esp-brookesia/blob/master/service/brookesia_service_helper/include/brookesia/service_helper/agent)。

## 目录

- [Agent 服务](#agent-服务)
  - [目录](#目录)
  - [管理器功能](#管理器功能)
    - [获取 Agent 属性](#获取-agent-属性)
    - [获取活动 Agent](#获取活动-agent)
    - [获取状态](#获取状态)
    - [设置 Agent 信息](#设置-agent-信息)
    - [激活/停用 Agent](#激活停用-agent)
    - [触发通用操作](#触发通用操作)
    - [挂起/恢复](#挂起恢复)
    - [中断 Agent 说话](#中断-agent-说话)
    - [重置数据](#重置数据)
    - [订阅 Agent 事件](#订阅-agent-事件)
  - [Coze Agent](#coze-agent)
    - [函数](#函数)
    - [事件](#事件)
  - [注意事项](#注意事项)

## 管理器功能

### 获取 Agent 属性

获取所有 Agent 的属性：

```bash
svc_call Agent GetAgentAttributes
```

获取指定 Agent 的属性：

```bash
svc_call Agent GetAgentAttributes {"Name":"Coze"}
```

### 获取活动 Agent

查看当前激活的 Agent：

```bash
svc_call Agent GetActiveAgent
```

### 获取状态

获取通用状态：

```bash
svc_call Agent GetGeneralState
```

获取挂起状态：

```bash
svc_call Agent GetSuspendStatus
```

### 设置 Agent 信息

设置 Agent 信息：

```bash
svc_call Agent SetAgentInfo {...}
```

> [!NOTE]
> 运行 `svc_call Agent TriggerGeneralAction {"Action":"Start"}` 前，需要先运行此命令设置 Agent 鉴权信息。

### 激活/停用 Agent

激活指定的 Agent：

```bash
svc_call Agent ActivateAgent {"Name":"Coze"}
svc_call Agent ActivateAgent {"Name":"Openai"}
```

停用当前激活的 Agent：

```bash
svc_call Agent DeactivateAgent
```

### 触发通用操作

启动 Agent：

```bash
svc_call Agent TriggerGeneralAction {"Action":"Start"}
```

> [!NOTE]
> 运行此命令前，需要先运行 `svc_call Agent SetAgentInfo {...}` 设置 Agent 鉴权信息。

停止 Agent：

```bash
svc_call Agent TriggerGeneralAction {"Action":"Stop"}
```

让 Agent 进入睡眠状态：

```bash
svc_call Agent TriggerGeneralAction {"Action":"Sleep"}
```

唤醒 Agent：

```bash
svc_call Agent TriggerGeneralAction {"Action":"WakeUp"}
```

### 挂起/恢复

触发挂起：

```bash
svc_call Agent TriggerSuspend
```

触发恢复：

```bash
svc_call Agent TriggerResume
```

### 中断 Agent 说话

中断 Agent 正在说话：

```bash
svc_call Agent TriggerInterruptSpeaking
```

### 重置数据

重置 Agent 服务数据：

```bash
svc_call Agent ResetData
```

### 订阅 Agent 事件

订阅通用操作触发事件：

```bash
svc_subscribe Agent GeneralActionTriggered
```

订阅通用事件：

```bash
svc_subscribe Agent GeneralEventHappened
```

订阅挂起状态变化事件：

```bash
svc_subscribe Agent SuspendStatusChanged
```

订阅 Agent 说话文本事件：

```bash
svc_subscribe Agent AgentSpeakingTextGot
```

订阅用户说话文本事件：

```bash
svc_subscribe Agent UserSpeakingTextGot
```

订阅表情事件：

```bash
svc_subscribe Agent EmoteGot
```

## Coze Agent

### 函数

设置活动机器人索引：

```bash
svc_call Agent SetActiveRobotIndex {"Index":0}
```

获取活动机器人索引：

```bash
svc_call Agent GetActiveRobotIndex
```

获取机器人信息：

```bash
svc_call Agent GetRobotInfos
```

### 事件

订阅 Coze 事件：

```bash
svc_subscribe Agent CozeEventHappened
```

## 注意事项

- Agent 服务需要 WiFi 和 SNTP 服务支持
- Agent（如 Coze、OpenAI）需要配置 API 密钥
- 确保设备已连接到互联网
- Agent 配置需要在 menuconfig 中完成
