# Brookesia GSP GUI 适配器

`AppHost` 在借用的显示设备上启动编译后的 GSP 场景，并在 GSP 会话成功停止前保留
bundle 和事件回调上下文。

可选的 `CONFIG_BROOKESIA_GSP_COMPRESS_COMPILED_BUNDLES` 默认为 `n`。启用后，
Brookesia CMake 辅助函数仍保留 GSPC 生成的原始 `.gspb` 与 API 头文件，但将所选
bundle 在固件中的原始字节替换为无损 zlib 封装。生成配置中的 bundle 指针此时指向
压缩封装，必须交给 `AppHost`，不能直接传给 `esp_gsp_create()`。

对普通编译 bundle，`AppHost` 将完整 GSPB 直接解压到一个 64 字节对齐的 PSRAM
分配中，校验压缩流、长度与 CRC，并保留到成功停止。它节省 Flash，但活动会话会占用
完整 bundle 大小的 PSRAM（当前 Settings 约 1.0 MB）。生成的组件目录与独立动态
字体保持不变。未压缩及可部署 bundle 继续走原路径；目前仅普通生成配置使用压缩，
其 `AppHost::bundle()` 和以往一样为空。
