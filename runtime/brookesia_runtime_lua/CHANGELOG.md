# ChangeLog

## v0.8.3 - 2026-10-27

### Breaking Changes:

- break(deps): replace public `georgik/lua` with private `espressif/lua` at `^5.5.1`.

## v0.8.2 - 2026-07-27

### Enhancements:

- chore(build): apply shared compile tuning to the Lua runtime test project.

## v0.8.1 - 2026-07-13

### Enhancements:

- feat(logging): print the component version during backend initialization.

### Bug Fixes:

- fix(build): use `idf_component.yml` as the single component version source.

## v0.8.0 - 2026-06-28

### Initial Release:

- Initial release of `brookesia_runtime_lua` with Lua backend integration for managed app packages.
- Provides lifecycle and host-bridge support through Runtime Manager.
