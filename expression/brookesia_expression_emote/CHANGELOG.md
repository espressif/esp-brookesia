# ChangeLog

## v0.7.3 - 2026-03-04

### Enhancements:

* feat(emote): add `RefreshAll` function call for expression service

## v0.7.2 - 2026-02-25

### Breaking Changes:

- break(repo): update `esp_emote_expression` dependency from `0.1.*` to `1.0.*`

### Enhancements:

- feat(emote): improve task affinity validation - set to `-1` (no affinity) if affinity exceeds available CPU cores
- feat(emote): update `function_hide_event_message()` to use `emote_set_event_msg()` API directly
- feat(emote): update native handle access from `gfx_emote_handle` to `gfx_handle` to match new API

## v0.7.1 - 2026-02-02

### Enhancements:

- feat(Kconfig): add Kconfig file with auto-register and debug log options
- feat(repo): add `GFX_ObjectType` enum for UI object type management
- feat(repo): add `HideEmoji` function for hiding emoji display
- feat(repo): add `HideEventMessage` function for hiding event message display
- feat(repo): add `SetQrcode` and `HideQrcode` functions for QR code display support
- feat(repo): change `esp_emote_expression` dependency to private
- feat(repo): update `brookesia_service_helper` dependency to `0.7.*`

## v0.7.0 - 2026-01-14

### Initial Release

- feat(repo): add expression emote management component for ESP-Brookesia ecosystem
