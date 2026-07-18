# Third-party components

This project is licensed **GPL-3.0-or-later** (see `LICENSE` and `NOTICE`). It
vendors / ports the following third-party code; each retains its own license
(all GPL-3.0-compatible), and the original license/attribution headers are
preserved in the source files.

| Component | Where | License | Source |
|-----------|-------|---------|--------|
| wadamesh — WebMirror remote-screen feature (ported) | `examples/companion_radio/ui-lvgl/WebMirror.{h,cpp}`, `WebScreen.{h,cpp}`, band/RLE + indev code in `UITask.cpp` | GPL-3.0-or-later | wadamesh (Kaj Schittecat) |
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
