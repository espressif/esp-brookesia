# Brookesia GSP GUI adapter

`AppHost` starts a compiled GSP scene bundle on borrowed display devices. It
keeps the selected bundle and callback context alive until the GSP session
stops successfully.

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
