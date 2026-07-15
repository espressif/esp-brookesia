# ESP-Brookesia Matter Controller

[中文版本](./README_CN.md)

This app embeds a **client-only Matter Controller on the ESP RainMaker Matter Fabric** into the ESP-Brookesia System Super framework. The on-screen UI covers the device list, lighting control, room grouping, commissioning QR code, and Thread topology.

Running this project also requires an **ESP RainMaker** patch. Use the official branches below for ESP-IDF, ESP-Matter, and ESP-Brookesia.

## Table of Contents

- [ESP-Brookesia Matter Controller](#esp-brookesia-matter-controller)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [SDK versions](#sdk-versions)
  - [Repository that needs a patch: ESP RainMaker](#repository-that-needs-a-patch-esp-rainmaker)
  - [Environment setup](#environment-setup)
    - [1. ESP-IDF 6.2 (ESP32-S31)](#1-esp-idf-62-esp32-s31)
    - [2. ESP-Matter `release/v1.6`](#2-esp-matter-releasev16)
    - [3. Point the example at your local clones](#3-point-the-example-at-your-local-clones)
    - [4. Hardware and certificates](#4-hardware-and-certificates)
  - [Build, flash, and run](#build-flash-and-run)
  - [How to use](#how-to-use)
  - [Troubleshooting](#troubleshooting)
  - [References](#references)

## Overview

The Matter Controller runs inside the [examples/system/super](../../examples/system/super) example and launches as a built-in app next to Settings.

What it does:

- **RainMaker commissioning**: shows a QR code / manual pairing code so the ESP RainMaker app can join the controller to the user's Matter Fabric
- **Device list and control**: fetches devices on the same fabric and supports on/off, brightness, color temperature, hue, and saturation
- **Rooms**: assign devices to rooms in the local UI
- **Live state**: subscribes to Matter attribute reports for reachability and lighting parameters
- **Thread page**: shows Thread-related node information when the fabric includes Thread devices

The firmware disables the Matter Server (`CONFIG_ESP_MATTER_ENABLE_MATTER_SERVER=n`) and follows the RainMaker [client-only controller](https://docs.espressif.com/projects/esp-matter/en/latest/esp32/controller.html) path: Wi-Fi comes from RainMaker / `network_provisioning`; the NOC and device list come from the RainMaker cloud.

## SDK versions


| Repository | Commit / version | Patch needed |
| --- | --- | --- |
| [ESP-IDF](https://github.com/espressif/esp-idf) | **6.2** (commit id: e37a7ae) | No |
| [ESP-Matter](https://github.com/espressif/esp-matter) | `release/v1.6` (commit id: 31b76ad) | No |
| [ESP RainMaker](https://github.com/espressif/esp-rainmaker) | `master`, baseline commit `c91229d4` | **Yes**, see below |
| ESP-Brookesia | this repo, branch `feat/add_matter_controller_app` (v0.8 line) | No (app is already included) |


Version basis:

- ESP-Brookesia `master` (v0.8) requires ESP-IDF **6.2** for ESP32-S31. See [ESP-Brookesia versioning](https://github.com/espressif/esp-brookesia/blob/master/README.md)
- ESP32-S31 is currently supported on ESP-IDF `master`. See [Which version should I start with?](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/versions.html#which-version-should-i-start-with)
- `examples/system/super/main/idf_component.yml` pins `espressif/esp_matter` to `~1.6.0`

> [!NOTE]
> The official RainMaker `examples/matter/matter_controller` README recommends ESP-IDF v5.5.3. That pin does **not** apply to this Brookesia / ESP32-S31 project. Use IDF 6.2 from the table above.

## Repository that needs a patch: ESP RainMaker

Patch file:

```
[patches/0001-feat-compatible-with-brookesia-matter-controller.patch](./patches/0001-feat-compatible-with-brookesia-matter-controller.patch)
```

Baseline: ESP RainMaker `master` commit `c91229d4` (`v1.15.0-31`, after the `matter-examples-revamp` merge).

What the patch changes:

1. Bumps the `esp_matter` component dependency from `~1.5.1` to `~1.6.0`
2. Aligns connectedhomeip / ESP-Matter 1.6 APIs (`ScopedMemoryBuffer`, subscribe callback signatures, `controller::get_fabric_index()`)
3. Adds `generate_controller_noc_chain_with_csr()` so the client-only controller can issue a NOC from a RainMaker CSR

```bash
git clone --recursive https://github.com/espressif/esp-rainmaker.git
cd esp-rainmaker
git checkout c91229d4
git submodule update --init --recursive

# Replace PATCH with the absolute path to this file
git apply /path/to/esp-brookesia/app/brookesia_app_matter_controller/patches/0001-feat-compatible-with-brookesia-matter-controller.patch
```

Verify the patch:

```bash
git apply --check /path/to/0001-feat-compatible-with-brookesia-matter-controller.patch
git status
```

If `--check` fails, the RainMaker tree has moved past `c91229d4`. Check out that commit and apply again. Do not force-apply onto an unrelated older tag such as `v1.15.0`.

## Environment setup

### 1. ESP-IDF 6.2 (ESP32-S31)

Follow the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s31/get-started/index.html) and build hello_world once to confirm the toolchain.

```bash
cd /path/to/esp-idf
./install.sh
source ./export.sh
```

### 2. ESP-Matter `release/v1.6`

Follow [ESP-Matter Developing — ESP Matter Setup](https://docs.espressif.com/projects/esp-matter/en/latest/esp32/developing.html#esp-matter-setup). This project needs **1.6**, so do not shallow-clone the default `main` branch:

```bash
# source ESP-IDF export.sh first
git clone -b release/v1.6 --depth 1 https://github.com/espressif/esp-matter.git
cd esp-matter
git submodule update --init --depth 1
cd ./connectedhomeip/connectedhomeip
./scripts/checkout_submodules.py --platform esp32 linux --shallow   # use darwin on macOS
cd ../..
./install.sh
```

Export the environment in every new terminal:

```bash
cd /path/to/esp-idf && source ./export.sh
cd /path/to/esp-matter && source ./export.sh
export IDF_CCACHE_ENABLE=1
```

### 3. Point the example at your local clones

`examples/system/super/CMakeLists.txt` locates RainMaker and ESP-Matter through environment variables:

```bash
export ESP_MATTER_PATH=/path/to/esp-matter
export RMAKER_PATH=/path/to/esp-rainmaker
```

Also replace the **developer-specific relative paths** in `examples/system/super/main/idf_component.yml` with your clones (absolute paths are fine):

```text
espressif/esp_matter:
  version: "~1.6.0"
  override_path: /path/to/esp-matter
  require: public

espressif/esp_rainmaker:
  version: ">=1.0"
  override_path: /path/to/esp-rainmaker/components/esp_rainmaker
```

### 4. Hardware and certificates

- Board: `esp32_s31_korvo1` (primary validation board for this controller).
- Partition table `examples/system/super/partitions_16m.csv` includes `esp_secure_cert` and `fctry`. Defaults:
  - `CONFIG_ESP_RMAKER_NO_CLAIM=y`
  - `CONFIG_ESP_RMAKER_USE_ESP_SECURE_CERT_MGR=y`
  - `CONFIG_ENABLE_ESP32_FACTORY_DATA_PROVIDER=y`

#### Claim device certificates

Self Claiming and Assisted Claiming cannot be used with RainMaker + Matter examples, because the certificate must already be present before Matter commissioning starts.
Use [host driven claiming](https://docs.rainmaker.espressif.com/docs/product_overview/concepts/claiming#host-driven-claiming-publicprivate-rainmaker) through the [RainMaker CLI](https://github.com/espressif/esp-rainmaker-cli/blob/master/docs/README.md#installation).

Make sure the device is connected to the host, log in to the CLI, then run:

```bash
esp-rainmaker-cli claim --matter {port}
```

This fetches the device certificates and flashes them onto the device.

#### Generate the factory NVS binary

The factory NVS (`fctry` partition) must be generated with [esp-matter-mfg-tool](https://github.com/espressif/esp-matter-tools).

The tool is published on PyPI: [esp-matter-mfg-tool](https://pypi.org/project/esp-matter-mfg-tool). Install it with `pip install esp-matter-mfg-tool`.

```bash
esp-matter-mfg-tool --vendor-id 0x131B --product-id 0x2 \
                    --vendor-name "Espressif" --product-name "RainMaker-Matter-Light" \
                    --hw-ver-str "DevKitM1" \
                    -cd $RMAKER_PATH/examples/matter/mfg/cd_131B_0002.der \
                    --csv $RMAKER_PATH/examples/matter/mfg/keys.csv \
                    --mcsv $RMAKER_PATH/examples/matter/mfg/master.csv
```

This generates the factory NVS binary required by Matter and embeds the RainMaker MQTT host URL through `master.csv`. You can instead bake the MQTT host into firmware: `idf.py menuconfig` → **ESP RainMaker Config** → `ESP_RMAKER_READ_MQTT_HOST_FROM_CONFIG`, then omit `--csv` and `--mcsv` when running mfg-tool.

Flash the generated factory binary to the `fctry` partition. In this project that offset is `0x1E000` (see `examples/system/super/partitions_16m.csv`).

```bash
esptool.py write_flash 0x1E000 out/131b_2/{node-id}/{node-id}-partition.bin
```

## Build, flash, and run

Also see:

- [ESP-Brookesia Programming Guide — Versioning](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia Programming Guide — How to Use Example Projects](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html#getting-started-example-projects)

```bash
cd /path/to/esp-brookesia/examples/system/super

idf.py gen-bmgr-config -b esp32_s31_korvo1
idf.py build
idf.py -p <PORT> flash monitor
```

Exit the serial monitor with `Ctrl-]`.

Erase flash first if the partition table changed:

```bash
idf.py -p <PORT> erase-flash flash monitor
```

A successful boot prints:

```text
=== System Example Completed ===
```

The device then enters the System Super desktop. Open the **Matter Controller** icon.

Optional stacks (audio, video, App Store, and others) stay off by default. Enable them under `idf.py menuconfig` → **Super Example Optional Features**, then `idf.py fullclean && idf.py build`.

## How to use

1. Launch Matter Controller after flashing. The **Commissioning** page shows a QR code and manual pairing code until the controller is provisioned.
2. Scan the QR code with the [ESP RainMaker app](https://rainmaker.espressif.com/docs/get-started.html) ([Android](https://play.google.com/store/apps/details?id=com.espressif.rainmaker) / [iOS](https://apps.apple.com/app/esp-rainmaker/id1497491540)). Select the Matter group / fabric for this controller.
3. The controller installs the Administer NOC issued by RainMaker and fetches the device list for that fabric.
4. Use **Devices** for on/off, brightness, CCT, and color. Use **Rooms** to group devices.
5. The device console still accepts commands prefixed with `matter esp controller`. See [Matter Controller](https://docs.espressif.com/projects/esp-matter/en/latest/esp32/controller.html).

Typical bring-up order: commission end devices (lights, and so on) into the same RainMaker user / Matter group first, then commission this controller so device cards can appear.

## Troubleshooting

**`ESP RainMaker repo not found` / `ESP Matter repo not found`**

`RMAKER_PATH` or `ESP_MATTER_PATH` is unset or points at a missing directory. Export both, then `idf.py fullclean && idf.py build`.

**`git apply` rejects the patch**

RainMaker is not at `c91229d4`. Check out that commit and apply again.

**Component Manager still resolves `esp_matter` 1.5.x**

`override_path` in `idf_component.yml` still points at an old tree, or it was not updated to your 1.6 clone.

**Blank screen after boot**

Confirm `littlefs_data.bin` was generated and flashed. System Super stages `system/super`, `system/fonts`, and `apps` resources during the build.

**App launches but the device list is empty**

Confirm RainMaker provisioning finished, the Matter group is correct, and end devices already sit on the same fabric. Check logs for `rainmaker_started`, `network_provisioned`, and the device-list callback.

**Wi-Fi never starts / conflicts with the Brookesia Wi-Fi service**

The S31 defaults turn off `CONFIG_BROOKESIA_SERVICE_WIFI_ENABLE_AUTO_REGISTER` so RainMaker owns the Wi-Fi lifecycle. Do not re-enable that option unless you also replace RainMaker provisioning.

## References

- [ESP-Brookesia Getting Started](https://docs.espressif.com/projects/esp-brookesia/en/latest/getting_started.html)
- [ESP-Matter Controller](https://docs.espressif.com/projects/esp-matter/en/latest/esp32/controller.html)
- [ESP-Matter Developing](https://docs.espressif.com/projects/esp-matter/en/latest/esp32/developing.html)
- [ESP RainMaker Matter Controller example](https://github.com/espressif/esp-rainmaker/tree/master/examples/matter/matter_controller)
- [ESP RainMaker Get Started](https://rainmaker.espressif.com/docs/get-started.html)
- [System Super example](../../examples/system/super/README.md)
