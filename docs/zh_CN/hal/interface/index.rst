.. _hal-interface-index-sec-00:

HAL 接口
=============

:link_to_translation:`en:[English]`

- 组件注册表： `espressif/brookesia_hal_interface <https://components.espressif.com/components/espressif/brookesia_hal_interface>`_
- 公共头文件： ``#include "brookesia/hal_interface.hpp"``

.. _hal-interface-index-sec-01:

概述
----

``brookesia_hal_interface`` 是 ESP-Brookesia 的硬件抽象基础组件，在「板级实现」与「上层业务或其它子系统」之间提供统一抽象，主要能力包括：

- **设备与接口模型**：用「设备」聚合硬件单元，用「接口」表达可复用能力（开发板信息、电池、编解码播放/录音、显示面板/触摸/背光、存储卷等），二者职责清晰、可组合
- **插件式注册**：具体设备与接口实现通过注册表登记，运行时按名称解析，避免业务侧硬编码实现类
- **探测与生命周期**：设备先探测是否可用，再初始化；支持批量或按名称单独初始化、对称反初始化
- **全局发现**：设备可按插件名或设备逻辑名解析；接口可在全局范围内按类型枚举或按设备内名称获取
- **常用 HAL 声明**：内置音频、显示、存储等接口的抽象定义，具体行为由适配层实现

.. _hal-interface-index-sec-02:

功能特性
--------

.. _hal-interface-index-sec-03-dup2:

设备与接口
^^^^^^^^^^

**设备** 表示一块可独立管理的硬件单元（例如某路音频编解码、某套显示子系统、某套存储子系统）。设备在正式工作前需要 **探测** ：仅当硬件在当前环境下可用时，才进入初始化。初始化阶段，设备在内部收集需要对外暴露的 **接口实例** 。

**接口** 表示一类稳定的能力边界（例如开发板信息、电池状态与充电控制、编解码播放、录音、面板绘图、触摸采样、背光控制、存储介质与文件系统发现等）。同一抽象类型下可以存在多份实例，通过注册时使用的名称区分；名称在全局接口注册表中唯一标识一份实例。

设备与接口的实现类通过项目中的插件机制登记；框架在运行时根据名称创建或取得实例，上层只需依赖本组件中的抽象类型与约定。

.. _hal-interface-index-sec-04-dup2:

注册与生命周期
^^^^^^^^^^^^^^

批量初始化时，框架按注册表逐项处理：对每台设备依次执行探测、设备侧初始化；初始化成功后，由框架将该设备声明的接口同步登记到全局接口表。某一设备探测失败或初始化失败时，通常仅影响该设备，其余设备可继续处理。

也支持仅针对 **某一插件注册名** 做初始化或反初始化。反初始化时，先撤销该设备相关接口在全局表中的登记，再执行设备自定义清理，并释放设备内部对接口的持有关系。

典型流程如下：

#. 已注册设备进入 **探测**；
#. 探测通过则 **初始化**，并将接口登记到全局注册表；探测不通过则 **跳过该设备**。

.. _hal-interface-index-sec-05-dup2:

发现与命名
^^^^^^^^^^

与设备相关的名称分两类，用途不同：

.. list-table::
   :header-rows: 1
   :widths: 18 82

   * - 名称类型
     - 含义
   * - 插件名
     - 插件注册表中的键，用于按注册项直接定位设备实例
   * - 设备名
     - 设备对象自身携带的逻辑名，可与插件名相同或不同

按其中任一路径都可以在运行时解析到对应设备。

对接口的访问面向 **全局接口注册表**：可按接口类型列出当前所有可转换的实例，也可取得某类型下首个匹配实例（名称与实例成对返回）。若已从设备对象入手，也可仅在该设备已发布的接口集合中，用登记时使用的完整接口名取回能力。

多设备并存时，建议为接口注册名增加可区分设备的前缀或其它命名空间，避免全局冲突。

.. _hal-interface-index-sec-06-dup2:

内置能力范畴
^^^^^^^^^^^^

头文件中提供常用 HAL 接口的抽象定义，涵盖开发板信息、电池与充电控制、音频编解码播放与录音、显示面板与触摸与背光、以及存储文件系统发现等。它们描述静态信息、能力参数与虚接口约定；具体寄存器操作、总线与时序由板级适配或其它组件完成。

以下接口头文件可通过 ``brookesia/hal_interface/interfaces.hpp`` 一次性引入，也可通过聚合入口 ``brookesia/hal_interface.hpp`` 同时引入设备基类：

.. list-table::
   :header-rows: 1
   :widths: 45 55

   * - 头文件
     - 主要类型
   * - ``audio/codec_player.hpp``
     - ``AudioCodecPlayerIface``
   * - ``audio/codec_recorder.hpp``
     - ``AudioCodecRecorderIface``
   * - ``display/backlight.hpp``
     - ``DisplayBacklightIface``
   * - ``display/panel.hpp``
     - ``DisplayPanelIface``
   * - ``display/touch.hpp``
     - ``DisplayTouchIface``
   * - ``general/board_info.hpp``
     - ``BoardInfoIface``
   * - ``power/battery.hpp``
     - ``PowerBatteryIface``
   * - ``storage/fs.hpp``
     - ``StorageFsIface``

.. _hal-interface-index-sec-03:

设备接口类
~~~~~~~~~~~

组件提供了以下接口类：

- ``AudioCodecPlayerIface``
- ``AudioCodecRecorderIface``
- ``DisplayBacklightIface``
- ``DisplayPanelIface``
- ``DisplayTouchIface``
- ``BoardInfoIface``
- ``PowerBatteryIface``
- ``StorageFsIface``

.. _hal-interface-index-sec-04:

API 参考
--------

.. _hal-interface-index-sec-05:

设备 & 接口基类
^^^^^^^^^^^^^^^^

.. toctree::
   :maxdepth: 1

   设备基类 <device>
   接口基类 <interface>

.. _hal-interface-index-sec-06:

音频接口类
^^^^^^^^^^

.. toctree::
   :maxdepth: 1

   播放器 <audio/codec_player>
   录音器 <audio/codec_recorder>

.. _hal-interface-index-sec-11:

显示接口类
^^^^^^^^^^

.. toctree::
   :maxdepth: 1

   面板 <display/panel>
   触摸 <display/touch>
   背光 <display/backlight>

.. _hal-interface-index-sec-12:

通用接口类
^^^^^^^^^^

.. toctree::
   :maxdepth: 1

   开发板信息 <general/board_info>

.. _hal-interface-index-sec-13:

电源接口类
^^^^^^^^^^

.. toctree::
   :maxdepth: 1

   电池 <power/battery>

.. _hal-interface-index-sec-14:

存储接口类
^^^^^^^^^^

.. toctree::
   :maxdepth: 1

   文件系统 <storage/fs>
