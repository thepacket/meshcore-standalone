#pragma once

// Shared LVGL design kit for the standalone UI: palette + reusable building
// blocks (screen background, glass card, pill, coloured icon chip, top bar) so
// every screen shares one professional, colourful, widget-rich language.
#include "lvgl.h"
#include "lv_icons.h"

#ifdef __cplusplus
extern "C" {
#endif

// palette (on black)
#define UI_TEXT    0xe8edf4
#define UI_MUTED   0x8090a4
#define UI_CARD    0xffffff   // used at low opacity
#define UI_BLUE    0x3b82f6
#define UI_GREEN   0x34d399
#define UI_PURPLE  0xa855f7
#define UI_CYAN    0x06b6d4
#define UI_TEAL    0x14b8a6
#define UI_ORANGE  0xf97316
#define UI_PINK    0xec4899
#define UI_INDIGO  0x6366f1
#define UI_AMBER   0xeab308
#define UI_EMERALD 0x10b981
#define UI_RED     0xef4444
#define UI_LIME    0x84cc16
#define UI_BADGE   0xf97316

// paint a screen with the standard black vertical gradient
void lv_ui_screen_bg(lv_obj_t* scr);

// a translucent rounded "glass" card
lv_obj_t* lv_ui_card(lv_obj_t* parent, int x, int y, int w, int h);

// a small coloured rounded pill containing text
lv_obj_t* lv_ui_pill(lv_obj_t* parent, const char* text, uint32_t color);

// a circular coloured icon chip (gradient + glow) with a white glyph (icons_fa)
lv_obj_t* lv_ui_chip(lv_obj_t* parent, uint32_t color, const char* icon, int size, bool enabled);

// a sub-screen top bar (back chevron + title + accent underline). Returns the
// bar; *back_btn (if non-NULL) receives the clickable back button.
lv_obj_t* lv_ui_topbar(lv_obj_t* scr, const char* title, uint32_t accent, lv_obj_t** back_btn);

// Navigation hook: screens call lv_nav_cb(dest) when a tappable element is
// activated. The host (sim or firmware screen-manager) sets this. "back" pops.
typedef void (*lv_nav_fn)(const char* dest);
extern lv_nav_fn lv_nav_cb;
// make `o` clickable so a tap routes to lv_nav_cb(dest) (dest must outlive o)
void lv_ui_clickable(lv_obj_t* o, const char* dest);

#ifdef __cplusplus
}
#endif
