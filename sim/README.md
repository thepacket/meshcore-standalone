# Desktop UI simulator

Exercise the standalone on-device UI on your computer, without the T-Deck hardware.

It compiles the **real** screen sources (`examples/companion_radio/ui-new/SettingsScreen.cpp`,
`OnScreenKeyboard.cpp`) against an SDL2 `DisplayDriver` and a mock in-memory settings
model, so the behavior matches the firmware (it's even built with `-funsigned-char` to
match the ESP32's `char` signedness). Only the model is faked; the UI logic is the genuine
on-device code.

## Requirements (macOS)

```sh
brew install sdl2 sdl2_ttf
```

## Build & run

```sh
./sim/build.sh          # produces sim/uisim
./sim/uisim             # interactive window
./sim/uisim --shot      # render a few screens to sim/shots/*.ppm and exit
```

## Controls

| Input | Maps to |
|-------|---------|
| Mouse click / drag | Touch tap / scroll |
| Arrow keys | Trackball up/down/left/right |
| Enter | Select / OK |
| Esc | Back / Cancel |
| Backspace | Delete (text entry) |
| Typing | Physical keyboard characters |

The on-screen keyboard appears for text fields (e.g. Node name); you can tap it or type.

## Notes

- Changes are not persisted — the mock model resets each run.
- Window is 320x240 — 1:1 with the real T-Deck panel (no pixel inflation), so the
  layout/legibility you see matches the device resolution. (Exact glyphs differ:
  the sim uses Menlo, the device uses LovyanGFX's built-in font.)
- This is a development tool; it is not built or shipped as firmware.
