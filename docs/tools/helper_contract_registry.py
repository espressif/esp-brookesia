from __future__ import annotations

from typing import Any, Dict


Contract = Dict[str, Any]


HELPER_CONTRACTS: list[Contract] = [
    {
        "category": "service",
        "slug": "audio",
        "title": {"en": "Audio", "zh_CN": "音频"},
        "omit_overview": True,
        "interface_title": {"en": "Service Interfaces", "zh_CN": "服务接口"},
        "interface_as_parent": True,
        "overview": {
            "en": (
                "The Audio contract is the helper-first interface for playback, recording, "
                "codec, and AFE workflows exposed through the service framework."
            ),
            "zh_CN": "Audio 合约是面向用户的标准音频接口，覆盖播放、录音、编解码与 AFE 工作流。",
        },
        "include_header": "brookesia/service_helper/audio.hpp",
        "header_path": "service/brookesia_service_helper/include/brookesia/service_helper/audio.hpp",
        "helper_type": "esp_brookesia::service::helper::Audio",
        "api_reference_doc": "/service/helper/base",
        "page_doc": "/service/audio",
        "schema_sections": [
            {
                "key": "main",
                "title": {"en": "Contract Schema", "zh_CN": "接口 Schema"},
                "functions_accessor": "get_function_schemas()",
                "events_accessor": "get_event_schemas()",
            }
        ],
        "implementations": [
            {
                "doc": "/service/audio/service_audio",
                "title": {"en": "Service Audio Provider", "zh_CN": "Service Audio 提供者"},
                "summary": {
                    "en": "Default audio service provider implementation built on the service framework.",
                    "zh_CN": "基于 service 框架的默认音频服务提供者实现。",
                },
            }
        ],
    },
    {
        "category": "service",
        "slug": "device",
        "title": {"en": "Device Control", "zh_CN": "设备控制"},
        "omit_overview": True,
        "interface_title": {"en": "Service Interfaces", "zh_CN": "服务接口"},
        "interface_as_parent": True,
        "overview": {
            "en": (
                "The Device contract is the helper-first application-layer interface for "
                "controlling and querying HAL-backed device capabilities."
            ),
            "zh_CN": "Device 合约是面向应用层的 HAL 设备控制与状态查询标准接口。",
        },
        "include_header": "brookesia/service_helper/device.hpp",
        "header_path": "service/brookesia_service_helper/include/brookesia/service_helper/device.hpp",
        "helper_type": "esp_brookesia::service::helper::Device",
        "api_reference_doc": "/service/helper/base",
        "page_doc": "/service/device",
        "schema_sections": [
            {
                "key": "main",
                "title": {"en": "Contract Schema", "zh_CN": "接口 Schema"},
                "functions_accessor": "get_function_schemas()",
                "events_accessor": "get_event_schemas()",
            }
        ],
        "implementations": [
            {
                "doc": "/service/device/service_device",
                "title": {"en": "Service Device Provider", "zh_CN": "Service Device 提供者"},
                "summary": {
                    "en": "Default device control provider that exposes HAL capabilities through the service framework.",
                    "zh_CN": "通过 service 框架暴露 HAL 能力的默认设备控制服务提供者实现。",
                },
            }
        ],
    },
    {
        "category": "service",
        "slug": "nvs",
        "title": {"en": "NVS", "zh_CN": "NVS"},
        "omit_overview": True,
        "interface_title": {"en": "Service Interfaces", "zh_CN": "服务接口"},
        "interface_as_parent": True,
        "overview": {
            "en": "The NVS contract is the helper-first interface for namespace-based key-value storage.",
            "zh_CN": "NVS 合约是面向用户的命名空间键值存储标准接口。",
        },
        "include_header": "brookesia/service_helper/nvs.hpp",
        "header_path": "service/brookesia_service_helper/include/brookesia/service_helper/nvs.hpp",
        "helper_type": "esp_brookesia::service::helper::NVS",
        "api_reference_doc": "/service/helper/base",
        "page_doc": "/service/nvs",
        "schema_sections": [
            {
                "key": "main",
                "title": {"en": "Contract Schema", "zh_CN": "接口 Schema"},
                "functions_accessor": "get_function_schemas()",
                "events_accessor": "get_event_schemas()",
            }
        ],
        "implementations": [
            {
                "doc": "/service/nvs/service_nvs",
                "title": {"en": "Service NVS Provider", "zh_CN": "Service NVS 提供者"},
                "summary": {
                    "en": "Default NVS-backed provider implementation for the helper contract.",
                    "zh_CN": "helper 合约默认使用的 NVS 后端提供者实现。",
                },
            }
        ],
    },
    {
        "category": "service",
        "slug": "sntp",
        "title": {"en": "SNTP", "zh_CN": "SNTP"},
        "omit_overview": True,
        "interface_title": {"en": "Service Interfaces", "zh_CN": "服务接口"},
        "interface_as_parent": True,
        "overview": {
            "en": "The SNTP contract is the helper-first interface for time synchronization and timezone control.",
            "zh_CN": "SNTP 合约是面向用户的时间同步与时区配置标准接口。",
        },
        "include_header": "brookesia/service_helper/sntp.hpp",
        "header_path": "service/brookesia_service_helper/include/brookesia/service_helper/sntp.hpp",
        "helper_type": "esp_brookesia::service::helper::SNTP",
        "api_reference_doc": "/service/helper/base",
        "page_doc": "/service/sntp",
        "schema_sections": [
            {
                "key": "main",
                "title": {"en": "Contract Schema", "zh_CN": "接口 Schema"},
                "functions_accessor": "get_function_schemas()",
                "events_accessor": "get_event_schemas()",
            }
        ],
        "implementations": [
            {
                "doc": "/service/sntp/service_sntp",
                "title": {"en": "Service SNTP Provider", "zh_CN": "Service SNTP 提供者"},
                "summary": {
                    "en": "Default SNTP provider implementation for the helper contract.",
                    "zh_CN": "helper 合约默认使用的 SNTP 提供者实现。",
                },
            }
        ],
    },
    {
        "category": "service",
        "slug": "video",
        "title": {"en": "Video", "zh_CN": "视频"},
        "omit_overview": True,
        "interface_title": {"en": "Service Interfaces", "zh_CN": "服务接口"},
        "interface_as_parent": True,
        "overview": {
            "en": (
                "The Video contract exposes helper-first encoder and decoder interfaces for the "
                "service framework."
            ),
            "zh_CN": "Video 合约以 helper 优先方式暴露编码器与解码器标准接口。",
        },
        "include_header": "brookesia/service_helper/video.hpp",
        "header_path": "service/brookesia_service_helper/include/brookesia/service_helper/video.hpp",
        "helper_type": "esp_brookesia::service::helper::Video",
        "api_reference_doc": "/service/helper/base",
        "page_doc": "/service/video",
        "schema_sections": [
            {
                "key": "encoder",
                "title": {"en": "Encoder", "zh_CN": "编码器"},
                "name_type": "esp_brookesia::service::helper::VideoEncoder<0>",
                "functions_accessor": "get_encoder_function_schemas()",
                "events_accessor": "get_encoder_event_schemas()",
            },
            {
                "key": "decoder",
                "title": {"en": "Decoder", "zh_CN": "解码器"},
                "name_type": "esp_brookesia::service::helper::VideoDecoder<0>",
                "functions_accessor": "get_decoder_function_schemas()",
                "events_accessor": "get_decoder_event_schemas()",
            },
        ],
        "implementations": [
            {
                "doc": "/service/video/service_video",
                "title": {"en": "Service Video Provider", "zh_CN": "Service Video 提供者"},
                "summary": {
                    "en": "Default video provider implementation and codec-specific headers.",
                    "zh_CN": "默认视频提供者实现及其编解码相关头文件。",
                },
            }
        ],
    },
    {
        "category": "service",
        "slug": "wifi",
        "title": {"en": "Wi-Fi", "zh_CN": "Wi-Fi"},
        "omit_overview": True,
        "interface_title": {"en": "Service Interfaces", "zh_CN": "服务接口"},
        "interface_as_parent": True,
        "overview": {
            "en": (
                "The Wi-Fi contract is the helper-first interface for station, scan, SoftAP, "
                "and provisioning workflows."
            ),
            "zh_CN": "Wi-Fi 合约是面向用户的标准接口，覆盖 STA、扫描、SoftAP 与配网工作流。",
        },
        "include_header": "brookesia/service_helper/wifi.hpp",
        "header_path": "service/brookesia_service_helper/include/brookesia/service_helper/wifi.hpp",
        "helper_type": "esp_brookesia::service::helper::Wifi",
        "api_reference_doc": "/service/helper/wifi",
        "page_doc": "/service/wifi",
        "schema_sections": [
            {
                "key": "main",
                "title": {"en": "Contract Schema", "zh_CN": "接口 Schema"},
                "functions_accessor": "get_function_schemas()",
                "events_accessor": "get_event_schemas()",
            }
        ],
        "implementations": [
            {
                "doc": "/service/wifi/service_wifi",
                "title": {"en": "Service Wi-Fi Provider", "zh_CN": "Service Wi-Fi 提供者"},
                "summary": {
                    "en": "Default Wi-Fi provider implementation, state machine, and HAL-specific pages.",
                    "zh_CN": "默认 Wi-Fi 提供者实现，以及其状态机和 HAL 相关页面。",
                },
            }
        ],
    },
    {
        "category": "service",
        "slug": "emote",
        "title": {"en": "Emote", "zh_CN": "表情"},
        "omit_overview": True,
        "interface_title": {"en": "Service Interfaces", "zh_CN": "服务接口"},
        "interface_as_parent": True,
        "overview": {
            "en": (
                "The Emote contract is the helper-first interface for display expressions, emoji, "
                "animations, and message overlays."
            ),
            "zh_CN": "表情合约是面向用户的标准接口，用于表情、动画、消息覆盖层等显示控制。",
        },
        "include_header": "brookesia/service_helper/expression/emote.hpp",
        "header_path": "service/brookesia_service_helper/include/brookesia/service_helper/expression/emote.hpp",
        "helper_type": "esp_brookesia::service::helper::ExpressionEmote",
        "api_reference_doc": "/service/helper/base",
        "page_doc": "/service/emote",
        "schema_sections": [
            {
                "key": "main",
                "title": {"en": "Contract Schema", "zh_CN": "接口 Schema"},
                "functions_accessor": "get_function_schemas()",
                "events_accessor": "get_event_schemas()",
            }
        ],
        "implementations": [
            {
                "doc": "/expression/emote/index",
                "title": {"en": "Expression Emote Provider", "zh_CN": "Expression Emote 提供者"},
                "summary": {
                    "en": "Backing expression component that implements the emote service contract.",
                    "zh_CN": "实现该表情服务合约的 expression 组件后端。",
                },
            }
        ],
    },
    {
        "category": "agent",
        "slug": "manager",
        "title": {"en": "Agent Manager", "zh_CN": "代理管理器"},
        "omit_overview": True,
        "interface_title": {"en": "Service Interfaces", "zh_CN": "服务接口"},
        "interface_as_parent": True,
        "overview": {
            "en": (
                "The Agent Manager contract is the helper-first interface for agent lifecycle, "
                "chat mode, speaking state, and shared agent events."
            ),
            "zh_CN": "Agent Manager 合约是面向用户的标准接口，用于代理生命周期、聊天模式与通用事件。",
        },
        "include_header": "brookesia/agent_helper/manager.hpp",
        "header_path": "agent/brookesia_agent_helper/include/brookesia/agent_helper/manager.hpp",
        "helper_type": "esp_brookesia::agent::helper::Manager",
        "api_reference_doc": "/agent/helper/manager",
        "page_doc": "/agent/manager/index",
        "schema_sections": [
            {
                "key": "main",
                "title": {"en": "Contract Schema", "zh_CN": "接口 Schema"},
                "functions_accessor": "get_function_schemas()",
                "events_accessor": "get_event_schemas()",
            }
        ],
        "implementations": [],
    },
    {
        "category": "agent",
        "slug": "coze",
        "title": {"en": "Coze", "zh_CN": "Coze"},
        "omit_overview": True,
        "interface_title": {"en": "Service Interfaces", "zh_CN": "服务接口"},
        "interface_as_parent": True,
        "overview": {
            "en": "The Coze contract is the helper-first interface for Coze agent metadata and robot selection.",
            "zh_CN": "Coze 合约是面向用户的标准接口，用于 Coze 代理信息与机器人选择。",
        },
        "include_header": "brookesia/agent_helper/coze.hpp",
        "header_path": "agent/brookesia_agent_helper/include/brookesia/agent_helper/coze.hpp",
        "helper_type": "esp_brookesia::agent::helper::Coze",
        "api_reference_doc": "/agent/helper/coze",
        "page_doc": "/agent/coze",
        "schema_sections": [
            {
                "key": "main",
                "title": {"en": "Contract Schema", "zh_CN": "接口 Schema"},
                "functions_accessor": "get_function_schemas()",
                "events_accessor": "get_event_schemas()",
            }
        ],
        "implementations": [],
    },
    {
        "category": "agent",
        "slug": "openai",
        "title": {"en": "OpenAI", "zh_CN": "OpenAI"},
        "omit_overview": True,
        "interface_title": {"en": "Service Interfaces", "zh_CN": "服务接口"},
        "interface_as_parent": True,
        "overview": {
            "en": "The OpenAI contract is the helper-first interface for OpenAI agent integration.",
            "zh_CN": "OpenAI 合约是面向用户的标准接口，用于 OpenAI 代理集成。",
        },
        "include_header": "brookesia/agent_helper/openai.hpp",
        "header_path": "agent/brookesia_agent_helper/include/brookesia/agent_helper/openai.hpp",
        "helper_type": "esp_brookesia::agent::helper::Openai",
        "api_reference_doc": "/agent/helper/openai",
        "page_doc": "/agent/openai",
        "schema_sections": [
            {
                "key": "main",
                "title": {"en": "Contract Schema", "zh_CN": "接口 Schema"},
                "functions_accessor": "get_function_schemas()",
                "events_accessor": "get_event_schemas()",
            }
        ],
        "implementations": [],
    },
    {
        "category": "agent",
        "slug": "xiaozhi",
        "title": {"en": "Xiaozhi", "zh_CN": "小智"},
        "omit_overview": True,
        "interface_title": {"en": "Service Interfaces", "zh_CN": "服务接口"},
        "interface_as_parent": True,
        "overview": {
            "en": "The Xiaozhi contract is the helper-first interface for Xiaozhi-specific tools and activation flows.",
            "zh_CN": "小智合约是面向用户的标准接口，用于小智特有工具与激活流程。",
        },
        "include_header": "brookesia/agent_helper/xiaozhi.hpp",
        "header_path": "agent/brookesia_agent_helper/include/brookesia/agent_helper/xiaozhi.hpp",
        "helper_type": "esp_brookesia::agent::helper::XiaoZhi",
        "api_reference_doc": "/agent/helper/xiaozhi",
        "page_doc": "/agent/xiaozhi",
        "schema_sections": [
            {
                "key": "main",
                "title": {"en": "Contract Schema", "zh_CN": "接口 Schema"},
                "functions_accessor": "get_function_schemas()",
                "events_accessor": "get_event_schemas()",
            }
        ],
        "implementations": [],
    },
]


def get_contract(category: str, slug: str) -> Contract:
    for contract in HELPER_CONTRACTS:
        if contract["category"] == category and contract["slug"] == slug:
            return contract
    raise KeyError(f"Unknown helper contract: {category}/{slug}")
