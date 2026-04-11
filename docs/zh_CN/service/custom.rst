.. _service-custom-sec-00:

服务自定义
==========

:link_to_translation:`en:[English]`

- 组件注册表： `espressif/brookesia_service_custom <https://components.espressif.com/components/espressif/brookesia_service_custom>`_
- 公共头文件： ``#include "brookesia/service_custom.hpp"``

.. _service-custom-sec-01:

概述
----

`brookesia_service_custom` 为 ESP-Brookesia 生态系统提供即用的 **CustomService**，支持用户自定义 function 和 event 的注册与调用，无需创建独立的服务组件。

.. _service-custom-sec-02:

典型使用场景
^^^^^^^^^^^^

- **轻量级功能**：对于无需封装为完整 Brookesia 组件的功能（如 LED 控制、PWM、GPIO 翻转、简单传感器等），可通过 CustomService 的接口进行封装和调用。
- **快速原型**：将应用逻辑快速暴露为可调用的 function 或 event，支持本地调用或远程 RPC 访问。
- **可扩展性**：在不修改服务框架的前提下，为应用添加自定义能力。

.. _service-custom-sec-03:

功能特性
--------

- **动态注册**：运行时通过 ``register_function()`` 和 ``register_event()`` 注册 function 和 event。
- **固定 Handler 签名**：Handler 接收 ``FunctionParameterMap``，返回 ``FunctionResult``；支持 lambda、``std::function``、自由函数、仿函数、``std::bind``。
- **事件发布/订阅**：完整的事件生命周期：注册、发布、订阅、注销。
- **ServiceManager 集成**：与 ServiceManager 配合，支持本地调用和远程 RPC。
- **可选 Worker**：可配置任务调度器，实现线程安全执行。

.. _service-custom-sec-04:

API 参考
--------

.. include-build-file:: inc/service/brookesia_service_custom/include/brookesia/service_custom/service_custom.inc
