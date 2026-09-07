.. _system-usb-cli-sec-00:

Serial/JTAG USB CLI
-------------------

:link_to_translation:`en:[English]`

``brookesia-usb`` 通过 USB Serial/JTAG 提供的单个 CDC-ACM 通道控制 ESP-Brookesia USB service，协议版本为 ``1``。

.. _system-usb-cli-sec-01:

安装
~~~~~~~~~~~~~~~~~~~~~~~~~~

USB CLI 需要 Python 3.9 或更高版本，可从 `PyPI <https://pypi.org/project/brookesia-usb-cli/>`__ 在线安装：

.. code-block:: bash

   python -m pip install brookesia-usb-cli

安装命令会自动安装 ``pyserial`` 依赖。安装完成后可以查看命令帮助：

.. code-block:: bash

   brookesia-usb --help

:ref:`system-toolkit-sec-00` 中的 ``brookesia deploy`` 会调用本 CLI 安装已构建的 ``.bpk``。

.. _system-usb-cli-sec-02:

设备和端口
~~~~~~~~~~~~~~~~~~~~~~~~~~

CLI 会根据 Espressif USB Serial/JTAG 设备标识自动选择端口，并通过 ``hello`` 命令验证设备：

.. code-block:: bash

   brookesia-usb devices
   brookesia-usb status
   brookesia-usb --port /dev/ttyACM0 status

也可以显式指定常用连接参数：

.. code-block:: bash

   brookesia-usb --port /dev/ttyACM0 --baudrate 115200 --timeout 10 status

- ``--port``：USB Serial/JTAG 设备路径；省略时自动发现匹配的 ACM 端口。
- ``--baudrate``：为兼容 pyserial 保留，USB Serial/JTAG 不使用物理波特率，默认值为 ``115200``。
- ``--timeout``：每次读写的超时时间，单位为秒，默认值为 ``10``。

如果存在多个匹配设备，请使用 ``devices`` 输出的路径显式指定 ``--port``。单个 Serial/JTAG CDC 配置通常只提供 ``/dev/ttyACM0``，不存在 ``/dev/ttyACM1`` 并不表示设备异常。

.. _system-usb-cli-sec-03:

控制会话和安全
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

需要控制会话的命令会发送 ``hello``，检查协议版本 ``1``、``serial_jtag`` 传输类型和 ``exclusive`` 会话状态，并在退出时发送 ``goodbye``。

设备日志会在控制会话期间被抑制，避免干扰 JSON 响应和文件帧。USB Serial/JTAG 的 CDC 通道同时用于刷写、JTAG 调试和控制台日志，请在执行命令前关闭 ``idf.py monitor``、minicom 或其他读取同一串口的程序。

USB 连接属于受信任的物理控制边界，但仍会执行协议、路径、大小、校验和服务参数验证。

.. _system-usb-cli-sec-04:

查询状态
~~~~~~~~~~~~~~~~~~~~~~~~~~

查询 USB service、传输状态和控制会话状态：

.. code-block:: bash

   brookesia-usb status
   brookesia-usb --port /dev/ttyACM0 status

设备未连接或 USB service 未启动时，命令会输出错误并返回非零状态。

.. _system-usb-cli-sec-05:

调用服务函数
~~~~~~~~~~~~~~~~~~~~~~~~~~

``call`` 命令调用设备上已注册的 Brookesia service 函数。参数必须是 JSON object，参数名称和类型必须符合服务 schema：

.. code-block:: bash

   brookesia-usb call Manager GetServiceNames '{}'
   brookesia-usb call Manager GetServiceSchema '{"Name":"Storage"}'
   brookesia-usb call SystemCore GetSystemInfo '{}'
   brookesia-usb call Storage FSStat '{"Path":"/littlefs"}'
   brookesia-usb call Storage FSList '{"Path":"/littlefs"}'

可以先通过 ``Manager.GetServiceNames`` 和 ``Manager.GetServiceSchema`` 查询可调用的 service 与函数。ServiceManager 会在设备侧校验必选参数、默认值、未知参数和参数类型。

需要 ``RawBuffer`` 参数的函数不能通过 JSON 接口调用，因为主机指针无法直接用于设备内存。文件和软件包数据应分别使用 ``put`` 和 ``install`` 命令。为避免递归调用，不能通过该接口调用 ``Usb`` service 自身。

.. _system-usb-cli-sec-06:

上传文件
~~~~~~~~~~~~~~~~~~~~~~~~~~

将本地文件上传到设备配置的上传根目录下。默认上传根目录为 ``/littlefs/usb``，远程路径必须是相对路径：

.. code-block:: bash

   brookesia-usb put ./logs/session.bin logs/session.bin
   brookesia-usb put ./config/device.json config/device.json --overwrite

默认不会覆盖已有文件，只有显式指定 ``--overwrite`` 才会替换目标文件。绝对路径、包含 ``..`` 的路径、符号链接逃逸和上传根目录之外的路径都会被拒绝。

CLI 会计算文件大小和 SHA-256 摘要，通过 CRC 保护的二进制帧传输文件，并在每个数据帧后等待设备确认。默认单帧 payload 不超过 ``16 KiB``，默认单次传输最大为 ``8 MiB``；具体限制由设备 Kconfig 配置决定。

设备会先将数据写入临时文件，只有在大小和 SHA-256 校验成功后，才会提交到目标路径。传输过程中断时，临时文件会被清理。

.. _system-usb-cli-sec-07:

安装 BPK 应用
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

将完整的 BPK 运行时应用包发送到设备并安装：

.. code-block:: bash

   brookesia-usb install ./build/my_app.bpk

软件包会先写入 USB 临时目录，再由 System Core 执行 manifest 校验、ZIP 路径安全检查、暂存、替换和回滚流程。主机不能直接选择设备上的应用安装目录。

如果连接中断或返回结果不明确，CLI 不会自动重试安装。重新执行前请先使用 ``brookesia-usb status`` 检查设备状态。

.. _system-usb-cli-sec-08:

终止传输
~~~~~~~~~~~~~~~~~~~~~~~~~~

根据 request ID 终止活动传输：

.. code-block:: bash

   brookesia-usb abort 42

``abort`` 是紧急命令，不会建立新的控制会话。request ``42`` 正在活动时，设备会删除临时文件并返回 ``aborted``；未知 request ID 会返回设备错误并以非零状态退出。

.. _system-usb-cli-sec-09:

错误和故障排查
~~~~~~~~~~~~~~~~~~~~~~~~~~

命令成功时返回 ``0``，传输、协议、参数校验或设备错误时返回 ``1``。常见错误码包括：

- ``invalid_command``：JSON 格式错误或操作不支持。
- ``busy``：已有控制会话或传输正在进行。
- ``bad_frame``：帧 CRC、类型或序列错误。
- ``size_mismatch`` / ``hash_mismatch``：声明的文件元数据与实际数据不一致。
- ``path_denied``：路径不安全或未显式允许覆盖。
- ``storage_full``：无法创建或写入临时存储。
- ``install_failed``：System Core 拒绝或未能安装软件包。
- ``timeout`` / ``aborted``：超时或传输被取消。

如果找不到设备，请先检查 USB Serial/JTAG Type-C 数据线和系统设备列表：

.. code-block:: bash

   ls /dev/ttyACM*
   brookesia-usb devices

确认没有其他程序占用同一 CDC 通道后，再使用 ``--port`` 显式指定设备路径。
