.. _gui-interface-json-ui-contributing-sec-00:

协议维护说明
============================================================

:link_to_translation:`en:[English]`

本页面向协议与文档的维护者，集中说明文档导航、阅读路线、推荐目录结构、编写约定，以及修改协议时的同步检查清单。框架使用者通常无需阅读本页。

文档导航
--------------------

文档按主题分组，每个分组目录下都有一个 index 作为本组导航。

.. list-table::
   :header-rows: 1

   * - 分组
     - 入口
     - 负责内容
   * - 入口
     - :doc:`index`
     - 协议总览、全局约束、模块导航
   * - document
     - :doc:`document/index`
     - document 加载层：``root.json``、``variants[]``、``when``、``Environment``、单位换算
   * - assets
     - :doc:`assets/index`
     - asset 统一模型，document 资产与 Runtime 全局资源边界
   * - view
     - :doc:`view/index`
     - ``view`` 通用字段与各控件类型
   * - styling
     - :doc:`styling/index`
     - ``layout``、``placement``、``style`` 与各控件 ``props``
   * - interaction
     - :doc:`interaction/index`
     - ``bindings``、``events``、``animations``、``screenFlow``
   * - runtime
     - :doc:`runtime`
     - 运行时资源模型、``document_id + absolute_path`` API

阅读路线
--------------------

第一次接触本协议，建议按以下顺序阅读：

#. :doc:`document/root`
#. :doc:`assets/index`
#. :doc:`view/index`
#. :doc:`styling/layout`
#. :doc:`styling/placement`
#. :doc:`styling/style`
#. :doc:`styling/props/index`
#. :doc:`interaction/bindings`
#. :doc:`interaction/events`
#. :doc:`interaction/animations`
#. :doc:`interaction/screen_flow`
#. :doc:`runtime`

先看 ``root`` / ``assets`` / ``view`` 建立整体心智模型，再看 ``styling`` 与 ``interaction`` 的字段细节，最后看 ``runtime`` 的对接 API。

推荐目录结构
--------------------

目录名不影响协议本身；一个文件是常量资源还是 UI 资源，由文件内 ``type`` 决定，而非目录名。下面是示例工程仍在使用的组织方式：

.. code-block:: text

   assets/
     documents/
       settings_home/
         root.json
         nodes/
           settings_home.json
           nav_item.json
     shared/
       constants/
         base.json
         theme.json
       fonts/
         default.json
       images/
         logo.json

文档编写约定
--------------------

新增或修改文档时遵循以下约定，保证全集风格一致。

- 正文以中文为主，技术术语、字段名、枚举值、API 保留英文。
- 一级标题中文优先；叶子控件页可用控件 ``type`` 名作标题（如 ``button``）。
- JSON 字段名保留 ``camelCase``；JSON 概念名小写（``view`` / ``document`` / ``variant``）；C++ 类型首字母大写（``Runtime`` / ``View`` / ``Document``）；渲染后端统一写作 ``backend``。
- 模块页结构：概览 → 相关文档 → 字段表 → 字段说明 → 示例。
- 叶子控件页结构：概览 → 相关文档 → 专属 props → 常见事件 →（可选）示例。

字段表表头按用途固定：

.. list-table::
   :header-rows: 1

   * - 用途
     - 表头
   * - 协议 / asset 字段
     - ``Key | 类型 | 默认值 | 是否必填 | 说明``
   * - layout / placement / style / animations
     - ``Key | 类型 | 默认值 | UI 影响 | Backend 行为/限制``
   * - 控件 props
     - ``Key | 类型 | 默认值 | Binding | UI 影响/限制``

修改协议时的同步检查
--------------------

文档、代码、资源三者必须保持一致。修改协议时至少同步检查以下文档与源码：

- 各分组文档：``document`` / ``assets`` / ``view`` / ``styling`` / ``interaction`` / ``runtime``
- ``src/parser.cpp``、``src/validator.cpp``
- ``include/brookesia/gui_interface/document.hpp``
- ``include/brookesia/gui_interface/widget.hpp``
- ``include/brookesia/gui_interface/runtime.hpp``
