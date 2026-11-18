# Brookesia GSP GUI adapter

`AppHost` starts a compiled GSP scene bundle on display devices. On ESP,
`DisplayLease::lifetime` must own both initialized HAL interfaces and their
devices; do not explicitly close them while the session is active. A non-null
callback context requires a shared `callback_owner`. For an ordinary generated
config, pass a `bundle_owner` that keeps its bundle bytes, generated tables,
fonts, and any other referenced data valid. Static data can use a shared owner
with a no-op deleter. Loaded deployable bundles and decompressed bytes are owned
by `AppHost` and the session. Prefer a callback context that does not own the
session; otherwise stop the session explicitly to break the ownership cycle.

Call `stop()` from an application task. A failed stop retains these owners and
can be retried. If the session is destroyed first, an ESP cleanup task keeps the
resources and retries the stop; the panel cannot start another session until
cleanup succeeds. An ESP session reserves an 8 KiB internal-stack cleanup task
before starting GSP, and startup fails if that task cannot be created.

On Linux, enable `BROOKESIA_BUILD_GUI_GSP` and provide `ESP_GSP_ROOT` for the
official ESP-GSP 1.4 simulator bridge. `DisplayLease::desktop` holds a shared
`DesktopDisplay`; no ESP panel or touch handles are required. Register the exact
compiled bundle and its GSPC schema-2 API sidecar with `register_bundle()` before
opening a session. Size/CRC and metadata validation reject mismatched artifacts.
The official renderer still validates the bundle itself.

After bridge polling fails, `running()` returns false once the bridge and renderer
have closed. A subsequent start, stop, or destruction joins the exited bridge thread.

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
but requires PSRAM for the expanded bundle during the session. Generated
component directories and dynamic fonts remain separate. Compression applies to
ordinary generated configs; `AppHost::bundle()` returns null for these configs.

`brookesia_gsp_prepare_theme` prepares light/dark scenes before bundle compilation.
The helper resolves `brookesia_gui_gsp/DejaVuSans.ttf` against this component's
font resources, so consumer scenes do not depend on example source paths. The
font's license is included beside the font in `gsp/fonts/`.
