# HAL 主机回归测试

本目录为 HAL 生命周期、板级清理和依赖补丁回归提供统一入口。在 Linux 上编译实际实现并替换硬件接口，不执行实机烧录。

## 环境要求

- Python 3.9+、C 编译器，以及支持 C++23 的编译器（GCC 13 或更新版本）。
- 核心测试需要 Boost 头文件、Boost.Thread 和 Boost.System 开发库。
- 依赖测试需要 Git、CMake，以及已有配置工程的 `managed_components` 目录。

运行器优先使用 `CXX`，否则选择 `g++-13` 或 `c++`。以下命令均在仓库根目录执行。

## 核心回归

```bash
python3 hal/brookesia_hal_adaptor/test_apps/host/run.py --suite core
```

这是供开发者本地手动运行的默认测试组，未接入 CI。覆盖扩展事件顺序、回调移除、摄像头与编码器并发、清理失败后的资源所有权，以及 NAND/BQ27220/USB 初始化回滚。

此外还使用任意设备名称验证真实 Board Manager 调用入口，覆盖无内存分配的错误与重试分发，以及从静态库保留 Mosaico 启动注册。注册失败测试覆盖全部三种错误处理策略，以及启用和禁用断言的配置。

## 依赖回归

先按正常流程配置固件工程，准备依赖及必要补丁，再执行：

```bash
python3 hal/brookesia_hal_adaptor/test_apps/host/run.py --suite all \
    --managed-components examples/system/super/managed_components
```

使用 `--suite dependencies` 可仅运行补丁测试，覆盖补丁应用、组件别名、重复配置、释放错误、帧模式停止和 UYVY/YUYV 协商。依赖由参数显式传入；运行器不会下载依赖或修改工程中的组件副本。

这些用例已在 Board Manager 0.5.15、esp_video 2.4.1、esp_capture 0.8.4 和 av_processor 0.6.6 上验证。升级 Board Manager 时，应使用候选工程解析出的依赖运行测试，检查补丁或 API 差异，再执行核心回归和实机测试。编译失败应作为兼容性问题处理，不应跳过对应测试。

## 验证范围与清理

测试在临时目录中构建，结束时删除生成的源码和可执行文件。编译或用例失败会返回非零退出码。核心测试无需 ESP-IDF 即可运行；依赖测试需显式提供组件源码。

测试模拟硬件故障并控制线程执行顺序，不验证传感器寄存器、显示 DMA 时序、USB 枚举或物理热插拔行为。实机验证使用已有 HAL 测试工程和 System Super；Video Service 测试工程保留预览格式回归。
