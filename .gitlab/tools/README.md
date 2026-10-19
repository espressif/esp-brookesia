# GitLab Tools

## `build_apps.py`

CI builds of `examples/system/super` and `examples/system/gsp_app` reject an existing source-directory `sdkconfig` before cleanup; use `idf.py -B <build-dir>` to keep an existing configuration.

These jobs run the build-local dependency-state regression before building and audit the selected backend's configuration, component paths, generated resources, firmware, and LittleFS image afterward. The audit assumes the standard build-local layout; it does not certify caller-specified external configuration or resource paths.

Run the state regression from the repository root:

```bash
python3 -B -m unittest discover -s examples/system/common -p test_component_manager.py -v
```

To audit an ordinary completed build, pass its output directory:

```bash
python3 examples/system/common/check_build_isolation.py examples/system/super/build
```

The audit checks `<build-dir>/dependencies/local_manifests.sha256` against the local override manifests and final Kconfig configuration. If it reports a missing or stale stamp, rerun `idf.py -B <build-dir> reconfigure`, resolve any configuration failure, and rebuild before auditing again. The regression tests cover when a successful stamp is written; implementation details are in [the shared CMake setup](../../examples/system/common.cmake) and [dependency-state helper](../../examples/system/common/component_manager.py).

## `check_public_header_hygiene.py`

`check_public_header_hygiene.py` checks Brookesia's public include paths for heavyweight or overly broad includes. Its purpose is to prevent accidental transitive dependencies from increasing C/C++ compile time and peak memory usage.

### What it checks

The script performs two textual checks:

1. It checks a small set of high-frequency public headers against fixed forbidden tokens. For example:

   - `task_scheduler.hpp` must not include `boost/asio`, `boost/thread`, or `describe_helpers.hpp`.
   - `log.hpp` must not include `boost/format`, `boost/json`, or `describe_helpers.hpp`.
   - `service/base.hpp` must not include `boost/thread`, `task_scheduler.hpp`, or `describe_helpers.hpp`.

2. It scans production source and header files under `app`, `gui`, `hal`, `runtime`, `service`, `system`, and `utils` and rejects direct use of these umbrella headers:

   ```cpp
   #include "brookesia/lib_utils.hpp"
   #include "brookesia/service_manager.hpp"
   #include "brookesia/service_helper.hpp"
   ```

The umbrella headers themselves are allowed to include those files. `examples`, `host`, `host_test`, `test_apps`, and `tests` are excluded because they are not production include paths.

### Usage

Run it from the `components/esp-brookesia` repository root:

```bash
python3 .gitlab/tools/check_public_header_hygiene.py
```

The script prints `Public header hygiene check passed` and returns `0` when no violation is found. On failure it prints each offending path and token under `Heavy public include violations:` and returns `1`.

### Scope and limitations

The check is intentionally small and deterministic. It uses literal text matching rather than a C/C++ preprocessor or include-graph parser, so it does not validate include completeness, ordering, cycles, API/ABI compatibility, or general IWYU compliance. Review any reported match in context.

The script derives the repository root from its own location and currently has no command-line options. It can be invoked locally or added as a CI/pre-commit check when public-header hygiene should be enforced automatically.
