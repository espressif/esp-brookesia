.. _gui-lvgl-image-pack-sec-00:

LVGL Image 打包
==========================

:link_to_translation:`en:[English]`

``brookesia_gui_lvgl`` 提供 CMake helper，将 app 的 PNG/JPG/JPEG 图片转换为 LVGL v9 ``.bin``，并在输出目录生成 ``index.json`` 形式的 ``imageSet`` descriptor。

.. _gui-lvgl-image_pack-sec-01:

CMake 命令
--------------------

.. code-block:: cmake

   brookesia_gui_lvgl_pack_images(
       INPUT_DIR <image_dir>
       OUTPUT_DIR <image_asset_dir>
       [CF <LVGLImage.py --cf value>]
       [COMPRESS <NONE|RLE|LZ4>]
       [ALIGN <n>]
       [BACKGROUND <hex>]
       [PREMULTIPLY]
       [RGB565_DITHER]
   )

行为：

- 递归扫描 ``INPUT_DIR`` 下的 ``.png`` / ``.PNG`` / ``.jpg`` / ``.JPG`` / ``.jpeg`` / ``.JPEG``。
- JPG/JPEG 会先用 Pillow 转成构建目录中的临时 PNG，再交给 LVGLImage.py；输出 image id 仍使用原始文件名 stem。
- 使用文件名 stem 作为 image id。
- 为每张图片生成 ``<id>.bin``。
- 输出目录的 ``index.json`` 使用 ``type: "imageSet"``，其中每个 ``images[]`` 条目的 ``src`` 指向同目录 ``<id>.bin``，并从 ``.bin`` header 写入 ``width`` / ``height``。
- 若多个图片文件名 stem 相同，配置阶段失败。
- ``CF RGB565`` 生成的 ``.bin`` 可被 LVGL backend 直接加载，适合无 alpha 的大背景图以降低镜像体积。
- 需要透明通道的 icon 或 sprite 仍建议使用 ``CF ARGB8888``。

.. _gui-lvgl-image_pack-sec-02:

LVGLImage.py 来源
------------------------------

helper 不维护本地 ``LVGLImage.py`` 副本，而是从当前工程依赖的 LVGL 组件解析：

- ESP-IDF：优先从 build components 中查找 ``lvgl``，再查找 ``lvgl__lvgl``，使用 ``${COMPONENT_DIR}/scripts/LVGLImage.py``。
- PC：从 ``lvgl`` target 的 ``SOURCE_DIR`` 读取 ``scripts/LVGLImage.py``。
- 特殊工程可通过 cache 变量 ``BROOKESIA_GUI_LVGL_IMAGE_TOOL`` 显式覆盖脚本路径。

.. _gui-lvgl-image_pack-sec-03:

Python 环境
--------------------

helper 需要可导入 ``png``、``lz4.block`` 和 ``PIL.Image`` 的 Python：

- 可通过 ``BROOKESIA_GUI_LVGL_IMAGE_TOOL_PYTHON`` 指定 Python。
- 默认会尝试工程 Python、``BROOKESIA_ROOT_DIR/.venv-docs/bin/python``、``BROOKESIA_ROOT_DIR/.venv/bin/python`` 等候选。
- ``BROOKESIA_GUI_LVGL_IMAGE_TOOL_AUTO_INSTALL_PIP_DEPS`` 默认为 ``ON``；若候选 Python 缺少依赖，会在 ``${CMAKE_BINARY_DIR}/brookesia_gui_lvgl_image_tool_venv`` 创建构建本地 venv 并安装 ``pypng``、``lz4``、``Pillow``。

.. _gui-lvgl-image_pack-sec-04:

Runtime 行为
--------------------

生成的 ``index.json`` 可作为 JSON UI ``imageSet`` asset 使用：

.. code-block:: json

   {
       "type": "imageSet",
       "images": [
           {
               "id": "bird",
               "src": "bird.bin",
               "width": 57,
               "height": 41
           }
       ]
   }

LVGL backend 会在 document load 阶段预加载 ``.bin`` 文件，并按路径缓存。后续 ``imageProps.src`` binding、``set_view_src(...)`` 或 props 增量 apply 只复用已预加载 descriptor，不在动态刷新路径读取文件。
backend 会读取 ``.bin`` header 中的 color format、尺寸和 stride；因此 ``ARGB8888``、``RGB565`` 等 LVGLImage.py
支持的 LVGL v9 bin 格式无需额外 runtime 转换。
