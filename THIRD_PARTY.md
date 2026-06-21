# Third-party components

This project vendors the following third-party code. Each retains its own
license (all MIT-compatible); the original license headers are preserved in the
source files.

| Component | Where | License | Source |
|-----------|-------|---------|--------|
| QR Code generator library (C) | `examples/companion_radio/ui-new/qrcodegen.{h,cpp}` | MIT | Project Nayuki — https://www.nayuki.io/page/qr-code-generator-library |
| LVGL (graphics library) | fetched to `sim-lvgl/vendor/lvgl` (sim) / PlatformIO `lib_deps` (firmware); gitignored | MIT | https://github.com/lvgl/lvgl (v9.2) |
| FontAwesome Free (Solid) | baked subset in `examples/companion_radio/ui-lvgl/icons_fa.c` (via `lv_font_conv`) | OFL-1.1 | https://github.com/FortAwesome/Font-Awesome |

The new LVGL UI lives under `examples/companion_radio/ui-lvgl/` with a desktop
prototype in `sim-lvgl/` (`./sim-lvgl/build.sh`). `icons_fa.c` is a generated
LVGL font holding only the ~12 icon glyphs we use.

Used by the on-device chat to render a scanned-message URL as a QR code
(compiled in via `-D HAVE_QRCODEGEN=1`). `qrcodegen.cpp` is the upstream C
source compiled as C++; it is otherwise unmodified.

MeshCore itself and its upstream dependencies are covered by the project's main
license (see `license.txt`).
