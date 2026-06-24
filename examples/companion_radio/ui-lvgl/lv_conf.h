// LVGL configuration for the on-device T-Deck build (companion_radio LVGL UI).
// Mirrors the desktop sim (sim-lvgl/lv_conf.h) but targets the real panel:
// RGB565, no SDL, tick from a callback, malloc from the (PSRAM-backed) heap.
#pragma once
#if 1  // enable content

#define LV_COLOR_DEPTH 16

// LovyanGFX pushImage(rgb565_t*) takes native RGB565 and handles the SPI wire
// byte order itself, so LVGL should NOT pre-swap. (Flip to 1 if reds<->blues.)
#define LV_COLOR_16_SWAP 0

// --- memory / OS ---
#define LV_USE_OS LV_OS_NONE
#define LV_USE_STDLIB_MALLOC  LV_STDLIB_CLIB   // use heap malloc (ESP32)
#define LV_USE_STDLIB_STRING  LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB

// tick is provided via lv_tick_set_cb() (millis) in UITask.cpp
#define LV_DEF_REFR_PERIOD 16

// --- drawing ---
#define LV_USE_DRAW_SW 1

// --- theme ---
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1

// --- fonts (the screens use 12/14/16/18) ---
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_DEFAULT &lv_font_montserrat_16

// --- widgets used by the screens ---
#define LV_USE_QRCODE 1
#define LV_USE_KEYBOARD 1
#define LV_USE_CHART 1

#endif
