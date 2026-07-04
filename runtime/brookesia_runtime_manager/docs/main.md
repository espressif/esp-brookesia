# Runtime Manager

`brookesia_runtime_manager` 只负责 runtime backend 的 app 生命周期和 native host bridge：

- `Runtime::load_app(const AppConfig &config)`
- `Runtime::start_app(AppId)`
- `Runtime::stop_app(AppId)`
- `Runtime::unload_app(AppId)`
- runtime native module / app context bridge

它不再作为 `RuntimeManager` service 注册，也不负责 `brookesia.start_service()` / `brookesia.call_function()` 这类系统 service bridge。托管在 `system_core` 中的 runtime app 会由 system core 私有的 host bridge 获得这些系统 service 能力；standalone runtime 默认只提供 `brookesia.print()`、`current_app_context()`、`finish_app()`、`attach_app_context()` 和 `detach_app_context()` 等基础函数。

`.bpk` 解包、发布验签与 app package manifest 解析均在 `system/brookesia_system_core`。需要加载 `.bpk` 时，请先使用：

```cpp
#include "brookesia/system_core/package.hpp"

// 可选：verify_app_package_release(package_path, { .public_key_pem_path = "..." });
auto manifest = system::core::unpack_app_package_to(package_path, install_dir, system_type);
auto config = system::core::make_runtime_app_config(manifest.value());
auto app_id = runtime.load_app(config.value());
```

包格式与验签 Kconfig（`BROOKESIA_SYSTEM_CORE_ENABLE_PACKAGE_RELEASE_VERIFY`）见 [system_core app package 规范](../../../system/brookesia_system_core/docs/app_package_spec.md)。
