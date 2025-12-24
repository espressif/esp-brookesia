# Agent Service

The Agent service provides AI agent management functionality, supporting agent services such as Coze and OpenAI.

Detailed interface descriptions please refer to [Agent service helper header file](https://github.com/espressif/esp-brookesia/blob/master/service/brookesia_service_helper/include/brookesia/service_helper/agent).

## Table of Contents

- [Agent Service](#agent-service)
  - [Table of Contents](#table-of-contents)
  - [Manager Functions](#manager-functions)
    - [Get Agent Attributes](#get-agent-attributes)
    - [Get Active Agent](#get-active-agent)
    - [Get Status](#get-status)
    - [Set Agent Info](#set-agent-info)
    - [Activate/Deactivate Agent](#activatedeactivate-agent)
    - [Trigger General Actions](#trigger-general-actions)
    - [Suspend/Resume](#suspendresume)
    - [Interrupt Agent Speaking](#interrupt-agent-speaking)
    - [Reset Data](#reset-data)
    - [Subscribe to Agent Events](#subscribe-to-agent-events)
  - [Coze Agent](#coze-agent)
    - [Functions](#functions)
    - [Events](#events)
  - [Notes](#notes)

## Manager Functions

### Get Agent Attributes

Get attributes of all Agents:

```bash
svc_call Agent GetAgentAttributes
```

Get attributes of a specific Agent:

```bash
svc_call Agent GetAgentAttributes {"Name":"Coze"}
```

### Get Active Agent

View the currently active Agent:

```bash
svc_call Agent GetActiveAgent
```

### Get Status

Get general state:

```bash
svc_call Agent GetGeneralState
```

Get suspend status:

```bash
svc_call Agent GetSuspendStatus
```

### Set Agent Info

Set Agent information:

```bash
svc_call Agent SetAgentInfo {...}
```

> [!NOTE]
> Before running `svc_call Agent TriggerGeneralAction {"Action":"Start"}`, you need to run this command first to set Agent authentication information.

### Activate/Deactivate Agent

Activate a specific Agent:

```bash
svc_call Agent ActivateAgent {"Name":"Coze"}
svc_call Agent ActivateAgent {"Name":"Openai"}
```

Deactivate the currently active Agent:

```bash
svc_call Agent DeactivateAgent
```

### Trigger General Actions

Start Agent:

```bash
svc_call Agent TriggerGeneralAction {"Action":"Start"}
```

> [!NOTE]
> Before running this command, you need to run `svc_call Agent SetAgentInfo {...}` first to set Agent authentication information.

Stop Agent:

```bash
svc_call Agent TriggerGeneralAction {"Action":"Stop"}
```

Put Agent to sleep:

```bash
svc_call Agent TriggerGeneralAction {"Action":"Sleep"}
```

Wake up Agent:

```bash
svc_call Agent TriggerGeneralAction {"Action":"WakeUp"}
```

### Suspend/Resume

Trigger suspend:

```bash
svc_call Agent TriggerSuspend
```

Trigger resume:

```bash
svc_call Agent TriggerResume
```

### Interrupt Agent Speaking

Interrupt Agent while speaking:

```bash
svc_call Agent TriggerInterruptSpeaking
```

### Reset Data

Reset Agent service data:

```bash
svc_call Agent ResetData
```

### Subscribe to Agent Events

Subscribe to general action triggered event:

```bash
svc_subscribe Agent GeneralActionTriggered
```

Subscribe to general event:

```bash
svc_subscribe Agent GeneralEventHappened
```

Subscribe to suspend status changed event:

```bash
svc_subscribe Agent SuspendStatusChanged
```

Subscribe to Agent speaking text event:

```bash
svc_subscribe Agent AgentSpeakingTextGot
```

Subscribe to user speaking text event:

```bash
svc_subscribe Agent UserSpeakingTextGot
```

Subscribe to emote event:

```bash
svc_subscribe Agent EmoteGot
```

## Coze Agent

### Functions

Set active robot index:

```bash
svc_call Agent SetActiveRobotIndex {"Index":0}
```

Get active robot index:

```bash
svc_call Agent GetActiveRobotIndex
```

Get robot information:

```bash
svc_call Agent GetRobotInfos
```

### Events

Subscribe to Coze event:

```bash
svc_subscribe Agent CozeEventHappened
```

## Notes

- Agent service requires WiFi and SNTP service support
- Agents (such as Coze, OpenAI) require API key configuration
- Ensure the device is connected to the internet
- Agent configuration needs to be completed in menuconfig
