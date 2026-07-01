# Display Service Test App

This test app verifies `brookesia_service_display` with a mock HAL display device.

The mock device publishes two 240x240 RGB565 panel interfaces, so the tests can cover:

- multiple physical outputs
- direct source registration and active-source arbitration
- dropped dummy frames from inactive sources
- partial updates such as `240x120` into a `240x240` output
- invalid frame rectangles and buffer sizes
- lightweight control RPC by output/source name
- `GetOutputs` / `GetSources` discovery RPC
- direct async present completion, latest-wins pending replacement, and active-source drop handling

## PC

```bash
cmake -S service/media/brookesia_service_display/test_apps -B /tmp/service_display_pc \
  -DBROOKESIA_SERVICE_DISPLAY_TEST_APPS_FORCE_PC=ON
cmake --build /tmp/service_display_pc -j4
ctest --test-dir /tmp/service_display_pc --output-on-failure
ctest --test-dir /tmp/service_display_pc -V
```

## ESP

Build this directory as an ESP-IDF Unity test app and run cases from the Unity menu.
