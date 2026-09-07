.. _system-usb-cli-sec-00:

Serial Command-Line Tool
-------------------------

:link_to_translation:`zh_CN:[中文]`

``brookesia-usb`` controls the ESP-Brookesia USB service through the single CDC-ACM channel provided by USB Serial/JTAG. The protocol version is ``1``.

.. _system-usb-cli-sec-01:

Install
~~~~~~~~~~~~~~~~~~~~~~~~~~

The USB CLI requires Python 3.9 or newer and can be installed online from `PyPI <https://pypi.org/project/brookesia-usb-cli/>`__:

.. code-block:: bash

   python -m pip install brookesia-usb-cli

The install command also installs the ``pyserial`` dependency. After installation, view the command help:

.. code-block:: bash

   brookesia-usb --help

``brookesia deploy`` in :ref:`system-toolkit-sec-00` wraps this CLI to install a built ``.bpk``.

.. _system-usb-cli-sec-02:

Devices and Ports
~~~~~~~~~~~~~~~~~~~~~~~~~~

The CLI selects a port automatically by the Espressif USB Serial/JTAG device identity and verifies it with the ``hello`` command:

.. code-block:: bash

   brookesia-usb devices
   brookesia-usb status
   brookesia-usb --port /dev/ttyACM0 status

The common connection options can also be specified explicitly:

.. code-block:: bash

   brookesia-usb --port /dev/ttyACM0 --baudrate 115200 --timeout 10 status

- ``--port``: USB Serial/JTAG device path; if omitted, discover a matching ACM port.
- ``--baudrate``: retained for pyserial compatibility; USB Serial/JTAG has no physical baud rate and defaults to ``115200``.
- ``--timeout``: per-read and write timeout in seconds, defaulting to ``10``.

When multiple matching devices are present, pass ``--port`` explicitly using a path reported by ``devices``. A single Serial/JTAG CDC configuration normally exposes only ``/dev/ttyACM0``; the absence of ``/dev/ttyACM1`` is expected.

.. _system-usb-cli-sec-03:

Control Sessions and Security
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Commands that require a control session send ``hello`` and validate protocol version ``1``, the ``serial_jtag`` transport, and the ``exclusive`` session state before sending ``goodbye`` on exit.

Device logs are suppressed during the control session so they cannot corrupt JSON responses or file frames. USB Serial/JTAG shares its CDC channel with flashing, JTAG debugging, and console logs; close ``idf.py monitor``, minicom, or any other program reading the same serial device before running a command.

The USB connection is treated as a trusted physical control boundary, while protocol, path, size, checksum, and service-argument validation still apply.

.. _system-usb-cli-sec-04:

Get Status
~~~~~~~~~~~~~~~~~~~~~~~~~~

Query the USB service, transfer, and control-session status:

.. code-block:: bash

   brookesia-usb status
   brookesia-usb --port /dev/ttyACM0 status

The command prints an error and returns a non-zero status when the device is unavailable or the USB service is not running.

.. _system-usb-cli-sec-05:

Call a Service Function
~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``call`` command invokes a registered Brookesia service function. Arguments must be a JSON object and must match the service schema:

.. code-block:: bash

   brookesia-usb call Manager GetServiceNames '{}'
   brookesia-usb call Manager GetServiceSchema '{"Name":"Storage"}'
   brookesia-usb call SystemCore GetSystemInfo '{}'
   brookesia-usb call Storage FSStat '{"Path":"/littlefs"}'
   brookesia-usb call Storage FSList '{"Path":"/littlefs"}'

Use ``Manager.GetServiceNames`` and ``Manager.GetServiceSchema`` to discover available services and functions. ServiceManager validates required parameters, default values, unknown parameters, and parameter types on the device.

Functions that require a ``RawBuffer`` argument cannot be called through the JSON interface because a host pointer is not valid in device memory. Use ``put`` and ``install`` for file and package data. Calls to the ``Usb`` service itself are rejected to prevent recursion into the active USB session.

.. _system-usb-cli-sec-06:

Upload a File
~~~~~~~~~~~~~~~~~~~~~~~~~~

Upload a local file under the configured device upload root. The default upload root is ``/littlefs/usb`` and the remote path must be relative:

.. code-block:: bash

   brookesia-usb put ./logs/session.bin logs/session.bin
   brookesia-usb put ./config/device.json config/device.json --overwrite

Existing files are not overwritten by default; explicitly pass ``--overwrite`` to replace one. Absolute paths, ``..`` path components, symbolic-link escapes, and paths outside the upload root are rejected.

The CLI calculates the file size and SHA-256 digest, sends CRC-protected binary frames, and waits for an acknowledgement after every data frame. The default frame payload is no larger than ``16 KiB`` and the default transfer limit is ``8 MiB``; the device Kconfig controls the actual limits.

The device writes data to a temporary file first and commits it only after size and SHA-256 verification succeed. The temporary file is cleaned up when a transfer is interrupted.

.. _system-usb-cli-sec-07:

Install an Application Package
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Send a complete BPK runtime app package to the device for validation and installation:

.. code-block:: bash

   brookesia-usb install ./build/my_app.bpk

The package is first written to the USB temporary directory. System Core then performs manifest validation, ZIP path-safety checks, staging, replacement, and rollback. The host cannot select the device-side app installation directory directly.

The CLI does not retry an installation after a disconnect or an ambiguous result. Run ``brookesia-usb status`` before retrying the command.

.. _system-usb-cli-sec-08:

Abort a Transfer
~~~~~~~~~~~~~~~~~~~~~~~~~~

Abort an active transfer by request ID:

.. code-block:: bash

   brookesia-usb abort 42

``abort`` is an emergency command and does not start a new control session. When request ``42`` is active, the device removes the temporary file and returns ``aborted``; an unknown request ID returns a device error and a non-zero status.

.. _system-usb-cli-sec-09:

Errors and Troubleshooting
~~~~~~~~~~~~~~~~~~~~~~~~~~

The CLI returns ``0`` after a successful operation and ``1`` for transport, protocol, validation, or device errors. Common error codes include:

- ``invalid_command``: malformed JSON or unsupported operation.
- ``busy``: another control session or transfer is active.
- ``bad_frame``: invalid frame CRC, type, or sequence.
- ``size_mismatch`` / ``hash_mismatch``: declared metadata does not match the data.
- ``path_denied``: unsafe path or overwrite was not explicitly enabled.
- ``storage_full``: temporary storage cannot be created or written.
- ``install_failed``: System Core rejected or failed to install the package.
- ``timeout`` / ``aborted``: the operation timed out or was cancelled.

If the device is not found, check the USB Serial/JTAG Type-C data cable and list system devices:

.. code-block:: bash

   ls /dev/ttyACM*
   brookesia-usb devices

After confirming that no other program owns the CDC channel, pass the device path explicitly with ``--port``.
