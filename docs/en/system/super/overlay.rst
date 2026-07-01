.. _system-super-overlay-sec-00:

Overlay
====================

:link_to_translation:`zh_CN:[中文]`

The overlay is the system overlay layer managed by ShellApp, hosting the status bar, the bottom swipe-up gesture indicator to exit an app, and system floating layers such as the message dialog, keyboard, and app modal.

.. _system-super-overlay-sec-01:

Overview
--------------------

The overlay is a Shell system UI capability, with the resource file ``resource/shell/screens/overlay.json``, physically located in the ``/overlay`` screen of the Shell main document. It is mounted by the ``overlay`` flow in the Shell manifest to ``AppTop`` with the ``Stack`` mount mode and ``z_order = 101``, mapped to the GUI backend ``GuiLayer::Top`` and kept on top. When a regular app opens, it can replace the replace screen of the app layer but does not cover this stack overlay.

The Shell desktop background is not part of the overlay. The background screen ``/background`` is mounted by the ``background`` flow to ``SystemBottom``, and the overlay only handles status, the gesture indicator, and the app modal.

.. _system-super-overlay-sec-02:

Loading and Mounting
--------------------

``ShellApp::mount_overlay()`` performs:

1. When the Shell app starts, the core has already loaded ``resource/shell/root.json``.
2. The core has already auto-started the ``overlay`` flow per the Shell manifest and mounted ``/overlay``.
3. ShellApp initializes status, Wi-Fi state, the clock timer, and the Display touch gesture subscription.

Therefore the overlay is not covered by a regular app's ``GuiLayer::Default`` or regular ``GuiLayer::Top`` replace screen.

.. _system-super-overlay-sec-03:

Status and Gesture Indicator
----------------------------

The current overlay has only one screen:

.. list-table::
   :header-rows: 1

   * - Node
     - Behavior
   * - ``/overlay/status``
     - The top status bar
   * - ``/overlay/status/status_right/wifi_pill``
     - Shown when Wi-Fi is connected
   * - ``/overlay/status/status_right/clock``
     - Local time
   * - ``/overlay/gesture_indicator``
     - The progress indicator for the bottom swipe-up app exit
   * - ``/overlay/gesture_indicator/bar``
     - The indicator bar that shortens with the swipe progress

The status keeps only the Wi-Fi flag and time. The Wi-Fi flag is hidden when the Wi-Fi service is unavailable or not connected. The time refreshes once when the Shell starts and refreshes periodically through a Shell timer.

The gesture indicator is hidden by default. When a regular app is in front, if the Display service reports an Up gesture starting from BottomEdge, the Shell temporarily switches the active display source to GUI, shows the indicator bar, and shortens the bar width by the drag distance. After reaching the exit threshold and holding for ``gestureExitHoldMs``, the Shell closes the current foreground app. Releasing partway plays a rebound animation and hides the indicator.

.. _system-super-overlay-sec-04:

Actions
--------------------

The current overlay no longer provides nav/close/hide/handle actions. Shell page switching is triggered by the Shell's own logic, and regular app exit is driven by the Display service ``TouchGesture``. JSON UI requires a unique action owner within a document; the overlay keeps the actions of subtrees such as the keyboard, message dialog, and app modal.

.. _system-super-overlay-sec-05:

Close
--------------------

The current behavior:

- No active app: no-op.
- The active app is the shell: no-op.
- The active app is a regular app: after the bottom swipe-up reaches the threshold, set the pending Shell page to App Launcher, then call ``stop_app(active.app_id)``.

After the regular app stops, ``on_app_stopped()`` triggers the Shell restore flow and shows the App Launcher.

.. _system-super-overlay-sec-06:

Unmounting
--------------------

``ShellApp::unmount_overlay()`` disconnects the overlay action connections, disconnects the Display/Wi-Fi event connections, and stops the system bar/gesture indicator/clock timer. After the Shell app stops, the core stops the ``overlay`` flow in the manifest and unloads ``/overlay``, and the Shell main document is unloaded by the core when the Shell app stops.
