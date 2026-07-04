.. _index-sec-00:

ESP-Brookesia Programming Guide
====================================

:link_to_translation:`zh_CN:[中文]`

.. image:: ../_static/brookesia_logo.png
   :alt: ESP-Brookesia Logo
   :width: 800px
   :align: center

|

.. list-table::
   :widths: 33 33 34

   * - |Getting Started|_
     - |Utils Components|_
     - |Hardware Components|_
   * - `Getting Started`_
     - `Utils Components`_
     - `Hardware Components`_
   * - |Service Components|_
     - |GUI Components|_
     - |Runtime Components|_
   * - `Service Components`_
     - `GUI Components`_
     - `Runtime Components`_
   * - |System Components|_
     - |App Components|_
     -
   * - `System Components`_
     - `App Components`_
     -

.. |Getting Started| image:: ../_static/index/getting_started.png
.. _Getting Started: getting_started.html
.. _index-nav-getting-started: getting_started.html

.. |Utils Components| image:: ../_static/index/utils.png
.. _Utils Components: utils/index.html
.. _index-nav-utils: utils/index.html

.. |Hardware Components| image:: ../_static/index/hal.png
.. _Hardware Components: hal/index.html
.. _index-nav-hal: hal/index.html

.. |Service Components| image:: ../_static/index/service.png
.. _Service Components: service/index.html
.. _index-nav-service: service/index.html

.. |GUI Components| image:: ../_static/index/gui.png
.. _GUI Components: gui/index.html
.. _index-nav-gui: gui/index.html

.. |Runtime Components| image:: ../_static/index/runtime.png
.. _Runtime Components: runtime/index.html
.. _index-nav-runtime: runtime/index.html

.. |System Components| image:: ../_static/index/system.png
.. _System Components: system/index.html
.. _index-nav-system: system/index.html

.. |App Components| image:: ../_static/index/app.png
.. _App Components: app/index.html
.. _index-nav-app: app/index.html

.. _index-sec-01:

.. rubric:: Overview

ESP-Brookesia is a full-stack development platform for AIoT and HMI products. It organizes hardware abstraction, service framework, runtimes, GUI, system framework, app components, and AI capabilities as ESP-IDF components so products can compose and reuse platform capabilities as needed.

The overall ESP-Brookesia architecture is shown below:

.. image:: ../_static/architecture_diagram_en.svg
  :alt: ESP-Brookesia Overall Architecture
  :width: 100%
  :align: center

.. raw:: html

  <br>

.. raw:: latex

  \vspace{1em}

.. _index-sec-02:

.. rubric:: Main Architecture Layers

- **Hardware Abstraction**: hides platform and hardware differences, unifies device access, and supports ESP devices and PC simulation.
- **Service Framework**: unifies service registration, function calls, event publish/subscribe, and MCP / Agent capability connections.
- **Runtime and GUI**: supports multi-runtime apps and the JSON UI-driven GUI model.
- **System Framework**: provides the product-level system framework that integrates GUI, Runtime, services, and app lifecycle.
- **App Ecosystem**: covers third-party app supply, upload and publish, device-side distribution, and installation.

.. _index-sec-03:

.. rubric:: Main Architecture Features

- **Hardware Decoupling**: HAL and PC simulation reduce the impact of hardware differences on upper-layer apps and systems.
- **App Development Efficiency**: service-based design, multi-runtime support, and declarative GUI improve app development and validation efficiency.
- **Productization and Ecosystem**: the system and app frameworks combine with the app-store mechanism to support scalable product delivery.

.. _index-sec-04:

.. rubric:: AI-Assisted Development and App Ecosystem

- **AI Development Validation Loop**: AI Workflow and PC Validation form a development validation loop. Developers review, tune, and supplement the result to generate a publishable app.
- **Publishing and Distribution Flow**: the app then moves through Upload & Publish, App Store, and Device Runtime to complete publishing, discovery, download, installation, and execution.

.. toctree::
   :hidden:

   Getting Started <getting_started>
   Utils Components <utils/index>
   Hardware Components <hal/index>
   Service Components <service/index>
   App Components <app/index>
   GUI Components <gui/index>
   Runtime Components <runtime/index>
   System Components <system/index>
