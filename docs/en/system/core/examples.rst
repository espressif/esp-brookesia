.. _system-core-examples-sec-00:

Examples
====================

:link_to_translation:`zh_CN:[中文]`

This page provides minimal integration skeletons for a native app and a runtime app.

Minimal Native App
--------------------

A native app subclasses ``core::IApp``, provides metadata in ``get_manifest()``, provides the GUI root and startup flow in ``get_gui_descriptor()``, and initializes business state in ``on_start()``:

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

``on_stop()`` can usually be omitted: the core auto-cancels timers, stops action subscriptions, stops startup flows, unloads the GUI document, and unregisters GUI resources when the app stops.

Minimal Runtime App
--------------------

A runtime app is deployed as an unpacked package, with this structure:

.. code-block:: text

   brookesia.app.demo/
     manifest.json
     app/
       app.lua
     res/
       profile.json
       root.json

``res/profile.json`` declares the startup flow:

.. code-block:: json

   {
     "root": "root.json",
     "screen_flows": [
       {"screen_flow": "main", "layer": "AppDefault"}
     ]
   }

``res/root.json`` references the screen and flow assets:

.. code-block:: json

   {
     "version": "0.1.0",
     "assets": ["screens/demo.json", "flows/main.json"]
   }

``res/flows/main.json`` declares a single-screen ``screenFlow``:

.. code-block:: json

   {
     "type": "screenFlow",
     "id": "main",
     "screens": ["demo"],
     "initial": "demo"
   }

The core auto-mounts ``/demo`` before ``on_start()``, so the script only initializes business state:

.. code-block:: lua

   brookesia_app = {}

   function brookesia_app.on_start()
       local gui = brookesia.start_service("SystemGui")
       brookesia.call_function(gui, "SetText", '{"Path":"/demo/title","Text":"Demo"}')
       return true
   end

The runtime app package should be deployed to the ``apps/<manifest-id>`` directory of the storage layout; ``System::init()`` scans the ``apps`` directories of internal and all external storage volumes at startup and installs them.
