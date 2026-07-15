# ChangeLog

## v0.8.1 - 2026-07-13

### Enhancements:

- feat(logging): print the component version during system initialization.
- feat(utils): consume Utils profiler snapshots and events for Shell overlay rendering.

### Bug Fixes:

- fix(build): use `idf_component.yml` as the single component version source.
- fix(debug): remove profiler and debug scheduler lifecycle ownership from ShellApp.
- fix(debug): remove the obsolete GUI debug provider from the Shell lifecycle.

## v0.8.0 - 2026-06-28

### Initial Release:

- Initial release of `brookesia_system_super` with launcher shell, overlay, app navigation, and startup UI resources.
- Provides built-in app and service-backed product shell integration.
