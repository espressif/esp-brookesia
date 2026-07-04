.. _system-super-troubleshooting-sec-00:

Troubleshooting
====================

:link_to_translation:`zh_CN:[中文]`

This page lists common problems while running Super.

.. _system-super-troubleshooting-sec-01:

Shell Root Missing
--------------------

The symptom is that ``start()`` fails and the log shows the shell ``root.json`` failing to load. Check that ``BROOKESIA_SYSTEM_SUPER_SHELL_RESOURCE_DIR`` is correct, that ``/littlefs/system/super/shell/root.json`` exists in ESP LittleFS, and that ``brookesia/system/super/shell/root.json`` exists in the PC build directory.

.. _system-super-troubleshooting-sec-02:

Overlay Screen Missing
----------------------

The symptom is that Super fails to start, or the top status bar or bottom gesture indicator does not show. Check that ``root.json`` contains ``screens/overlay.json`` and the file exists, that the Shell manifest contains ``overlay -> AppTop + Stack + z_order 101``, and that ``flows/overlay.json`` is loaded by the root.

.. _system-super-troubleshooting-sec-03:

Background Screen Missing
-------------------------

If the bottom of the Shell page does not show the desktop background or the app solid background, check that ``root.json`` contains ``screens/background.json`` and ``screens/app_background.json``, that the staged ``shell/images/index.json`` and the matching ``.bin`` exist, that the Shell manifest contains ``background -> SystemBottom`` and ``flows/background.json`` is loaded, and that content pages use a transparent background.

.. _system-super-troubleshooting-sec-04:

Shell Images Missing
--------------------

The symptom is that navigation icons do not show, or the shell root fails to load with a missing image asset descriptor. Check that ``shell/images/index.json`` and ``*.bin`` exist in the PC build directory and ESP LittleFS, that ``root.json`` contains the generated ``imageSet`` descriptors such as ``images/index.json``, and that ``brookesia_gui_lvgl_pack_images()`` processed the images under ``assets/shell/images`` successfully.

.. _system-super-troubleshooting-sec-05:

Launcher Has No Apps
--------------------

Check that the app is installed in the core, that the app manifest ``visible`` is ``true``, that the runtime app package is in the ``apps/<manifest-id>`` directory of the storage layout and was scanned, and that the shell has been restarted to refresh the launcher. The Shell summary shows the current launchable app count.

.. _system-super-troubleshooting-sec-06:

Launcher Tap No Response
------------------------

Check that the button template root in ``root.json`` still uses the action ``super.launch.app``, that icon/label child nodes keep ``clickable=false``, that the ``clicked`` effect still dispatches ``super.launch.app`` with ``requireValidPress=true``, that the template id is still ``launcher_app_button``, that the launcher grid path is still ``/launcher/content/grid``, and that the dynamic instance path matches ``/launcher/content/grid/app_<app_id>``.

.. _system-super-troubleshooting-sec-07:

Shell Not Restored After Close
------------------------------

Check that the active app is indeed not the shell, that the Display service has touch gesture enabled, that a ``TouchGesture`` starting from BottomEdge with the Up direction was received, that ``stop_app()`` did not fail, that ``on_app_stopped()`` was not skipped because ``stopping_ = true``, and that the shell app id is valid.

.. _system-super-troubleshooting-sec-08:

Shell Page Not Switching
------------------------

Check that the Shell root contains the three content screens ``/home``, ``/launcher``, and ``/notifications``, and that ``ShellApp::show_page()`` does not return an error log.

.. _system-super-troubleshooting-sec-09:

Gesture Indicator Covered
-------------------------

In the current design the overlay mounts to ``GuiLayer::Top`` as ``AppTop + Stack + z_order 101``, and a regular app screen uses replace mount. If it is covered, check that the overlay mounted successfully, that the app did not bypass the app-owned GUI API to use a system-only mount target or a higher ``z_order`` directly, and that the JSON UI screen is not visually obscured by an opaque full-screen element while the layer order is still correct.

.. _system-super-troubleshooting-sec-10:

Cannot Exit by Swipe Up
-----------------------

The current design exits a regular app through the bottom swipe-up gesture while it is in front. Check that the Display service is started and bound to the current output's touch, that ``/overlay/gesture_indicator`` exists and can be shown through binding, and that the gesture starts from the bottom screen edge and holds to the exit threshold.
