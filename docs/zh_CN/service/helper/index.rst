.. _service-helper-index-sec-00:

服务辅助
========

:link_to_translation:`en:[English]`

- 组件注册表： `espressif/brookesia_service_helper <https://components.espressif.com/components/espressif/brookesia_service_helper>`_
- 公共头文件： ``#include "brookesia/service_helper.hpp"``

.. _service-helper-index-sec-01:

概述
----

`brookesia_service_helper` 是 ESP-Brookesia 服务层面向应用的统一辅助接口组件，提供各类通用服务的 helper 包装与契约入口。

.. _service-helper-index-sec-02:

特性
----

- 统一入口：通过一组 helper 类封装不同服务能力，降低业务层接入复杂度。
- 契约友好：与函数/事件 schema 体系配套，便于进行标准化调用与文档生成。
- 组件解耦：应用面向 helper 编程，减少对具体 service provider 实现的直接依赖。
- 可扩展：支持为新增服务能力持续扩展 helper 子模块与对应 API 文档。

.. _service-helper-index-sec-03:

模块介绍
--------

.. toctree::
   :maxdepth: 1

   Helper 基类 <base>
   音频 <audio>
   设备控制 <device>
   表情 <emote>
   NVS <nvs>
   SNTP <sntp>
   视频 <video>
   Wi-Fi <wifi>
