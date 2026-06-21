# LVGL desktop prototype

A host build of the new **LVGL** standalone UI, so the redesigned screens can be
seen and clicked on a computer before the on-device (T-Deck) bring-up. The screen
code under [`examples/companion_radio/ui-lvgl/`](../examples/companion_radio/ui-lvgl)
is the *same* source that will run on the device.

## Build & run

```sh
brew install sdl2            # macOS (Linux: your distro's SDL2 dev package)
./sim-lvgl/build.sh          # first run shallow-clones LVGL into vendor/ (gitignored)
./sim-lvgl/lvglsim           # live interactive window (mouse = touch)
```

- **Interactive:** `./sim-lvgl/lvglsim` opens a 320×240 window (true device size).
  Mouse = touch — tap tiles to navigate, the back chevron returns, tap fields to edit.
- **Headless screenshot:** `./sim-lvgl/lvglsim <screen> <out.ppm>` renders one screen
  to a PPM (no window needed). Screen names: `home`, `chat`, `conv`, `settings`,
  `sg0`…`sgN` (a settings group), `edit`, `noise`, `signal`, `heard`, `repeaters`,
  `scan`, `repeater_detail`, `peer`.

LVGL is built once into a cached static lib (`build/liblvgl.a`); delete it to rebuild
LVGL itself. Our screen sources recompile every run, so iterating on the UI is fast.

## How it fits together

- `lv_ui.{h,c}` — shared design kit: palette, glass cards, pills, colour icon chips,
  top bar, and the `lv_nav_cb` navigation hook the firmware screen-manager will reuse.
- `lv_home / lv_chat / lv_settings / lv_diag / lv_repeaters / lv_peer` — the screens.
- `icons_fa.c` + `lv_icons.h` — a FontAwesome Free (OFL) subset baked to an LVGL font
  via `lv_font_conv` (see [`../THIRD_PARTY.md`](../THIRD_PARTY.md)).
- `main.c` — host harness: an LVGL display (SDL window or flush-to-PPM), a tiny nav
  stack, and a `placeholder` for screens not yet ported.

Values shown are demo/sample data; on the device the same screens bind to live
`MyMesh` state at bring-up.
