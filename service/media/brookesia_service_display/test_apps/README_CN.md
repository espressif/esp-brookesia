# Display Service 测试应用

该测试应用通过 mock HAL display device 验证 `brookesia_service_display`。

mock device 发布两个 240x240 RGB565 panel 接口，因此测试可以覆盖：

- 多物理输出
- direct source 注册与 active-source 仲裁
- 非 active source 的 dummy/drop 行为
- 在 `240x240` output 上提交 `240x120` 这类 partial update
- 无效 frame 矩形和 buffer size
- 基于 output/source name 的轻量控制 RPC
- `GetOutputs` / `GetSources` 发现类 RPC
- direct async present 的完成回调、latest-wins pending 替换和 active-source drop 行为

## PC

```bash
cmake -S service/media/brookesia_service_display/test_apps -B /tmp/service_display_pc \
  -DBROOKESIA_SERVICE_DISPLAY_TEST_APPS_FORCE_PC=ON
cmake --build /tmp/service_display_pc -j4
ctest --test-dir /tmp/service_display_pc --output-on-failure
ctest --test-dir /tmp/service_display_pc -V
```

## ESP

将该目录按 ESP-IDF Unity test app 构建，并通过 Unity 菜单运行测试用例。
