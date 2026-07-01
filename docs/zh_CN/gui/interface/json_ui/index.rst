.. _gui-interface-json-ui-index-sec-00:

GUI 接口
============================================================

:link_to_translation:`en:[English]`

概览
--------------------

GUI 接口层定义应用 UI 与具体渲染后端之间的公共契约，覆盖文档模型、资源命名、动作绑定、运行时资源和后端适配边界。

JSON UI 是当前公开的后端无关界面编排协议：应用用 JSON 描述界面结构、资源、样式与交互，由 Runtime 解析为文档模型，再交给具体后端渲染。

本页从 GUI 接口层视角说明 JSON UI 的定位、主线模型与全局约束。各模块的完整字段表、示例与 Runtime 映射见下方分组文档；文档维护与协议同步规范见 :doc:`协议维护说明 <contributing>`。

协议要点：

- 以单个 ``root.json`` 作为 document 入口
- 顶层 ``assets`` 与 ``variants[].assets`` 提供资源
- document 内 asset 一级类型：``constant`` / ``imageSet`` / ``viewScreen`` / ``viewTemplate`` / ``interactionTemplate`` / ``screenFlow`` / ``styleSet``
- Runtime 全局资源：``theme`` / ``fontSet`` / ``imageSet``

主线模型
--------------------

- ``root.json`` 组织 document 入口、公共资源与 variant 命中
- 各 asset 文件提供常量与 view 定义；字体、图片、主题通常由 Runtime 注册或加载
- ``view`` 通过 ``layout`` 排布子节点，通过 ``placement`` 决定自身摆放
- Runtime 将 JSON 资源解析为 ``Document``，并以 ``document_id + absolute_path`` 索引节点

全局约束
--------------------

资源引用
^^^^^^^^^^^^^^^^^^^^

``${...}`` 引用必须显式带 namespace：

.. code-block:: json

   "${constant.path.to.value}"
   "${font.title}"
   "${image.logo}"
   "${color.brand.primary}"

规则：

- ``${constant.<path>}`` 从命中的 ``constant`` asset 合并后的常量树解析
- ``${env.<field>}`` 从当前解析环境读取，如 ``${env.widthDp}`` / ``${env.heightDp}`` / ``${env.theme}``
- ``${font.<id>}`` 展开为字体资源 id，而非字体文件路径
- ``${image.<id>}`` 展开为图片资源 id，而非图片文件路径
- ``${color.<path>}`` 展开为颜色值，仅可用于 ``style`` 的颜色字段
- ``${view.<path>}`` 仅可用于 ``placement.relativeTo``
- 支持的 namespace：``constant`` / ``env`` / ``font`` / ``image`` / ``color`` / ``view``，以及表达式包裹 ``${expr(...)}``

不支持 ``${metrics.titleFont}``、复数形式 ``${colors.*}``、``font: "title"``、``src: "logo"`` 以及冒号写法 ``${font:title}`` / ``${image:logo}``。

单位
^^^^^^^^^^^^^^^^^^^^

JSON UI 中的尺寸在解析阶段统一换算为后端使用的像素值：

- ``px``：用 JSON number 表示，如 ``24``
- ``dp``：用于布局尺寸、间距、圆角、边框等非文字尺寸，``px = round(dp * Environment.density)``
- ``sp``：用于字号，``px = round(sp * Environment.density * Environment.font_scale)``

字段取值规则：``layout.gap`` 用 ``dp`` 字符串或裸数字 ``px``；``placement.x`` / ``placement.y`` 支持 ``dp`` 字符串、百分比或裸数字 ``px``；``placement.width`` / ``placement.height`` 还支持 ``match`` / ``wrap``；``style.fontSize`` 用 ``sp`` 字符串或裸数字 ``px``。

更完整的单位与换算示例见 :doc:`document/root` 与 :doc:`styling/placement`。

JSON UI 规范
--------------------

.. toctree::
   :maxdepth: 1
   :titlesonly:

   document/index
   assets/index
   view/index
   styling/index
   interaction/index
   runtime
   协议维护说明 <contributing>
