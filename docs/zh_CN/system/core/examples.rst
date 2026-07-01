.. _system-core-examples-sec-00:

示例
====================

:link_to_translation:`en:[English]`

本文提供原生应用和运行时应用的最小接入骨架。

.. _system-core-examples-sec-01:

原生应用最小骨架
--------------------

原生应用继承 ``core::IApp``，在 ``get_manifest()`` 提供元信息，在 ``get_gui_descriptor()`` 提供 GUI root 和启动 flow，并在 ``on_start()`` 初始化业务状态：

.. code-block:: cpp

   class DemoApp final: public core::IApp {
   public:
       core::AppManifest get_manifest() const override
       {
           return {
               .id = "brookesia.app.demo",
               .localized_names = {{"en", "Demo"}, {"zh_CN", "演示"}},
               .kind = core::AppKind::Native,
               .visible = true,
               .resource_dir = "app/demo/assets",
           };
       }

       core::AppGuiDescriptor get_gui_descriptor() const override
       {
           return {
               .root_kind = core::GuiRootKind::File,
               .root = "root.json",
               .screen_flows = {
                   {.screen_flow = "main", .layer = core::GuiAppLayer::AppDefault},
               },
           };
       }

       std::expected<void, std::string> on_start(core::AppContext &context) override
       {
           return context.gui().set_text("/demo/title", "Demo Ready");
       }
   };

``on_stop()`` 通常可以省略：core 会在 app stop 时自动取消 timer、停止 action subscription、停止启动 flow、unload GUI document 并反注册 GUI 资源。

.. _system-core-examples-sec-02:

运行时应用最小包
--------------------

运行时应用部署为 unpacked package，目录结构：

.. code-block:: text

   brookesia.app.demo/
     manifest.json
     app/
       app.lua
     res/
       profile.json
       root.json

``res/profile.json`` 声明启动 flow：

.. code-block:: json

   {
     "root": "root.json",
     "screen_flows": [
       {"screen_flow": "main", "layer": "AppDefault"}
     ]
   }

``res/root.json`` 引用 screen 和 flow asset：

.. code-block:: json

   {
     "version": "0.1.0",
     "assets": ["screens/demo.json", "flows/main.json"]
   }

``res/flows/main.json`` 声明一屏 ``screenFlow``：

.. code-block:: json

   {
     "type": "screenFlow",
     "id": "main",
     "screens": ["demo"],
     "initial": "demo"
   }

core 会在 ``on_start()`` 前自动挂载 ``/demo``，脚本只需要初始化业务状态：

.. code-block:: lua

   brookesia_app = {}

   function brookesia_app.on_start()
       local gui = brookesia.start_service("SystemGui")
       brookesia.call_function(gui, "SetText", '{"Path":"/demo/title","Text":"Demo"}')
       return true
   end

运行时应用包应部署到 storage layout 的 ``apps/<manifest-id>`` 目录；``System::init()`` 会在启动时扫描 internal 和所有 external storage volume 的 ``apps`` 目录并安装。
