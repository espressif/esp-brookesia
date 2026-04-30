<div align="center">
    <img src="docs/_static/brookesia_logo.png" alt="logo" width="800">
</div>

# ESP-Brookesia

* [中文版本](./README_CN.md)

## Overview

ESP-Brookesia is a human-machine interaction development framework designed for AIoT devices. It aims to simplify the processes of application development and AI capability integration. Built on ESP-IDF, it provides developers with full-stack support — from hardware abstraction and system services to AI agents — through a component-based architecture, accelerating the development and time-to-market of HMI and AI application products.

> [!NOTE]
> "[Brookesia](https://en.wikipedia.org/wiki/Brookesia)" is a genus of chameleons known for their ability to camouflage and adapt to their surroundings, which closely aligns with the goals of ESP-Brookesia. This framework aims to provide a flexible and scalable solution that can adapt to various hardware devices and application requirements, much like the Brookesia chameleon with its high degree of adaptability and flexibility.

The key features of ESP-Brookesia include:

- **Native ESP-IDF Support**: Developed in C/C++, deeply integrated with the ESP-IDF development ecosystem and ESP Registry component registry, fully leveraging the Espressif open-source component ecosystem;
- **Extensible Hardware Abstraction**: Defines unified hardware interfaces (audio, display, touch, storage, etc.) and provides a board-level adaptation layer for quick porting to different hardware platforms;
- **Rich System Services**: Provides out-of-the-box system-level services including Wi-Fi connectivity, audio/video processing, using a Manager + Helper architecture for decoupling and extensibility, providing support for Agent CLI;
- **Multi-LLM Backend Integration**: Built-in adapters for mainstream AI platforms including OpenAI, Coze, Xiaozhi, providing unified agent management and lifecycle control;
- **MCP Protocol Support**: Exposes device service capabilities to large language models via Function Calling / MCP protocol, enabling unified communication between LLMs and system services;
- **AI Expression Capability**: Supports visual AI expression through emoji sets, animation sets, and more, providing rich visual feedback for anthropomorphic interactions.

The functional framework of ESP-Brookesia is shown below, consisting of three layers from bottom to top: **Environment & Dependencies**, **Services & Framework**, and **Application Layer**:

<div align="center">
    <img src="docs/_static/framework_overview.svg" alt="framework_overview" width="800">
</div>
<br>

- **Utils**: Provides common foundational capabilities for upper-layer modules. `General Utils` includes the logging system, error checking, state machine, task scheduler, plugin manager, and memory/thread/time profilers. `MCP Utils` acts as the bridge between ESP-Brookesia services and the MCP engine, exposing registered service functions as standard MCP tools so large language models can call device capabilities.
- **HAL**: Defines unified hardware access interfaces and provides board-level adaptation. `Interface` defines standardized hardware APIs for audio playback/recording, display panels and touch, status LEDs, and storage file systems. `Adaptor` provides implementations for specific development boards and completes hardware resource initialization and mapping. `Boards` provides board-level YAML configuration that describes the peripheral topology, pin assignments, and driver parameters of each board.
- **General Service**: Provides system-level foundational services, including `Wi-Fi` connection management, `Audio` capture and playback, `Video` codec processing, `NVS` non-volatile storage, `SNTP` network time synchronization, and a `Custom` service extension mechanism. All services use the Manager + Helper architecture and support both local calls and RPC-based remote communication.
- **AI Agent Framework**: Provides a unified management framework for AI agents, with built-in adapters for mainstream AI platforms such as `Coze`, `OpenAI`, and `XiaoZhi`. Through the `Function Calling / MCP` protocol, it enables bidirectional communication between large language models and system services, allowing LLMs to perceive and invoke device capabilities.
- **AI Expression**: Provides visual expression capabilities for AI interaction scenarios, including `Emote` sets and animation control, delivering rich visual feedback for anthropomorphic interaction.
- **System** *(planned)*: Provides GUI, system management, and application framework support for different product forms such as mobile devices, speakers, and robots.
- **Runtime** *(planned)*: Provides runtime support for WebAssembly, Python, Lua, and more, enabling dynamic application loading and execution.

## Documentation

- 中文：https://docs.espressif.com/projects/esp-brookesia/zh_CN
- English: https://docs.espressif.com/projects/esp-brookesia/en

## Quick Reference

- [ESP-Brookesia Versioning](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-versioning)
- [Development Environment Setup](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-dev-environment)
- [Hardware Preparation](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-hardware)
- [How to Obtain and Use Components](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-component-usage)
- [How to Use Example Projects](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-example-projects)

## Component List

<center>

#### Utils Components

| Component | Description | Version |
| --- | --- | --- |
| [brookesia_lib_utils](https://components.espressif.com/components/espressif/brookesia_lib_utils) | Core utility library providing task scheduler, thread configuration, performance profilers, logging system, state machine, plugin system, and other foundational capabilities | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_lib_utils/badge.svg)](https://components.espressif.com/components/espressif/brookesia_lib_utils) |
| [brookesia_mcp_utils](https://components.espressif.com/components/espressif/brookesia_mcp_utils) | MCP bridge library that registers Brookesia service functions or custom callbacks as MCP tools for invocation by large language models | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_mcp_utils/badge.svg)](https://components.espressif.com/components/espressif/brookesia_mcp_utils) |

#### HAL Components

| Component | Description | Version |
| --- | --- | --- |
| [brookesia_hal_adaptor](https://components.espressif.com/components/espressif/brookesia_hal_adaptor) | HAL board-level adaptation component providing audio and LCD device interface implementations for specific development boards | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_hal_adaptor/badge.svg)](https://components.espressif.com/components/espressif/brookesia_hal_adaptor) |
| [brookesia_hal_boards](https://components.espressif.com/components/espressif/brookesia_hal_boards) | HAL board configuration component providing board-specific device configurations, peripheral mappings, and hardware setup hooks for esp_board_manager | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_hal_boards/badge.svg)](https://components.espressif.com/components/espressif/brookesia_hal_boards) |
| [brookesia_hal_interface](https://components.espressif.com/components/espressif/brookesia_hal_interface) | HAL foundation component that defines a unified device/interface model, lifecycle management, and hardware interface abstractions (audio, display, touch, storage, etc.) | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_hal_interface/badge.svg)](https://components.espressif.com/components/espressif/brookesia_hal_interface) |

#### Service Components

| Component | Description | Version |
| --- | --- | --- |
| [brookesia_service_audio](https://components.espressif.com/components/espressif/brookesia_service_audio) | Audio service providing audio playback, encoding/decoding, volume control, and playback state management | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_service_audio/badge.svg)](https://components.espressif.com/components/espressif/brookesia_service_audio) |
| [brookesia_service_custom](https://components.espressif.com/components/espressif/brookesia_service_custom) | Custom service extension template for developing custom services based on the Brookesia service framework | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_service_custom/badge.svg)](https://components.espressif.com/components/espressif/brookesia_service_custom) |
| [brookesia_service_device](https://components.espressif.com/components/espressif/brookesia_service_device) | Device service providing application-layer access to HAL hardware abstraction layer control and status interfaces | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_service_device/badge.svg)](https://components.espressif.com/components/espressif/brookesia_service_device) |
| [brookesia_service_helper](https://components.espressif.com/components/espressif/brookesia_service_helper) | Service helper library providing type-safe definitions, schemas, and unified service calling interfaces (based on CRTP pattern) | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_service_helper/badge.svg)](https://components.espressif.com/components/espressif/brookesia_service_helper) |
| [brookesia_service_manager](https://components.espressif.com/components/espressif/brookesia_service_manager) | Service manager providing plugin-based service lifecycle management, dual communication modes (local calls & TCP RPC), and event pub/sub mechanism | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_service_manager/badge.svg)](https://components.espressif.com/components/espressif/brookesia_service_manager) |
| [brookesia_service_nvs](https://components.espressif.com/components/espressif/brookesia_service_nvs) | NVS (Non-Volatile Storage) service providing namespace-based key-value storage management | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_service_nvs/badge.svg)](https://components.espressif.com/components/espressif/brookesia_service_nvs) |
| [brookesia_service_sntp](https://components.espressif.com/components/espressif/brookesia_service_sntp) | SNTP network time service providing NTP server management, timezone settings, and automatic time synchronization | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_service_sntp/badge.svg)](https://components.espressif.com/components/espressif/brookesia_service_sntp) |
| [brookesia_service_video](https://components.espressif.com/components/espressif/brookesia_service_video) | Video service supporting local capture encoding (H.264, MJPEG, RGB/YUV) and compressed-stream decoding to pixel formats | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_service_video/badge.svg)](https://components.espressif.com/components/espressif/brookesia_service_video) |
| [brookesia_service_wifi](https://components.espressif.com/components/espressif/brookesia_service_wifi) | Wi-Fi service providing state machine management, AP scanning, connection management, and SoftAP functionality | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_service_wifi/badge.svg)](https://components.espressif.com/components/espressif/brookesia_service_wifi) |

#### AI Agent Components

| Component | Description | Version |
| --- | --- | --- |
| [brookesia_agent_coze](https://components.espressif.com/components/espressif/brookesia_agent_coze) | Coze agent adapter providing Coze API integration, WebSocket communication, and agent management | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_agent_coze/badge.svg)](https://components.espressif.com/components/espressif/brookesia_agent_coze) |
| [brookesia_agent_helper](https://components.espressif.com/components/espressif/brookesia_agent_helper) | Agent helper library providing type-safe definitions, schemas, and unified agent calling interfaces | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_agent_helper/badge.svg)](https://components.espressif.com/components/espressif/brookesia_agent_helper) |
| [brookesia_agent_manager](https://components.espressif.com/components/espressif/brookesia_agent_manager) | Agent manager providing AI agent lifecycle management, state machine control, and unified operation interfaces | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_agent_manager/badge.svg)](https://components.espressif.com/components/espressif/brookesia_agent_manager) |
| [brookesia_agent_openai](https://components.espressif.com/components/espressif/brookesia_agent_openai) | OpenAI agent adapter providing OpenAI API integration, peer-to-peer communication, and agent management | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_agent_openai/badge.svg)](https://components.espressif.com/components/espressif/brookesia_agent_openai) |
| [brookesia_agent_xiaozhi](https://components.espressif.com/components/espressif/brookesia_agent_xiaozhi) | Xiaozhi agent adapter providing Xiaozhi API integration | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_agent_xiaozhi/badge.svg)](https://components.espressif.com/components/espressif/brookesia_agent_xiaozhi) |

#### AI Expression Components

| Component | Description | Version |
| --- | --- | --- |
| [brookesia_expression_emote](https://components.espressif.com/components/espressif/brookesia_expression_emote) | AI emote and animation management providing emoji sets, animation sets, and event message sets for rich visual feedback in anthropomorphic interactions | [![Component Registry](https://components.espressif.com/components/espressif/brookesia_expression_emote/badge.svg)](https://components.espressif.com/components/espressif/brookesia_expression_emote) |

</center>

## Getting the Repository

If you would like to contribute to ESP-Brookesia or build on the examples in this repository, you can get the `master` branch with the following command:

``bash
git clone https://github.com/espressif/esp-brookesia
``

## Contribution Guidelines

We currently welcome contributions in the following areas:

- Fixing bugs
- Adding support for new development boards to [brookesia_hal_boards](./hal/brookesia_hal_boards)

For new component ideas, we recommend starting a discussion in [GitHub Issues](https://github.com/espressif/esp-brookesia/issues) first so we can align on the use case, interface boundaries, and overall roadmap together. Once there is agreement on the direction, the follow-up contribution process is usually much smoother.

## Additional Resources

- Latest documentation: https://docs.espressif.com/projects/esp-brookesia/en, built from the [docs](./docs) directory of this repository;
- ESP-IDF Programming Guide: https://docs.espressif.com/projects/esp-idf/en;
- ESP-Brookesia components can be found on [ESP Component Registry](https://components.espressif.com/);
- Visit the [esp32.com](https://www.esp32.com/) forum to ask questions and explore community resources;
- If you find a bug or need a new feature, please check [GitHub Issues](https://github.com/espressif/esp-brookesia/issues) first to avoid duplicate submissions;
