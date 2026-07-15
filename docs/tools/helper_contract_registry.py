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
        "include_header": "brookesia/service_helper/media/audio.hpp",
        "header_path": "service/framework/brookesia_service_helper/include/brookesia/service_helper/media/audio.hpp",
        "helper_type": "esp_brookesia::service::helper::Audio",
        "api_reference_doc": "/service/framework/helper/base",
        "page_doc": "/service/audio",
        "schema_sections": [
            {
                "key": "playback",
                "title": {"en": "Playback", "zh_CN": "播放"},
                "name_type": "esp_brookesia::service::helper::AudioPlayback",
                "functions_accessor": "get_playback_function_schemas()",
                "events_accessor": "get_playback_event_schemas()",
            },
            {
                "key": "encoder",
                "title": {"en": "Encoder", "zh_CN": "编码器"},
                "name_type": "esp_brookesia::service::helper::AudioEncoder<0>",
                "functions_accessor": "get_encoder_function_schemas()",
                "events_accessor": "get_encoder_event_schemas()",
            },
            {
                "key": "decoder",
                "title": {"en": "Decoder", "zh_CN": "解码器"},
                "name_type": "esp_brookesia::service::helper::AudioDecoder<0>",
                "functions_accessor": "get_decoder_function_schemas()",
                "events_accessor": "get_decoder_event_schemas()",
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
        "slug": "display",
        "title": {"en": "Display", "zh_CN": "显示"},
        "omit_overview": True,
        "interface_title": {"en": "Service Interfaces", "zh_CN": "服务接口"},
        "interface_as_parent": True,
        "overview": {
            "en": "The Display contract is the helper-first interface for outputs, sources, touch, and backlight control.",
            "zh_CN": "Display 合约是面向用户的显示输出、显示源、触摸与背光控制标准接口。",
        },
        "include_header": "brookesia/service_helper/media/display.hpp",
        "header_path": "service/framework/brookesia_service_helper/include/brookesia/service_helper/media/display.hpp",
        "helper_type": "esp_brookesia::service::helper::Display",
        "api_reference_doc": "/service/framework/helper/display",
        "page_doc": "/service/display",
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
        "category": "service",
        "slug": "http",
        "title": {"en": "HTTP", "zh_CN": "HTTP"},
        "omit_overview": True,
        "interface_title": {"en": "Service Interfaces", "zh_CN": "服务接口"},
        "interface_as_parent": True,
        "overview": {
            "en": "The HTTP contract is the helper-first interface for synchronous and asynchronous HTTP requests.",
            "zh_CN": "HTTP 合约是面向用户的同步与异步 HTTP 请求标准接口。",
        },
        "include_header": "brookesia/service_helper/network/http.hpp",
        "header_path": "service/framework/brookesia_service_helper/include/brookesia/service_helper/network/http.hpp",
        "helper_type": "esp_brookesia::service::helper::Http",
        "api_reference_doc": "/service/framework/helper/http",
        "page_doc": "/service/http",
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
        "category": "service",
        "slug": "nes",
        "title": {"en": "NES", "zh_CN": "NES"},
        "omit_overview": True,
        "interface_title": {"en": "Service Interfaces", "zh_CN": "服务接口"},
        "interface_as_parent": True,
        "overview": {
            "en": "The NES contract is the helper-first interface for NES runtime lifecycle and gamepad control.",
            "zh_CN": "NES 合约是面向用户的 NES 运行时生命周期与手柄控制标准接口。",
        },
        "include_header": "brookesia/service_helper/emulation/nes.hpp",
        "header_path": "service/framework/brookesia_service_helper/include/brookesia/service_helper/emulation/nes.hpp",
        "helper_type": "esp_brookesia::service::helper::Nes",
        "api_reference_doc": "/service/framework/helper/nes",
        "page_doc": "/service/emulation/nes",
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
        "include_header": "brookesia/service_helper/system/device.hpp",
        "header_path": "service/framework/brookesia_service_helper/include/brookesia/service_helper/system/device.hpp",
        "helper_type": "esp_brookesia::service::helper::Device",
        "api_reference_doc": "/service/framework/helper/base",
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
        "slug": "storage",
        "title": {"en": "Storage", "zh_CN": "Storage"},
        "omit_overview": True,
        "interface_title": {"en": "Service Interfaces", "zh_CN": "服务接口"},
        "interface_as_parent": True,
        "overview": {
            "en": "The Storage contract is the helper-first interface for namespace-based key-value storage.",
            "zh_CN": "Storage 合约是面向用户的命名空间键值存储标准接口。",
        },
        "include_header": "brookesia/service_helper/system/storage.hpp",
        "header_path": "service/framework/brookesia_service_helper/include/brookesia/service_helper/system/storage.hpp",
        "helper_type": "esp_brookesia::service::helper::Storage",
        "api_reference_doc": "/service/framework/helper/base",
        "page_doc": "/service/storage",
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
                "doc": "/service/storage/service_storage",
                "title": {"en": "Service Storage Provider", "zh_CN": "Service Storage 提供者"},
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
            "en": "The SNTP contract is the helper-first interface for network time synchronization.",
            "zh_CN": "SNTP 合约是面向用户的网络时间同步标准接口。",
        },
        "include_header": "brookesia/service_helper/network/sntp.hpp",
        "header_path": "service/framework/brookesia_service_helper/include/brookesia/service_helper/network/sntp.hpp",
        "helper_type": "esp_brookesia::service::helper::SNTP",
        "api_reference_doc": "/service/framework/helper/sntp",
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
                    "en": "Default HAL-backed provider implementation for network time synchronization.",
                    "zh_CN": "基于 HAL 的默认网络时间同步服务提供者实现。",
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
        "include_header": "brookesia/service_helper/media/video.hpp",
        "header_path": "service/framework/brookesia_service_helper/include/brookesia/service_helper/media/video.hpp",
        "helper_type": "esp_brookesia::service::helper::Video",
        "api_reference_doc": "/service/framework/helper/base",
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
        "include_header": "brookesia/service_helper/network/wifi.hpp",
        "header_path": "service/framework/brookesia_service_helper/include/brookesia/service_helper/network/wifi.hpp",
        "helper_type": "esp_brookesia::service::helper::Wifi",
        "api_reference_doc": "/service/framework/helper/wifi",
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
        "header_path": "service/framework/brookesia_service_helper/include/brookesia/service_helper/expression/emote.hpp",
        "helper_type": "esp_brookesia::service::helper::ExpressionEmote",
        "api_reference_doc": "/service/framework/helper/base",
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
                "doc": "/service/expression/emote/index",
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
        "include_header": "brookesia/service_helper/agent/manager.hpp",
        "header_path": "service/framework/brookesia_service_helper/include/brookesia/service_helper/agent/manager.hpp",
        "helper_type": "esp_brookesia::service::helper::AgentManager",
        "api_reference_doc": "/service/framework/helper/agent/manager",
        "page_doc": "/service/agent/manager/index",
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
        "include_header": "brookesia/service_helper/agent/coze.hpp",
        "header_path": "service/framework/brookesia_service_helper/include/brookesia/service_helper/agent/coze.hpp",
        "helper_type": "esp_brookesia::service::helper::Coze",
        "api_reference_doc": "/service/framework/helper/agent/coze",
        "page_doc": "/service/agent/coze",
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
        "include_header": "brookesia/service_helper/agent/openai.hpp",
        "header_path": "service/framework/brookesia_service_helper/include/brookesia/service_helper/agent/openai.hpp",
        "helper_type": "esp_brookesia::service::helper::Openai",
        "api_reference_doc": "/service/framework/helper/agent/openai",
        "page_doc": "/service/agent/openai",
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
        "include_header": "brookesia/service_helper/agent/xiaozhi.hpp",
        "header_path": "service/framework/brookesia_service_helper/include/brookesia/service_helper/agent/xiaozhi.hpp",
        "helper_type": "esp_brookesia::service::helper::XiaoZhi",
        "api_reference_doc": "/service/framework/helper/agent/xiaozhi",
        "page_doc": "/service/agent/xiaozhi",
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
