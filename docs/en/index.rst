.. _index-sec-00:

ESP-Brookesia Programming Guide
===============================

:link_to_translation:`zh_CN:[中文]`

.. image:: ../_static/brookesia_logo.png
   :alt: ESP-Brookesia Logo
   :width: 800px
   :align: center

|

========================  ========================  ========================
|Getting Started|_        |Utils Components|_      |HAL Components|_
------------------------  ------------------------  ------------------------
`Getting Started`_        `Utils Components`_      `HAL Components`_
------------------------  ------------------------  ------------------------
|Service Components|_     |AI Agent Components|_   |AI Expression Components|_
------------------------  ------------------------  ------------------------
`Service Components`_     `AI Agent Components`_   `AI Expression Components`_
========================  ========================  ========================

.. |Getting Started| image:: ../_static/index/getting_started.png
.. _Getting Started: getting_started.html
.. _index-nav-getting-started: getting_started.html

.. |Utils Components| image:: ../_static/index/utils.png
.. _Utils Components: utils/index.html
.. _index-nav-utils: utils/index.html

.. |HAL Components| image:: ../_static/index/hal.png
.. _HAL Components: hal/index.html
.. _index-nav-hal: hal/index.html

.. |Service Components| image:: ../_static/index/service.png
.. _Service Components: service/index.html
.. _index-nav-service: service/index.html

.. |AI Agent Components| image:: ../_static/index/agent.png
.. _AI Agent Components: agent/index.html
.. _index-nav-agent: agent/index.html

.. |AI Expression Components| image:: ../_static/index/expression.png
.. _AI Expression Components: expression/index.html
.. _index-nav-expression: expression/index.html

.. _index-sec-01:

.. rubric:: Overview

ESP-Brookesia is a human-machine interaction development framework for AIoT devices. It is designed to simplify application development and AI capability integration. Built on ESP-IDF and a component-based architecture, it provides full-stack support from hardware abstraction and system services to AI agents, helping developers accelerate the development and time-to-market of HMI and AI products.

.. NOTE::
   "Brookesia" is a genus of chameleons known for camouflage and adaptability, which closely aligns with the goals of ESP-Brookesia. The framework is designed to provide a flexible and scalable solution that can adapt to different hardware devices and application requirements, much like the Brookesia chameleon itself.

The main features of ESP-Brookesia include:

- **Native ESP-IDF support**: Developed in C/C++ with deep integration into the ESP-IDF development workflow and the ESP Registry component catalog, fully leveraging Espressif's open-source component ecosystem.
- **Extensible hardware abstraction**: Defines unified hardware interfaces for audio, display, touch, storage, and more, and provides a board adaptation layer for fast porting across hardware platforms.
- **Rich system services**: Offers ready-to-use system-level services such as Wi-Fi connectivity and audio/video processing. A Manager + Helper architecture is used for decoupling and extension, and also provides support for Agent CLI.
- **Multi-LLM backend integration**: Includes built-in adapters for mainstream AI platforms such as OpenAI, Coze, and XiaoZhi, with unified agent management and lifecycle control.
- **MCP protocol support**: Exposes device service capabilities to large language models through Function Calling / MCP, enabling unified communication between LLMs and system services.
- **AI expression capabilities**: Supports emote sets, animation sets, and other visual AI expressions to provide rich visual feedback for anthropomorphic interaction.

.. _index-sec-02:

.. rubric:: Functional Architecture

ESP-Brookesia adopts a layered architecture. From bottom to top, it consists of **Environment & Dependencies**, **Service & Framework**, and **Application Layer**, as shown below:

.. image:: ../_static/framework_overview.svg
   :alt: ESP-Brookesia framework overview
   :width: 800px
   :align: center

|

.. _index-sec-03:

.. rubric:: Environment & Dependencies

The runtime foundation of the framework. ``ESP-IDF`` provides the build toolchain, real-time operating system, and peripheral drivers, while ``ESP Registry`` centrally manages the distribution and version evolution of framework components and their third-party dependencies.

.. _index-sec-04:

.. rubric:: Service & Framework

The core layer of the framework. It connects downward to the environment and dependencies and provides standardized service interfaces upward to applications and AI agents, covering utilities, hardware abstraction, system services, AI agents, and expression modules.

- **Utils**: Provides common foundational capabilities for upper-layer modules. ``General Utils`` includes the logging system, error checking, state machine, task scheduler, plugin manager, and memory/thread/time profilers. ``MCP Utils`` acts as the bridge between ESP-Brookesia services and the MCP engine, exposing registered service functions as standard MCP tools so large language models can call device capabilities.
- **HAL**: Defines unified hardware access interfaces and provides board-level adaptation. ``Interface`` defines standardized hardware APIs for audio playback/recording, display panels and touch, status LEDs, and storage file systems. ``Adaptor`` provides implementations for specific development boards and completes hardware resource initialization and mapping. ``Boards`` provides board-level YAML configuration that describes the peripheral topology, pin assignments, and driver parameters of each board.
- **General Service**: Provides system-level foundational services, including ``Wi-Fi`` connection management, ``Audio`` capture and playback, ``Video`` codec processing, ``NVS`` non-volatile storage, ``SNTP`` network time synchronization, and a ``Custom`` service extension mechanism. All services use the Manager + Helper architecture and support both local calls and RPC-based remote communication.
- **AI Agent Framework**: Provides a unified management framework for AI agents, with built-in adapters for mainstream AI platforms such as ``Coze``, ``OpenAI``, and ``XiaoZhi``. Through the ``Function Calling / MCP`` protocol, it enables bidirectional communication between large language models and system services, allowing LLMs to perceive and invoke device capabilities.
- **AI Expression**: Provides visual expression capabilities for AI interaction scenarios, including ``Emote`` sets and animation control, delivering rich visual feedback for anthropomorphic interaction.
- **System** *(planned)*: Provides GUI, system management, and application framework support for different product forms such as mobile devices, speakers, and robots.
- **Runtime** *(planned)*: Provides runtime support for WebAssembly, Python, Lua, and more, enabling dynamic application loading and execution.

.. _index-sec-05:

.. rubric:: Application Layer

The final products and projects built on top of the layers above:

- ``General Projects``: General project templates for product development that integrate framework components and can be used directly as the basis for product development.
- ``System Apps`` *(planned)*: A collection of system-level applications for products, including settings, AI assistants, app stores, and more, which can be selectively integrated as needed.

.. toctree::
   :hidden:

   Getting Started <getting_started>
   Utils Components <utils/index>
   HAL Components <hal/index>
   Service Components <service/index>
   AI Agent Components <agent/index>
   AI Expression Components <expression/index>
