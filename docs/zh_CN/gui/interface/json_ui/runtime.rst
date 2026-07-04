.. _gui-interface-json-ui-runtime-sec-00:

Runtime
==============================

:link_to_translation:`en:[English]`

.. _gui-interface-json_ui-runtime-sec-01:

概览
--------------------

本文档负责：

- 运行时资源模型
- ``document_id + absolute_path`` API 映射
- 字体/图片资源查询
- 模板实例

本文档不负责 JSON 字段表；字段细节请查看各模块文档。

.. _gui-interface-json_ui-runtime-sec-02:

相关文档
--------------------

- :doc:`index`
- :doc:`assets/index`
- :doc:`view/index`
- :doc:`styling/placement`
- :doc:`interaction/screen_flow`

.. _gui-interface-json_ui-runtime-sec-03:

运行时资源模型
--------------------

parser 最终会把资源解析成运行时 ``Document``：

- 顶层 ``screen`` -> ``Document.screens``
- 顶层非 ``screen`` 的 ``view`` asset -> ``Document.templates``

Runtime 另外还维护三类非 document 全局资源：

- 全局 ``font``
- 全局 ``image``
- 全局 ``theme``

随后 ``Runtime`` 负责：

- document 注册
- eager screen 预创建
- dynamic screen 延迟创建
- 模板实例化
- 路径索引
- 事件与 bindings

当前 bindings 规则：

- ``bindings`` key 使用公开 ``camelCase`` 点路径
- ``bindings`` value 是当前节点路径作用域下的本地 key，不带前缀
- runtime 实际使用 ``document_id + absolute_path + local_key`` 定位 binding
- 建议通过 ``Runtime::set_binding_value(document_id, absolute_path, key, value)``、``Runtime::set_binding_values(document_id, updates)`` 或 ``View::set_binding_value(key, value)`` 读写

.. _gui-interface-json_ui-runtime-sec-04:

当前公共 API 对协议的映射
------------------------------

.. _gui-interface-json_ui-runtime-sec-05:

document 相关
^^^^^^^^^^^^^^^^^^^^^^

- ``load(root_path, document, environment)``：加载已解析好的 ``Document`` 对象
- ``load_json(root_path, json, base_dir, environment)``：从 JSON 字符串加载
- ``load_file(file_path, environment)``
- ``unload(document_id)``
- ``update(document_id, file_path, environment)``
- ``get_constant_value(document_id, path)``：读取该 document 合并后常量树中的值
- ``load_theme(...)``
- ``load_theme_json(...)``
- ``load_theme_file(...)``
- ``list_supported_themes()``
- ``set_theme(theme_id)``
- ``set_theme(theme_id, reapply_loaded_documents)``
- ``get_theme()``
- ``register_font(...)``
- ``register_font_json(...)``
- ``register_font_file(...)``
- ``list_supported_fonts(language = "")``
- ``list_supported_languages()``
- ``set_language(language)``
- ``set_language(language, reapply_loaded_documents)``
- ``get_language()``
- ``set_default_font_for_language(language, font_id)``
- ``get_default_font_for_language(language)``
- ``register_image(...)``
- ``unregister_image(...)``
- ``register_image_json(...)``
- ``register_image_file(...)``
- ``list_font_resources(document_id)``
- ``list_image_resources(document_id)``
- ``set_view_debug_enabled(enabled)``
- ``is_view_debug_enabled()``
- ``reapply_styles(document_id)``
- ``enable_live_preview(document_id, options = {})``
- ``disable_live_preview(document_id)``
- ``poll_live_preview()``

说明：

- view debug 是 Runtime 级调试能力
- 开启后会为当前 runtime 下所有 view 叠加仅用于观察边界和层级的 debug outline
- 切换 view debug 不需要 ``update(...)``
- ``theme`` 是 Runtime 全局资源，通过 ``set_theme(...)`` 切换
- ``set_theme(theme_id)`` 会立即对所有已加载 document 重新应用 style；若 file-backed document 使用
  ``Theme`` variant，则会重新解析该 root
- ``set_theme(theme_id, false)`` 只更新 Runtime 当前 theme，并在 Runtime 内部标记已加载 document 的 style 或
  environment dirty
- Runtime 总会先应用一份内建默认 theme；``list_supported_themes()`` 不包含这份内建资源
- 外部 theme 只负责覆盖内建默认 theme，不替代默认值基线
- ``font`` 是 Runtime 全局资源；``list_supported_fonts(language)`` 可按语言过滤字体 id
- ``language`` 是 Runtime 当前语言；``get_language()`` 返回当前值
- ``set_language(language)`` 会立即对所有已加载 document 重新应用 style；若语言还影响 variants、常量或初始绑定值，应用层还需要对相关 document 调用 ``update(...)``
- ``set_language(language, false)`` 只更新 Runtime 当前语言，并在 Runtime 内部标记已加载 document 的 environment dirty
- 内建默认 theme 不指定 ``font``；若节点和当前 theme 未显式写 ``style.font``，Runtime 会优先使用“当前语言 -> 默认 font id”的映射
- 只有显式 ``style.font: "${font.<id>}"`` 会绕过语言默认字体；这适合固定品牌字或特殊标题，不适合需要跟随语言切换的正文
- live preview 也是 Runtime 级开发辅助能力，但只适用于 ``load_file(...)`` 加载出的 document
- live preview 负责监听 root 文件与解析依赖文件的变化，并自动调用 ``update(...)``

.. _gui-interface-json_ui-runtime-sec-06:

screen 相关
^^^^^^^^^^^^^^^^^^^^

- ``list_displays()``
- ``list_layers()``
- ``mount_screen(document_id, absolute_path, target = MountTarget{})``
- ``unmount_screen(document_id, absolute_path)``
- ``push_transient_screen(document_id, absolute_path, target = MountTarget{})``：临时压入一个 screen，返回 ``TransientMountId``
- ``pop_transient_screen(transient_mount_id)``：按 id 弹出临时 screen
- ``start_screen_flow(document_id, flow_id, target = MountTarget{})``
- ``trigger_screen_flow(document_id, flow_id, action)``
- ``stop_screen_flow(document_id, flow_id)``
- ``has_screen_flow(document_id, flow_id)``
- ``get_screen_flow_state(document_id, flow_id)``

其中 ``MountTarget`` 至少包含：

- ``display_id``
- ``layer``
- ``mount_mode``
- ``z_order``

默认值：

- ``display_id = ""``
  - 表示 backend 默认 display
- ``layer = GuiLayer::Default``
- ``mount_mode = MountStackMode::Replace``
- ``z_order = 0``

内置 layer 为：

- ``Default``
- ``Top``
- ``System``
- ``Bottom``

说明：

- ``Default`` 对应某个 display 的默认页面层
- ``Replace`` 是默认 mount 语义：同一 ``display_id + layer`` 上再次 ``mount_screen(...)`` 只替换已有的 replace screen
- ``Stack`` 允许同一 ``display_id + layer`` 上共存多个 screen，不替换 replace screen
- 同一 layer 内会按 ``z_order`` 置前；数值越大越靠上；同 ``z_order`` 按挂载顺序后挂载者靠上
- 被替换的 screen 若为 ``mountMode = "eager"``，会被 hidden
- 被替换的 screen 若为 ``mountMode = "dynamic"``，会按 dynamic 语义销毁
- 不同 ``display_id`` 或不同 ``layer`` 允许同时存在多个 mounted screen
- ``absolute_path`` 仍只表示 document 内节点路径，不包含 display/layer

``screenFlow`` 是 document-local 的 screen flow asset，定义见 :doc:`interaction/screen_flow`。
``start_screen_flow(document_id, flow_id, ...)`` 会挂载 flow 的 initial screen；若该 flow 已在运行则视为幂等成功，不会重复挂载。
如果配置了 transitions，则后续可以通过
``trigger_screen_flow(...)`` 根据当前 state 和 action 切换 screen；如果 transitions 缺省或为空，则该 flow 只表示
“静态首屏”，Runtime 会记录当前 state，但触发切换会返回错误。``has_screen_flow(...)`` 仅判断 document 内是否**定义**了该 flow，
不代表 flow 正在运行。``stop_screen_flow(...)`` 会卸载当前 screen 并停止
flow。document 可同时运行多个 flow；``unload(...)`` 或 ``update(...)`` 会自动停止该 document 下正在运行的 flow。

.. _gui-interface-json_ui-runtime-sec-07:

任意节点查询
^^^^^^^^^^^^^^^^^^^^

- ``find_view(document_id, absolute_path)``
- ``View::find_view(path)``

说明：

- ``find_view(document_id, absolute_path)`` 当前可以查询任意已实例化节点
- 所有公共路径都使用 Unix 风格绝对路径，例如 ``/about/header/title``

.. _gui-interface-json_ui-runtime-sec-08:

节点状态与运行时操作
^^^^^^^^^^^^^^^^^^^^

- ``get_view_state(view, kind)`` / ``get_view_state(document_id, absolute_path, kind)``：读取节点运行时状态（``ViewStateKind``）
- ``scroll_view_to_visible(document_id, absolute_path, animated = true)`` / ``scroll_view_to_visible(view, animated = true)``：把目标节点滚动到可见区域
- ``set_view_src(document_id, absolute_path, src)``：直接更新 image 节点的 ``src``
- ``process_backend()``：驱动一次 backend 处理（事件、动画、刷新等）；通常在宿主主循环中周期调用

.. _gui-interface-json_ui-runtime-sec-09:

模板实例
^^^^^^^^^^^^^^^^^^^^

- ``create_view(document_id, template_id, parent_absolute_path, instance_id)``
- ``destroy_view(document_id, absolute_path)``

其中：

- ``template_id`` 是顶层模板 id
- ``parent_absolute_path`` 是 parent 的绝对路径
- ``instance_id`` 由调用方显式提供
- ``destroy_view(...)`` 只能删除非 ``screen`` 子树

.. _gui-interface-json_ui-runtime-sec-10:

事件订阅
^^^^^^^^^^^^^^^^^^^^

- ``subscribe_event_action(document_id, action, handler)``
- ``subscribe_event_action_with_id(document_id, action, handler)``
- ``unsubscribe_subscription(subscription_id)``
- ``View::on_event(type, handler)``
- ``Button::on_click(handler)``

说明：

- ``subscribe_event_action(...)`` 使用 ``document_id + action`` 精确匹配
- ``subscribe_event_action_with_id(...)`` 使用相同的 ``document_id + action`` 路由，并返回 ``SubscriptionId``
- 同一个 document 内，静态声明的非空 ``action`` 必须全局唯一
- 模板定义生成的多个实例可以共用同一个 ``action``，可通过 ``event.path`` / ``event.node_id`` 区分来源
- 新接口允许预注册：目标模板实例当前还没 create 也能先注册
- ``subscribe_event_action(...)`` 返回 ``ScopedConnection``；connection 析构时会自动断开
- ``subscribe_event_action_with_id(...)`` 返回 ``SubscriptionId``；适合需要记录订阅身份或后续显式断开的场景
- ``unsubscribe_subscription(subscription_id)`` 可显式断开通过 ``with_id`` 获取到的订阅；对无效或已断开的 id 返回失败
- ``View::on_event(...)`` 依赖一个已经可查询到的具体实例，不做 ``action`` 字符串过滤

.. _gui-interface-json_ui-runtime-sec-11:

运行时动画
^^^^^^^^^^^^^^^^^^^^

- ``start_view_animation(document_id, absolute_path, animation, completed_handler = {})``
- ``start_view_animation_with_id(document_id, absolute_path, animation, completed_handler = {})``
- ``start_view_animation_with_result(document_id, absolute_path, animation, completed_handler = {})``
- ``start_view_animation(view, animation, completed_handler = {})``
- ``start_view_animation_with_id(view, animation, completed_handler = {})``
- ``start_view_animation_with_result(view, animation, completed_handler = {})``

说明：

- 这组 API 会在运行时直接启动一个 view 动画，不依赖节点 JSON 中预先声明 ``animations``。
- 当前推荐用于：
  - ``placement.x``
  - ``placement.y``
  - ``placement.width``
  - ``placement.height``
  - ``imageProps.angle``
  - ``imageProps.offsetX``
  - ``imageProps.offsetY``
- ``animation.property`` 继续复用 ``AnimationProperty``，当前至少支持：
  - ``Opacity``
  - ``X``
  - ``Y``
  - ``Width``
  - ``Height``
  - ``Scale``
  - ``Angle``
  - ``OffsetX``
  - ``OffsetY``
- ``animation.easing`` 继续复用 ``AnimationEasing``，当前 ``linear`` 和 ``easeIn`` 已覆盖常见运行时场景。
- ``start_view_animation(...)`` 返回 ``ScopedConnection``，析构后会取消该动画。
- ``start_view_animation_with_id(...)`` 返回 ``SubscriptionId``，可配合 ``unsubscribe_subscription(subscription_id)`` 主动取消。
- ``start_view_animation_with_result(...)`` 返回 ``RuntimeAnimationStartResult``，同时带回 ``SubscriptionId`` 与启动结果信息，便于在启动失败或被立即完成时做判断。
- 同一 view 上同一动画 property 的新动画会覆盖之前的动画。
- ``completed_handler`` 只会在动画自然完成时触发；若动画被取消，则不会再触发完成回调。

.. _gui-interface-json_ui-runtime-sec-12:

资源查询
^^^^^^^^^^^^^^^^^^^^

- ``list_font_resources(document_id)``
- ``list_image_resources(document_id)``

说明：

- 字体查询返回 Runtime 当前全局可见的字体资源集合
- ``list_supported_fonts(language = "")`` 返回字体 id，不返回完整 descriptor
- 图片查询继续按 ``document_id`` 返回该 document 当前可见的图片资源，包含 document ``imageSet`` 和 Runtime 全局 image
- Runtime 全局 image 可在 ``load_file()`` 前注册，document 中的 ``${image.<id>}`` 会在加载校验阶段解析到该资源
- ``register_image(...)`` 在未显式填写尺寸时，会请求 backend 尝试通过图片 metadata 补全 ``width`` / ``height``
- document 加载阶段会通过 backend preload hook 预加载当前 document ``imageSet`` 和 Runtime 全局 image；缺失或格式无效会让 ``load_file()`` / ``load_json()`` 失败，而不是延迟到首次显示时失败
- ``unregister_image(id)`` 会从 Runtime 全局 image 表移除对应资源，并释放 backend 对该 image 的预加载引用
- 同名 image 资源优先使用 document ``imageSet``，再回退到 Runtime 全局 image

.. _gui-interface-json_ui-runtime-sec-13:

Binding 批量更新与增量 Apply
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- ``Runtime::set_binding_values(document_id, updates)`` 可一次写入多组 ``BindingValueUpdate``。
- Runtime 会按节点合并本批次触发的 props/style/layout/placement apply mask；同一节点在一批更新中通常只重应用一次对应域。
- 初次创建、document reload、resource refresh 仍使用完整 apply；binding 高频更新路径使用精确 mask。
- 动画 API 会直接修改 backend 节点属性，不会回写 binding store。若应用层自己缓存 binding 值，在动画结束或取消后需要主动失效缓存，或用强制写入覆盖动画后的真实状态。

.. _gui-interface-json_ui-runtime-sec-14:

语言和主题的延迟更新策略
^^^^^^^^^^^^^^^^^^^^^^^^

Runtime 默认 API 保持即时更新语义：

- ``set_language(language)`` 校验语言后更新 Runtime 当前语言，并立即对所有已加载 document 重新应用 style
- ``set_theme(theme_id)`` 校验主题后更新 Runtime 当前主题，并立即对所有已加载 document 重新应用 style；使用
  ``Theme`` variant 的 file-backed document 会重新解析 root
- ``update(document_id, file_path, environment)`` 会重新解析单个 document，可用于让新的 ``Environment`` 影响 variants、常量文案和初始绑定值

如果应用同时缓存了多个已加载 document，语言或主题切换可能不希望在 display task 上同步刷新所有已加载页面。调用方可以显式使用 Runtime lazy 更新：

- ``set_language(language, false)`` 会更新 Runtime 当前语言和已加载 tree 的 environment 记录，并在 Runtime 内部标记 ``environment_dirty``
- ``set_theme(theme_id, false)`` 会更新 Runtime 当前 theme；普通 document 标记 ``styles_dirty``，使用 ``Theme``
  variant 的 file-backed document 标记 ``environment_dirty``
- Runtime 在 ``mount_screen(...)``、``find_view(...)``、``create_view(...)``、``subscribe_event_action(...)`` 等访问目标 document 前调用 ``refresh_document_if_dirty(...)`` 兑现 dirty 更新
- ``refresh_document_if_dirty(...)`` **只处理 ``environment_dirty``**：存在 ``environment_dirty`` 时对 ``load_file(...)`` 加载的 document 执行内部 ``update(...)``，成功后清掉 environment dirty
- ``styles_dirty`` 不在统一 dirty 兑现里处理：它在具体访问点单独兑现，例如 ``mount_screen(...)`` 挂载某 screen 时只对被挂载的 subtree 重新应用样式；也可由 ``set_theme(theme_id)`` / ``reapply_styles(document_id)`` 显式兑现
- 非 file-backed document 没有可重新解析的 root 文件，不能完整兑现 lazy environment 更新；这类 document 需要使用默认即时 API 或由调用方显式 ``update(...)``
- 未加载 document 不需要 dirty 标记，首次 ``load_file(...)`` 会直接使用调用方传入的当前 ``Environment`` 和 Runtime theme
