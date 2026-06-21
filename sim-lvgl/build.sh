#!/bin/bash
# Build the LVGL desktop prototype (headless screenshot harness).
# Caches LVGL as a static lib (liblvgl.a) so iterating on our UI is fast.
set -e
cd "$(dirname "$0")/.."   # repo root

LVGL=sim-lvgl/vendor/lvgl
OUT=sim-lvgl/build
mkdir -p "$OUT" sim-lvgl/shots

# fetch LVGL (MIT) on first run; gitignored, not committed
if [ ! -d "$LVGL" ]; then
  echo "cloning LVGL v9.2 (one-time)..."
  git clone --depth 1 --branch release/v9.2 https://github.com/lvgl/lvgl.git "$LVGL"
fi

BREW=/opt/homebrew/bin/brew
SDL=$($BREW --prefix sdl2)
SDL_CFLAGS="-I$SDL/include"
SDL_LIBS="-L$SDL/lib -lSDL2"

CFLAGS="-I sim-lvgl -I $LVGL -I examples/companion_radio/ui-lvgl $SDL_CFLAGS -DLV_CONF_INCLUDE_SIMPLE -O2 -Wno-unused-function"

# 1. Build LVGL into a static lib once (delete sim-lvgl/build/liblvgl.a to rebuild)
if [ ! -f "$OUT/liblvgl.a" ]; then
  echo "compiling LVGL (one-time, cached)..."
  mkdir -p "$OUT/obj"
  i=0
  while IFS= read -r src; do
    obj="$OUT/obj/$(echo "$src" | sed 's#[/.]#_#g').o"
    cc $CFLAGS -c "$src" -o "$obj" &
    i=$((i+1)); if [ $((i % 8)) -eq 0 ]; then wait; fi
  done < <(find "$LVGL/src" -name '*.c')
  wait
  ar rcs "$OUT/liblvgl.a" "$OUT"/obj/*.o
  echo "built $OUT/liblvgl.a"
fi

# 2. Compile our UI + harness and link
cc $CFLAGS \
  sim-lvgl/main.c \
  examples/companion_radio/ui-lvgl/lv_ui.c \
  examples/companion_radio/ui-lvgl/lv_home.c \
  examples/companion_radio/ui-lvgl/lv_chat.c \
  examples/companion_radio/ui-lvgl/icons_fa.c \
  "$OUT/liblvgl.a" \
  $SDL_LIBS -lm \
  -o sim-lvgl/lvglsim

echo "built sim-lvgl/lvglsim   ->  run:  ./sim-lvgl/lvglsim sim-lvgl/shots/home.ppm"
