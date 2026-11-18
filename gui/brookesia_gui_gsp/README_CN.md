# Brookesia GSP GUI 适配器

`AppHost` 在显示设备上启动编译后的 GSP 场景。ESP 平台的
`DisplayLease::lifetime` 必须持有已初始化的显示和触摸 HAL 接口及其设备；会话运行
期间不要主动关闭它们。非空回调上下文必须提供共享的 `callback_owner`。使用普通
生成配置时，`bundle_owner` 需要保证 bundle 字节、生成的目录表、字体及配置引用的
其他数据有效；静态数据可以使用空操作删除器的共享所有者。已加载的 deployable
bundle 和解压后的字节由 `AppHost` 与会话持有。回调上下文宜与会话分开持有；如果
上下文也拥有会话，需要显式停止会话以解除循环引用。

在应用任务中调用 `stop()`。停止失败时，会话保留这些所有者，调用方可以重试；若
会话先被销毁，ESP 清理任务会继续持有资源并重试，同一显示面板在清理成功前不能启动
新会话。ESP 会话在启动 GSP 前会创建一个内部栈为 8 KiB 的清理任务；任务创建失败时
会话启动失败。

Linux 下启用 `BROOKESIA_BUILD_GUI_GSP`，并用 `ESP_GSP_ROOT` 指定官方 ESP-GSP 1.4
模拟器桥接组件。`DisplayLease::desktop` 持有共享的 `DesktopDisplay`，不需要 ESP
显示或触摸句柄。启动会话前，通过 `register_bundle()` 注册原始编译包及对应的
GSPC schema-2 API sidecar；大小、CRC 和元数据校验会拒绝不匹配的产物，包本身仍由
官方渲染器校验。

桥接轮询失败后，`running()` 会在桥接和渲染进程关闭后返回 false。后续启动、停止
或析构会等待已退出的桥接线程结束并回收它。

Session 调用和事件轮询在同一桥接线程执行。启动会话会创建渲染进程；停止时先完成
已接受的调用，再释放桥接和子进程。`browser_url()` 标识当前预览文档，包括会话
fragment 在内的 URL 发生变化后，使用方应重新创建预览文档。系统启动方式及支持
范围见 Toolkit 的 `simulator/docs/build-native-linux.md`。

可选的 `CONFIG_BROOKESIA_GSP_COMPRESS_COMPILED_BUNDLES` 默认为 `n`。启用后，
Brookesia CMake 辅助函数仍保留 GSPC 生成的原始 `.gspb` 与 API 头文件，但将所选
bundle 在固件中的原始字节替换为无损 zlib 封装。生成配置中的 bundle 指针此时指向
压缩封装，必须交给 `AppHost`，不能直接传给 `esp_gsp_create()`。

对普通编译 bundle，`AppHost` 将完整 GSPB 直接解压到一个 64 字节对齐的 PSRAM
分配中，校验压缩流、长度与 CRC，并保留到成功停止。压缩可以节省 Flash，但会话期间
需要 PSRAM 存放解压后的 bundle。生成的组件目录与动态字体仍独立保存。压缩适用于普通
生成配置；这类配置的 `AppHost::bundle()` 返回空指针。

`brookesia_gsp_prepare_theme` 在 bundle 编译前生成明暗主题场景，将
`brookesia_gui_gsp/DejaVuSans.ttf` 解析为本组件的字体资源，使用方无需引用示例
源码路径。字体许可证随字体保存在 `gsp/fonts/`。
