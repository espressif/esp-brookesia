# Brookesia GSP GUI adapter

`AppHost` starts a compiled GSP scene bundle on borrowed display devices. It
keeps the selected bundle and callback context alive until the GSP session
stops successfully.

On Linux, enable `BROOKESIA_BUILD_GUI_GSP` and provide `ESP_GSP_ROOT` for the
official ESP-GSP 1.4 simulator bridge. `DisplayLease::desktop` holds a shared
`DesktopDisplay`; no ESP panel or touch handles are required. Register the exact
compiled bundle and its GSPC schema-2 API sidecar with `register_bundle()` before
opening a session. Size/CRC and metadata validation reject mismatched artifacts.
The official renderer still validates the bundle itself.

Session calls and event polling execute on one bridge thread. A session starts a
renderer process, and stopping it drains accepted calls before releasing the
bridge and child process. `browser_url()` identifies the current preview document;
replace that document when the URL changes, including its session fragment.
The Linux system entrypoint and support limits are documented in the Toolkit's
`simulator/docs/build-native-linux.md`.

The optional `CONFIG_BROOKESIA_GSP_COMPRESS_COMPILED_BUNDLES` build setting
defaults to `n`. When enabled by a consuming example, the Brookesia CMake
helper keeps GSPC's original `.gspb` and generated API headers, but embeds a
lossless zlib envelope in place of each selected bundle's raw Flash bytes.
Pass the generated config through `AppHost`; its bundle pointer then refers to
the envelope and must not be passed directly to `esp_gsp_create()`.

For a compressed ordinary compiler bundle, `AppHost` expands the exact GSPB
into one 64-byte-aligned PSRAM allocation, checks the complete zlib stream,
length, and CRC, and retains those bytes until stop succeeds. This saves Flash
at the cost of the active expanded bundle size in PSRAM (about 1.0 MB for the
current Settings bundle). The generated component directories and separate
dynamic font remain unchanged. Uncompressed and deployable bundles retain
their existing paths; compression is currently used only for ordinary generated
configs, whose `AppHost::bundle()` remains null as before.

`brookesia_gsp_prepare_theme` prepares light/dark scenes before bundle compilation.
The helper resolves `brookesia_gui_gsp/DejaVuSans.ttf` against this component's
font resources, so consumer scenes do not depend on example source paths. The
font's license is included beside the font in `gsp/fonts/`.
