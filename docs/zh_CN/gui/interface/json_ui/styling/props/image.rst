.. _gui-interface-json-ui-styling-props-image-sec-00:

imageProps
====================

:link_to_translation:`en:[English]`

概览
--------------------

``imageProps`` 适用于 ``image``。

相关文档
--------------------

- :doc:`index`
- :doc:`../../view/image`
- :doc:`../../assets/image`

字段表
--------------------

.. list-table::
   :header-rows: 1

   * - Key
     - 类型
     - 默认值
     - Binding
     - UI 影响 / 限制
   * - ``src``
     - string, 必须为 ``"${image.<id>}"``
     - ``""``
     - ``imageProps.src``
     - parser 解析为图片资源 id；binding 写入时 store 中应直接存图片资源 id
   * - ``innerAlign``
     - string enum，见 ``innerAlign``
     - ``"default"``
     - 否
     - 图片内部对齐/铺排方式；``"contain"`` 可让图片在固定外框内等比完整显示，``"tile"`` 可配合 ``offsetX/offsetY`` 做平铺滚动
   * - ``recolor``
     - string，``#RRGGBB``
     - ``""``
     - ``imageProps.recolor``
     - 图片重着色；空字符串关闭重着色
   * - ``recolorOpacity``
     - integer，``0..255``
     - ``255``
     - ``imageProps.recolorOpacity``
     - 重着色混合强度；``0`` 关闭混合，``255`` 完全使用目标颜色
   * - ``angle``
     - integer 度
     - ``0``
     - ``imageProps.angle``
     - 图片本地旋转角度，单位为度；作用于所有节点的旋转优先使用 ``commonProps.angle``
   * - ``offsetX``
     - integer px
     - ``0``
     - ``imageProps.offsetX``
     - 图片内容横向偏移；常用于平铺背景滚动
   * - ``offsetY``
     - integer px
     - ``0``
     - ``imageProps.offsetY``
     - 图片内容纵向偏移
   * - ``zoom``
     - integer
     - ``256``
     - ``imageProps.zoom``
     - 图片本地缩放值；``256`` 表示 1x；作用于所有节点的缩放优先使用 ``commonProps.zoom``
   * - ``pivotX``
     - integer px 或 percent string
     - ``0``
     - ``imageProps.pivotX``
     - 图片本地旋转/缩放 pivot 的 X 坐标；作用于所有节点的 pivot 优先使用 ``commonProps.pivotX``
   * - ``pivotY``
     - integer px 或 percent string
     - ``0``
     - ``imageProps.pivotY``
     - 图片本地旋转/缩放 pivot 的 Y 坐标；作用于所有节点的 pivot 优先使用 ``commonProps.pivotY``

示例
--------------------

.. code-block:: json

   "imageProps": {
       "src": "${image.logo}",
       "innerAlign": "tile",
       "recolor": "#2563eb",
       "recolorOpacity": 160,
       "angle": 15,
       "offsetX": -12,
       "zoom": 320,
       "pivotX": "50%",
       "pivotY": "50%"
   }

``recolor`` 与 ``style.imageOpacity`` / ``style.imageRecolor`` 相互独立：``imageProps.recolor``
作为节点局部覆盖，``style.imageRecolor`` 可由 theme/styleRefs 统一管理图标颜色。
如果只设置 ``recolor``，``recolorOpacity`` 默认使用 ``255``。写入空字符串可恢复原图，同时保留
``recolorOpacity`` 当前值，便于后续动态恢复；若该节点还有 theme/style 层重着色，空字符串会清除局部覆盖并回退到 style 层效果。

``imageProps.angle`` / ``zoom`` / ``pivotX`` / ``pivotY`` 只作用于当前 image 节点。对于需要作用在所有节点上的 transform，使用 ``commonProps.angle`` / ``zoom`` / ``pivotX`` / ``pivotY``。

innerAlign
--------------------

公开取值如下：

.. list-table::
   :header-rows: 1

   * - 值
     - 行为
   * - ``"default"``
     - 使用 backend 默认图片对齐方式
   * - ``"center"``
     - 图片内容在 image view 内居中
   * - ``"contain"``
     - 图片等比缩放到完整显示在 image view 内，适合 launcher icon、导航 icon 等固定小外框
   * - ``"cover"``
     - 图片等比缩放并覆盖 image view，可能裁剪超出部分
   * - ``"stretch"``
     - 图片拉伸填满 image view，不保持原始比例
   * - ``"tile"``
     - 图片平铺显示，可配合 ``offsetX`` / ``offsetY`` 做滚动背景

如果 image view 同时固定了 ``placement.width`` 和 ``placement.height``，但没有设置 ``innerAlign``，
后端只会设置外框尺寸，不会自动执行 contain 缩放。需要完整显示小图标时应显式写：

.. code-block:: json

   "imageProps": {
       "src": "${image.icon}",
       "innerAlign": "contain"
   }

图片尺寸行为见 :doc:`../placement` 的「Image 尺寸」。
如果 image view 使用 ``placement.aspectRatio``，该字段只约束 image view 外框；图片内容在外框内如何缩放、
裁剪或拉伸仍由 ``innerAlign`` 决定。
