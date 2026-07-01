.. _gui-interface-json-ui-assets-image-sec-00:

imageSet
====================

:link_to_translation:`en:[English]`

概览
--------------------

``imageSet`` descriptor 描述一组图片资源。Document 可在 ``root.json.assets`` 中引用 ``imageSet``，Runtime 也可通过
``register_image_file/json(...)`` 全局注册其中的所有图片，使 ``${image.<id>}`` 可以在 ``imageProps.src`` 中使用。

图片资源统一组织为 ``imageSet.images[]``，即使只有一张图片。

相关文档
--------------------

- :doc:`index`
- :doc:`../index`
- :doc:`../runtime`

字段表
--------------------

.. list-table::
   :header-rows: 1

   * - Key
     - 类型
     - 默认值
     - 是否必填
     - 说明
   * - ``type``
     - string
     - 无
     - 是
     - 固定为 ``"imageSet"``
   * - ``images``
     - object[]
     - 无
     - 是
     - 图片 descriptor 列表，不能为空
   * - ``images[].id``
     - string
     - 无
     - 是
     - 图片资源 id
   * - ``images[].src``
     - string
     - ``""``
     - 否
     - 图片文件路径；相对 ``imageSet`` 文件所在目录解析
   * - ``images[].width``
     - integer px
     - ``0``
     - 否
     - 源图宽度；用于 image sizing
   * - ``images[].height``
     - integer px
     - ``0``
     - 否
     - 源图高度；用于 image sizing

``images[]`` entry 不包含 ``type``。同一个 ``imageSet`` 内不能出现重复 id。

Runtime API
----------------------

- ``register_image(...)``
- ``unregister_image(...)``
- ``register_image_json(...)``
- ``register_image_file(...)``
- ``list_image_resources(document_id)``

``register_image_json/file(...)`` 会注册 ``imageSet`` 中的所有图片；若其中任意一项注册失败，本次已注册的图片会回滚。

Document image 资源优先级高于 Runtime 全局 image；Runtime 全局 image 用于补充 document 未声明的图片 id。

源文件格式（backend 相关）
----------------------------------

``src`` / ``RuntimeImageResource.primary_src`` 的具体文件格式由当前 backend 决定。``gui_interface`` 只保存图片 id、路径和尺寸，并在 document load 阶段调用 backend 的资源补全与预加载 hook。

若某个 backend 支持从专有图片格式中读取尺寸，``register_image(...)`` 可以在 ``width`` / ``height`` 缺失时通过 backend hook 补全。若无法补全，``width`` / ``height`` 必须显式为正数。

示例
--------------------

.. code-block:: json

   {
       "type": "imageSet",
       "images": [
           {
               "id": "logo",
               "src": "../../../../../docs/_static/brookesia_logo.png",
               "width": 3409,
               "height": 834
           }
       ]
   }
