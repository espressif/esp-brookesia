.. _system-super-app_flow-sec-00:

App Flow
====================

:link_to_translation:`zh_CN:[中文]`

This page describes the Super flow from starting the Shell to opening and closing a regular app.

Starting Super
--------------------

``super::System::start()`` enters the core ``System::start()`` and then calls Super's ``on_start()``:

1. ``stopping_ = false``.
2. ``restore_shell()`` starts the Shell.
3. The core auto-starts three flows per the Shell manifest: ``background -> SystemBottom``, ``shell_pages -> AppDefault``, ``overlay -> AppTop + Stack + z_order 101``. The Shell ``on_start()`` then loads the theme, subscribes to overlay actions, and generates launcher data.

At this point the active app is the Shell, the overlay shows only the top status bar, and the App Launcher shows the app grid and count.

Opening a Regular App
---------------------

After the user taps a launcher button:

1. JSON UI triggers ``super.launch.app``.
2. The Shell finds the target ``AppId`` by event path.
3. The Shell plays the app launch modal and the system UI collapse animation.
4. After the animation completes, the Shell calls ``owner_.start_app(app_id)``.
5. The core starts the target app and sets it as the active app.
6. ``on_app_started()`` notifies the Shell of the current foreground app and switches the app background and system status bar through the Shell-managed overlay.

The regular app's main screen mounts to ``AppDefault`` by default and naturally replaces the Shell page. The Shell background flow stays on ``SystemBottom`` but switches from the Shell image background to the solid app background; the overlay stays on top of that layer as an ``AppTop`` stack screen.

Closing the Active App
----------------------

After the user drags up from the bottom screen edge, triggers the overlay gesture indicator, and holds to the threshold:

1. The Shell tracks the bottom swipe-up distance through the Display service ``TouchGesture`` event.
2. The indicator bar shortens with the drag, and closing triggers after reaching the threshold and holding briefly.
3. The Shell queries ``get_active_app()``.
4. If the active app is not the shell, set the pending Shell page to App Launcher, then call ``stop_app(active.app_id)``.
5. The core stops the app and cleans up GUI/timer/runtime/resources.
6. ``on_app_stopped()`` calls ``restore_shell()`` and shows the App Launcher.

If the user releases partway without meeting the exit condition, the indicator bar rebounds and hides, and the active app keeps running.

Shell Page Switching
--------------------

Shell page switching is triggered by the Shell's own logic. If the Shell is the active app, the system calls ``ShellApp::show_page()`` directly to switch screens within the same app document. If a regular app is the active app, the system first records the target page and stops the regular app; ``on_app_stopped()`` then restores the Shell and shows the target page.

Restoring the Shell
--------------------

The current ``restore_shell()`` logic:

1. If the shell id is invalid, no-op.
2. If the Shell is already ``Running`` or ``Paused``, switch directly to the pending page and expand the overlay.
3. If the Shell is not running yet, start the Shell and then trigger the pending page.

The Shell runs resident, so it does not need to restart after a regular app stops; the App Launcher data is generated when the Shell starts.

System Stop
--------------------

``on_stop()`` sets ``stopping_ = true``. When the Shell stops, the core stops the screen flows in the manifest and unloads the Shell GUI document. ``stopping_`` avoids an app stop triggering a shell restore during system stop.
