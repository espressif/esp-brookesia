# Agent Service

The Agent service provides AI agent management functionality, supporting agent services such as Coze, OpenAI, and Xiaozhi.

For detailed interface documentation, please refer to [Agent Manager](https://github.com/espressif/esp-brookesia/blob/master/agent/brookesia_agent_manager) and [Agent Helper](https://github.com/espressif/esp-brookesia/blob/master/agent/brookesia_agent_helper).

> [!NOTE]
> - Some Agents (such as Coze, OpenAI) require API key configuration in menuconfig
> - Ensure the device is connected to the internet before enabling Agent functionality

## Table of Contents

- [Agent Service](#agent-service)
  - [Table of Contents](#table-of-contents)
  - [General Agent Functions](#general-agent-functions)
    - [Function Call Interface](#function-call-interface)
      - [Activate Agent](#activate-agent)
      - [Trigger General Actions](#trigger-general-actions)
      - [Suspend/Resume](#suspendresume)
      - [Interrupt Agent Speaking](#interrupt-agent-speaking)
      - [Set Chat Mode](#set-chat-mode)
      - [Get Chat Mode](#get-chat-mode)
      - [Manual Start/Stop Listening](#manual-startstop-listening)
      - [Reset Data](#reset-data)
      - [Get Agent Attributes](#get-agent-attributes)
      - [Get Active Agent](#get-active-agent)
      - [Get Status](#get-status)
      - [Set Agent Info](#set-agent-info)
    - [Event Subscription Interface](#event-subscription-interface)
      - [General Action Event](#general-action-event)
      - [General Happened Event](#general-happened-event)
      - [Suspend Status Changed Event](#suspend-status-changed-event)
      - [Speaking Status Changed Event](#speaking-status-changed-event)
      - [Listening Status Changed Event](#listening-status-changed-event)
      - [Agent Speaking Text Event](#agent-speaking-text-event)
      - [User Speaking Text Event](#user-speaking-text-event)
      - [Emote Event](#emote-event)
  - [Special Agent Functions](#special-agent-functions)
    - [Xiaozhi](#xiaozhi)
      - [Event Subscription Interface](#event-subscription-interface-1)
    - [Coze](#coze)
      - [Function Call Interface](#function-call-interface-1)
      - [Event Subscription Interface](#event-subscription-interface-2)

## General Agent Functions

### Function Call Interface

#### Activate Agent

Activate a specific Agent:

```bash
svc_call AgentManager ActivateAgent {"Name":"AgentXiaoZhi"}
```

Parameter description:

- `Name`: Agent name, valid values are
  - `AgentXiaoZhi`
  - `AgentCoze` (requires API key configuration)
  - `AgentOpenai` (requires API key configuration)

#### Trigger General Actions

Start Agent:

```bash
svc_call AgentManager TriggerGeneralAction {"Action":"Start"}
```

Stop Agent:

```bash
svc_call AgentManager TriggerGeneralAction {"Action":"Stop"}
```

Put Agent to sleep:

```bash
svc_call AgentManager TriggerGeneralAction {"Action":"Sleep"}
```

Wake up Agent:

```bash
svc_call AgentManager TriggerGeneralAction {"Action":"WakeUp"}
```

#### Suspend/Resume

Trigger suspend:

```bash
svc_call AgentManager Suspend
```

Trigger resume:

```bash
svc_call AgentManager Resume
```

#### Interrupt Agent Speaking

```bash
svc_call AgentManager InterruptSpeaking
```

#### Set Chat Mode

```bash
svc_call AgentManager SetChatMode {"Mode":"Manual"}
```

Parameter description:

- `Mode`: Chat mode, valid values are
  - `Manual`: Manual mode, Agent requires manual start and stop listening
  - `RealTime`: Real-time mode, Agent listens while speaking

#### Get Chat Mode

```bash
svc_call AgentManager GetChatMode
```

#### Manual Start/Stop Listening

```bash
svc_call AgentManager ManualStartListening
svc_call AgentManager ManualStopListening
```

#### Reset Data

Reset Agent service data:

```bash
svc_call AgentManager ResetData
```

#### Get Agent Attributes

Get attributes of all Agents:

```bash
svc_call AgentManager GetAgentAttributes
```

Get attributes of a specific Agent:

```bash
svc_call AgentManager GetAgentAttributes {"Name":"AgentCoze"}
```

#### Get Active Agent

View the currently active Agent:

```bash
svc_call AgentManager GetActiveAgent
```

#### Get Status

Get general state:

```bash
svc_call AgentManager GetGeneralState
```

Get suspend status:

```bash
svc_call AgentManager GetSuspendStatus
```

Get speaking status:

```bash
svc_call AgentManager GetSpeakingStatus
```

Get listening status:

```bash
svc_call AgentManager GetListeningStatus
```

#### Set Agent Info

Set Agent information:

```bash
svc_call AgentManager SetAgentInfo {...}
```

> [!NOTE]
> For Agents that require authentication (such as `Coze` and `OpenAI`), you need to run this command to set Agent authentication information before running `svc_call AgentManager TriggerGeneralAction {"Action":"Start"}`.


### Event Subscription Interface

#### General Action Event

```bash
svc_subscribe AgentManager GeneralActionTriggered
```

#### General Happened Event

```bash
svc_subscribe AgentManager GeneralEventHappened
```

#### Suspend Status Changed Event

```bash
svc_subscribe AgentManager SuspendStatusChanged
```

#### Speaking Status Changed Event

```bash
svc_subscribe AgentManager SpeakingStatusChanged
```

#### Listening Status Changed Event

```bash
svc_subscribe AgentManager ListeningStatusChanged
```

#### Agent Speaking Text Event

```bash
svc_subscribe AgentManager AgentSpeakingTextGot
```

#### User Speaking Text Event

```bash
svc_subscribe AgentManager UserSpeakingTextGot
```

#### Emote Event

```bash
svc_subscribe AgentManager EmoteGot
```

## Special Agent Functions

### Xiaozhi

Xiaozhi Agent is the XiaoZhi AI platform agent implementation, no API key configuration required, ready to use out of the box.

#### Event Subscription Interface

Subscribe to activation code received event (for device activation binding):

```bash
svc_subscribe AgentXiaoZhi ActivationCodeReceived
```

### Coze

#### Function Call Interface

- Set active robot index

```bash
svc_call AgentCoze SetActiveRobotIndex {"Index":0}
```

- Get active robot index

```bash
svc_call AgentCoze GetActiveRobotIndex
```

- Get robot information

```bash
svc_call AgentCoze GetRobotInfos
```

#### Event Subscription Interface

- Coze event

```bash
svc_subscribe AgentCoze CozeEventHappened
```
