#!/bin/bash
# Build the desktop UI simulator. Compiles the REAL on-device screen sources
# against a host SDL2 DisplayDriver + a mock settings model, so the actual UI
# logic can be exercised without the T-Deck hardware.
set -e
cd "$(dirname "$0")/.."   # repo root

SRC=examples/companion_radio/ui-new
OUT=sim/build
rm -rf "$OUT"; mkdir -p "$OUT" sim/shots

# Copy the real screen sources into the build dir, then drop in the host UITask.h
# shim. Because C/C++ resolves #include "UITask.h" relative to the including
# file's own directory first, placing the shim alongside the copied sources makes
# them compile against it instead of the firmware's UITask.h.
cp "$SRC/SettingsScreen.cpp" "$SRC/SettingsScreen.h" "$SRC/SettingsModel.h" \
   "$SRC/OnScreenKeyboard.cpp" "$SRC/OnScreenKeyboard.h" \
   "$SRC/PacketMonitorScreen.cpp" "$SRC/PacketMonitorScreen.h" \
   "$SRC/HomeLauncherScreen.cpp" "$SRC/HomeLauncherScreen.h" "$SRC/TileIcons.h" \
   "$SRC/NoiseScopeScreen.cpp" "$SRC/NoiseScopeScreen.h" \
   "$SRC/LastHeardScreen.cpp" "$SRC/LastHeardScreen.h" \
   "$SRC/SignalScreen.cpp" "$SRC/SignalScreen.h" \
   "$SRC/TraceRouteScreen.cpp" "$SRC/TraceRouteScreen.h" \
   "$SRC/ChatStore.cpp" "$SRC/ChatStore.h" \
   "$SRC/ChatHomeScreen.cpp" "$SRC/ChatHomeScreen.h" \
   "$SRC/ConversationScreen.cpp" "$SRC/ConversationScreen.h" \
   "$SRC/RichText.cpp" "$SRC/RichText.h" "$SRC/EmojiGlyphs.h" \
   "$SRC/qrcodegen.cpp" "$SRC/qrcodegen.h" \
   "$SRC/UIStyle.h" "$OUT/"
cp sim/UITask.h sim/SimDisplay.h "$OUT/"

BREW=/opt/homebrew/bin/brew
SDL=$($BREW --prefix sdl2)
TTF=$($BREW --prefix sdl2_ttf)
SDL_CFLAGS="-I$SDL/include/SDL2 -I$TTF/include/SDL2"
SDL_LIBS="-L$SDL/lib -L$TTF/lib -lSDL2 -lSDL2_ttf"

clang++ -std=c++17 -DSIM_BUILD -DHAVE_QRCODEGEN=1 -funsigned-char \
  -I "$OUT" -I src -I sim \
  $SDL_CFLAGS \
  "$OUT/SettingsScreen.cpp" "$OUT/OnScreenKeyboard.cpp" "$OUT/PacketMonitorScreen.cpp" \
  "$OUT/HomeLauncherScreen.cpp" "$OUT/NoiseScopeScreen.cpp" "$OUT/LastHeardScreen.cpp" \
  "$OUT/SignalScreen.cpp" "$OUT/TraceRouteScreen.cpp" \
  "$OUT/ChatStore.cpp" "$OUT/ChatHomeScreen.cpp" "$OUT/ConversationScreen.cpp" \
  "$OUT/RichText.cpp" "$OUT/qrcodegen.cpp" \
  sim/SimDisplay.cpp sim/sim_settings_model.cpp sim/sim_main.cpp \
  $SDL_LIBS \
  -o sim/uisim

echo "built sim/uisim   ->  run:  ./sim/uisim        (interactive)"
echo "                       or:  ./sim/uisim --shot  (render screens to sim/shots/)"
