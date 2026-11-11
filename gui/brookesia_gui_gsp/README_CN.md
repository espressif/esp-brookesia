# Brookesia GSP GUI 适配器

`AppHost` 在借用的显示设备上启动编译后的 GSP 场景。调用者必须保持回调上下文、
HAL 设备及普通生成配置所引用的借用数据有效，直到 `stop()` 成功；启动或停止失败时
也不能提前释放。`AppHost` 拥有已加载的 deployable bundle 和解压后的字节，普通
未压缩 bundle 字节及生成的目录表仍由调用者保管。

Linux 下启用 `BROOKESIA_BUILD_GUI_GSP`，并用 `ESP_GSP_ROOT` 指定官方 ESP-GSP 1.4
模拟器桥接组件。`DisplayLease::desktop` 持有共享的 `DesktopDisplay`，不需要 ESP
显示或触摸句柄。启动会话前，通过 `register_bundle()` 注册原始编译包及对应的
GSPC schema-2 API sidecar；大小、CRC 和元数据校验会拒绝不匹配的产物，包本身仍由
官方渲染器校验。

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
