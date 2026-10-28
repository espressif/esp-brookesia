# ChangeLog

## v0.9.0 - 2026-10-28

### Breaking Changes:

- break(deps): move to the shared Brookesia 0.9 line and use matching 0.9.x component dependencies.

### Enhancements:

- feat(gui): add 480x480 shell, startup, theme, and font resource variants.
- feat(expansion): notify users when supported expansion modules are attached, removed, or fail to initialize.

### Bug Fixes:

- fix(deps): declare the Service Helper dependency required by expansion notifications.

## v0.8.3 - 2026-08-05

### Bug Fixes:

- fix(keyboard): make common punctuation and symbols available in the special-key layout.

## v0.8.2 - 2026-07-27

### Enhancements:

- feat(build): split shell lifecycle, display, input, resource, and launch modules.
- chore(deps): rely on System Core contracts instead of direct utility linkage.

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
