// LVGL home / launcher screen for the standalone UI (prototype).
// Android-launcher feel: black background, per-feature colored icon chips,
// activity/noise widget cards, status + identity bars. Shared by the desktop
// sim and (later) the T-Deck firmware.
#include "lvgl.h"
#include "lv_icons.h"
#include <stdio.h>

// ---- palette ----------------------------------------------------------------
#define C_TEXT   0xe8edf4
#define C_MUTED  0x8090a4
#define C_CARD   0xffffff   // used at low opacity for glass cards
#define C_GREEN  0x34d399
#define C_PURPLE 0xa855f7
#define C_BADGE  0xf97316

typedef struct { const char* icon; uint32_t color; bool enabled; } Tile;

static const Tile TILES[12] = {
  {ICON_CHAT,      0x3b82f6, true},   // blue
  {ICON_CONTACTS,  0x22c55e, false},  // green
  {ICON_REPEATERS, 0xa855f7, true},   // purple
  {ICON_FINDER,    0x06b6d4, true},   // cyan
  {ICON_HEARD,     0x14b8a6, true},   // teal
  {ICON_MAP,       0xf97316, false},  // orange
  {ICON_ADVERT,    0xec4899, true},   // pink
  {ICON_SETTINGS,  0x6366f1, true},   // indigo
  {ICON_TRACE,     0xeab308, true},   // amber
  {ICON_TERMINAL,  0x10b981, true},   // emerald
  {ICON_NOISE,     0xef4444, true},   // red
  {ICON_SIGNAL,    0x84cc16, true},   // lime
};

static lv_obj_t* glass_card(lv_obj_t* parent, int x, int y, int w, int h) {
  lv_obj_t* c = lv_obj_create(parent);
  lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(c, x, y);
  lv_obj_set_size(c, w, h);
  lv_obj_set_style_radius(c, 16, 0);
  lv_obj_set_style_bg_color(c, lv_color_hex(C_CARD), 0);
  lv_obj_set_style_bg_opa(c, 18, 0);
  lv_obj_set_style_border_color(c, lv_color_hex(C_CARD), 0);
  lv_obj_set_style_border_opa(c, 28, 0);
  lv_obj_set_style_border_width(c, 1, 0);
  lv_obj_set_style_shadow_width(c, 8, 0);
  lv_obj_set_style_shadow_color(c, lv_color_hex(0x000000), 0);
  lv_obj_set_style_shadow_opa(c, 80, 0);
  lv_obj_set_style_pad_all(c, 6, 0);
  return c;
}

static lv_obj_t* pill(lv_obj_t* parent, const char* text, uint32_t color) {
  lv_obj_t* p = lv_obj_create(parent);
  lv_obj_remove_flag(p, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(p, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(p, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(p, 56, 0);
  lv_obj_set_style_border_color(p, lv_color_hex(color), 0);
  lv_obj_set_style_border_opa(p, 180, 0);
  lv_obj_set_style_border_width(p, 1, 0);
  lv_obj_set_style_pad_hor(p, 8, 0);
  lv_obj_set_style_pad_ver(p, 2, 0);
  lv_obj_t* l = lv_label_create(p);
  lv_label_set_text(l, text);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(0xffffff), 0);
  lv_obj_center(l);
  lv_obj_set_size(p, LV_SIZE_CONTENT, 20);
  return p;
}

static void make_tile(lv_obj_t* parent, const Tile* t, int x, int y, int w, int h,
                      bool selected, int badge) {
  lv_obj_t* card = glass_card(parent, x, y, w, h);
  if (selected) {
    lv_obj_set_style_border_color(card, lv_color_hex(t->color), 0);
    lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(t->color), 0);
    lv_obj_set_style_shadow_opa(card, 120, 0);
    lv_obj_set_style_shadow_width(card, 20, 0);
  }

  // colored circular icon chip with a soft glow
  int cs = 34;
  lv_obj_t* chip = lv_obj_create(card);
  lv_obj_remove_flag(chip, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(chip, cs, cs);
  lv_obj_set_style_radius(chip, LV_RADIUS_CIRCLE, 0);
  uint32_t cc = t->enabled ? t->color : 0x39414f;
  lv_obj_set_style_bg_color(chip, lv_color_hex(cc), 0);
  lv_obj_set_style_bg_grad_color(chip, lv_color_darken(lv_color_hex(cc), 90), 0);
  lv_obj_set_style_bg_grad_dir(chip, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_border_width(chip, 0, 0);
  lv_obj_set_style_pad_all(chip, 0, 0);
  lv_obj_set_style_shadow_color(chip, lv_color_hex(cc), 0);
  lv_obj_set_style_shadow_opa(chip, t->enabled ? 130 : 0, 0);
  lv_obj_set_style_shadow_width(chip, t->enabled ? 14 : 0, 0);
  lv_obj_center(chip);

  lv_obj_t* icon = lv_label_create(chip);
  lv_label_set_text(icon, t->icon);
  lv_obj_set_style_text_font(icon, &icons_fa, 0);
  lv_obj_set_style_text_color(icon, lv_color_hex(t->enabled ? 0xffffff : 0x9aa3b2), 0);
  lv_obj_center(icon);

  if (badge > 0) {
    lv_obj_t* b = lv_obj_create(card);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(b, 16, 16);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(C_BADGE), 0);
    lv_obj_set_style_border_color(b, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    lv_obj_align(b, LV_ALIGN_TOP_RIGHT, 2, -2);
    lv_obj_t* bl = lv_label_create(b);
    lv_label_set_text_fmt(bl, "%d", badge);
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(bl, lv_color_hex(0xffffff), 0);
    lv_obj_center(bl);
  }
}

static void make_statusbar(lv_obj_t* scr) {
  lv_obj_t* bar = lv_obj_create(scr);
  lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(bar, 320, 24);
  lv_obj_set_pos(bar, 0, 0);
  lv_obj_set_style_bg_opa(bar, 0, 0);
  lv_obj_set_style_border_width(bar, 0, 0);
  lv_obj_set_style_pad_all(bar, 0, 0);

  lv_obj_t* menu = lv_label_create(bar);
  lv_label_set_text(menu, LV_SYMBOL_LIST);
  lv_obj_set_style_text_color(menu, lv_color_hex(C_TEXT), 0);
  lv_obj_align(menu, LV_ALIGN_LEFT_MID, 8, 0);

  lv_obj_t* p = pill(bar, "Public", 0x3b82f6);
  lv_obj_align(p, LV_ALIGN_LEFT_MID, 30, 0);

  lv_obj_t* clock = lv_label_create(bar);
  lv_label_set_text(clock, "15:34");
  lv_obj_set_style_text_font(clock, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(clock, lv_color_hex(C_TEXT), 0);
  lv_obj_align(clock, LV_ALIGN_RIGHT_MID, -56, 0);

  lv_obj_t* wifi = lv_label_create(bar);
  lv_label_set_text(wifi, LV_SYMBOL_WIFI);
  lv_obj_set_style_text_color(wifi, lv_color_hex(0x06b6d4), 0);
  lv_obj_align(wifi, LV_ALIGN_RIGHT_MID, -30, 0);

  lv_obj_t* batt = lv_label_create(bar);
  lv_label_set_text(batt, LV_SYMBOL_BATTERY_3);
  lv_obj_set_style_text_color(batt, lv_color_hex(C_GREEN), 0);
  lv_obj_align(batt, LV_ALIGN_RIGHT_MID, -8, 0);
}

static lv_obj_t* widget_card(lv_obj_t* scr, int x, int y, int w, int h, const char* title, uint32_t accent) {
  lv_obj_t* c = glass_card(scr, x, y, w, h);
  lv_obj_set_style_pad_all(c, 6, 0);
  lv_obj_t* cap = lv_label_create(c);
  lv_label_set_text(cap, title);
  lv_obj_set_style_text_font(cap, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(cap, lv_color_hex(accent), 0);
  lv_obj_align(cap, LV_ALIGN_TOP_LEFT, 2, 0);
  return c;
}

static void make_hero(lv_obj_t* scr, int y, int h) {
  int w = (320 - 8 * 2 - 8) / 2;

  // Activity (purple bar chart)
  lv_obj_t* a = widget_card(scr, 8, y, w, h, "Activity", C_PURPLE);
  lv_obj_t* bc = lv_chart_create(a);
  lv_obj_set_size(bc, w - 18, h - 24);
  lv_obj_align(bc, LV_ALIGN_BOTTOM_MID, 0, 2);
  lv_obj_set_style_bg_opa(bc, 0, 0); lv_obj_set_style_border_width(bc, 0, 0);
  lv_chart_set_div_line_count(bc, 0, 0);
  lv_chart_set_type(bc, LV_CHART_TYPE_BAR);
  lv_chart_set_point_count(bc, 12);
  lv_chart_set_range(bc, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
  lv_chart_series_t* bs = lv_chart_add_series(bc, lv_color_hex(C_PURPLE), LV_CHART_AXIS_PRIMARY_Y);
  int acts[12] = {20, 45, 30, 70, 55, 90, 40, 65, 80, 35, 60, 50};
  for (int i = 0; i < 12; i++) lv_chart_set_next_value(bc, bs, acts[i]);

  // Noise (green line chart) + value
  lv_obj_t* n = widget_card(scr, 8 + w + 8, y, w, h, "Noise", C_GREEN);
  lv_obj_t* val = lv_label_create(n);
  lv_label_set_text(val, "-104");
  lv_obj_set_style_text_font(val, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(val, lv_color_hex(C_GREEN), 0);
  lv_obj_align(val, LV_ALIGN_TOP_RIGHT, -2, 0);
  lv_obj_t* lc = lv_chart_create(n);
  lv_obj_set_size(lc, w - 18, h - 24);
  lv_obj_align(lc, LV_ALIGN_BOTTOM_MID, 0, 2);
  lv_obj_set_style_bg_opa(lc, 0, 0); lv_obj_set_style_border_width(lc, 0, 0);
  lv_obj_set_style_size(lc, 0, 0, LV_PART_INDICATOR);
  lv_chart_set_div_line_count(lc, 0, 0);
  lv_chart_set_type(lc, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(lc, 28);
  lv_chart_set_range(lc, LV_CHART_AXIS_PRIMARY_Y, -120, -40);
  lv_chart_series_t* ls = lv_chart_add_series(lc, lv_color_hex(C_GREEN), LV_CHART_AXIS_PRIMARY_Y);
  for (int i = 0; i < 28; i++)
    lv_chart_set_next_value(lc, ls, -104 + (int)(7 * lv_trigo_sin(i * 900) / 32767) + (i * 7 % 5) - 2);
  lv_obj_set_style_line_width(lc, 2, LV_PART_ITEMS);
}

static void make_identitybar(lv_obj_t* scr) {
  lv_obj_t* who = lv_label_create(scr);
  lv_label_set_text(who, "Andy  " LV_SYMBOL_RIGHT "  TDeck+");
  lv_obj_set_style_text_font(who, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(who, lv_color_hex(C_MUTED), 0);
  lv_obj_align(who, LV_ALIGN_BOTTOM_LEFT, 8, -3);

  lv_obj_t* sig = lv_label_create(scr);
  lv_label_set_text(sig, LV_SYMBOL_BARS);
  lv_obj_set_style_text_color(sig, lv_color_hex(0x84cc16), 0);
  lv_obj_align(sig, LV_ALIGN_BOTTOM_RIGHT, -8, -3);
}

void lv_home_create(lv_obj_t* scr) {
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x0a0e14), 0);
  lv_obj_set_style_bg_grad_color(scr, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(scr, 0, 0);

  make_statusbar(scr);

  int heroY = 26, heroH = 42;
  make_hero(scr, heroY, heroH);

  int gridTop = heroY + heroH + 6;
  int gridBot = 240 - 16;
  int cols = 4, rows = 3, gap = 6, marginX = 6;
  int cw = (320 - marginX * 2 - gap * (cols - 1)) / cols;
  int rh = (gridBot - gridTop - gap * (rows - 1)) / rows;
  for (int i = 0; i < 12; i++) {
    int r = i / cols, c = i % cols;
    int x = marginX + c * (cw + gap);
    int y = gridTop + r * (rh + gap);
    make_tile(scr, &TILES[i], x, y, cw, rh, i == 7 /*Settings*/, i == 0 ? 2 : 0);
  }

  make_identitybar(scr);
}
