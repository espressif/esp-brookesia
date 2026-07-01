.. _hal-pc-simulation-sec-00:

Host Platform Simulation Support
================================

:link_to_translation:`zh_CN:[中文]`

ESP-Brookesia provides HAL backends for non-ESP targets so that upper-layer code can reuse the abstract device model of :ref:`HAL Interface <hal-interface-index-sec-00>` on a PC, enabling development, debugging, automated testing, and web demos without changing business logic.

Two platform backends are currently available:

- ``brookesia_hal_linux``: the Linux host backend, exported as the CMake alias ``brookesia::hal_linux``.
- ``brookesia_hal_wasm``: the WebAssembly backend, exported as the CMake alias ``brookesia::hal_wasm``.

.. _hal-pc-simulation-sec-01:

Overview
--------

The host simulation backends sit at the same layer as :ref:`ESP Device Board Adaptation <hal-adaptor-sec-00>`: both implement the device/interface model defined by ``brookesia_hal_interface`` and register **system**, **network**, **audio**, **display**, **storage**, **power**, **video**, and **Wi-Fi** capabilities into the global HAL table for upper layers to discover by name. The difference is that the host backends run on a Linux or WebAssembly runtime instead of a real board with ``esp_board_manager``.

.. _hal-pc-simulation-sec-02:

Linux Platform
--------------

``brookesia_hal_linux`` is the Linux host backend of ``brookesia_hal_interface``. It builds and runs the framework and upper-layer apps on desktop Linux, which is convenient for development and integration without real hardware.

.. _hal-pc_simulation-sec-03:

Dependencies and Build
^^^^^^^^^^^^^^^^^^^^^^^

- The dependency installer ``scripts/install_linux_deps.sh`` in the component directory supports ``apt`` / ``dnf`` / ``pacman``; ``--minimal`` installs only the build, Boost, and OpenSSL dependencies, ``--full`` also installs the ``proot`` helper used for transparent absolute paths, and ``--media`` / ``--video`` / ``--display`` / ``--network`` install the optional groups for FFmpeg/PortAudio/V4L2, SDL2, and NetworkManager/UPower.
- The component ships a host test project that builds via ``cmake -S host_test -B host_test/build`` and runs with ``ctest``; it forces stub backends by default to keep CI deterministic.

.. _hal-pc_simulation-sec-04:

Backend Strategy
^^^^^^^^^^^^^^^^^^

Each capability can choose between stub and real backends, controlled by CMake variables or Kconfig:

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Capability
     - Backend options
   * - Media (audio)
     - ``stub`` or ``ffmpeg_portaudio`` (FFmpeg + PortAudio playback, capture, encode/decode)
   * - Video
     - ``stub`` or ``ffmpeg_v4l2`` (FFmpeg ``libavdevice`` / V4L2 capture and decode)
   * - Display
     - ``stub`` or ``sdl2`` (SDL2 window, bitmap refresh, mouse/touch, backlight simulation)
   * - Power
     - ``stub`` or ``upower`` (reads real battery state from ``/sys/class/power_supply``)
   * - Wi-Fi
     - ``stub`` or ``networkmanager`` (scan, connect, disconnect, and status via ``nmcli``)

``auto`` prefers the real backend when its dependencies are available; at runtime it prints a warning and falls back to the deterministic stub path if there is no display session, camera, battery, or sufficient permission.

.. _hal-pc_simulation-sec-05:

Runtime Data
^^^^^^^^^^^^^^

- KV data is persisted by priority ``BROOKESIA_HAL_LINUX_KV_DIR`` -> executable directory ``.brookesia/kv`` -> ``$XDG_STATE_HOME`` -> ``$HOME`` -> system temporary directory.
- The file system maps ``/spiffs`` / ``/littlefs`` / ``/fatfs`` / ``/sdcard`` to host subdirectories through ``BROOKESIA_HAL_LINUX_FS_ROOT``, defaulting to ``.brookesia/fs`` when unset.
- HTTP uses Boost.Beast and OpenSSL and can throttle the response read rate through Kconfig to simulate ESP-class download throughput; SNTP and Storage use the host network stack and local file system.

.. _hal-pc-simulation-sec-03:

WASM Platform
-------------

``brookesia_hal_wasm`` is the WebAssembly backend that compiles the framework into a browser or WebAssembly runtime for web-side simulation and demos.

- Like the Linux backend, it implements and registers the system, network, audio, video, display, power, storage, and Wi-Fi device-family interfaces into the global HAL table.
- The display backend is based on SDL2 with a default ``800x480`` window; ``DisplayWasmDevice::Config`` configures the resolution and window title, and it polls quit and touch events.
- The network backend provides connectivity and HTTP client interfaces, enabling browser capabilities through link options when built as an Emscripten target.
- Under ``EMSCRIPTEN`` the component appends the link options ``-sUSE_SDL=2`` / ``-sFETCH=1`` / ``-sASYNCIFY=1`` / ``-sALLOW_MEMORY_GROWTH=1`` for SDL2 rendering, HTTP fetch, async blocking, and memory growth respectively.

.. _hal-pc-simulation-sec-04:

Choosing a Backend
------------------

- Real ESP devices: use :ref:`ESP Device Board Adaptation <hal-adaptor-sec-00>` and :ref:`ESP Device Board Configuration <hal-boards-sec-00>` to initialize real peripherals through ``esp_board_manager``.
- Local PC development and CI: use ``brookesia_hal_linux``, with stub backends for deterministic behavior when no real hardware is present.
- Web demos and cross-platform showcases: use ``brookesia_hal_wasm`` compiled to WebAssembly.

.. _hal-pc-simulation-sec-05:

Notes
-----

- The host simulation backends keep HAL interface behavior compatible so upper-layer logic can be reused on desktop or browser environments, but they do not reproduce the real timing, power management, or peripheral limits of the target ESP hardware.
- Stub backends are used for deterministic tests, while real backends rely on host environment capabilities and fall back to the stub path automatically when a capability is missing.
- Platform-specific capability differences (such as charger control, SoftAP, and camera) follow each component's source code and its Kconfig/CMake options.
