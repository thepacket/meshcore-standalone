// LVGL home / launcher: black background, per-feature coloured icon chips,
// Activity/Noise widget cards, status + identity bars. Uses the shared lv_ui kit.
#include "lv_ui.h"
#include "lv_data.h"
#include <stdio.h>

typedef struct { const char* icon; uint32_t color; bool enabled; const char* dest; } Tile;

static const Tile TILES[12] = {
  {ICON_CHAT,      UI_BLUE,    true,  "chat"},
  {ICON_CONTACTS,  UI_GREEN,   true,  "contacts"},
  {ICON_REPEATERS, UI_PURPLE,  true,  "repeaters"},
  {ICON_FINDER,    UI_CYAN,    true,  "scan"},
  {ICON_HEARD,     UI_TEAL,    true,  "heard"},
  {ICON_MAP,       UI_ORANGE,  false, ""},
  {ICON_ADVERT,    UI_PINK,    true,  "discover"},
  {ICON_SETTINGS,  UI_INDIGO,  true,  "settings"},
  {ICON_TRACE,     UI_AMBER,   true,  "trace"},
  {ICON_TERMINAL,  UI_EMERALD, true,  "terminal"},
  {ICON_NOISE,     UI_PURPLE,  true,  "stats"},
  {ICON_SIGNAL,    UI_LIME,    true,  "signal"},
};

static int g_home_sel = 7;   // last-selected tile; restored when returning home
// record the selected tile BEFORE navigating (nav destroys this screen), then go
static void tile_clicked(lv_event_t* e) {
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  g_home_sel = idx;
  if (lv_nav_cb && TILES[idx].dest[0]) lv_nav_cb(TILES[idx].dest);
}

static void make_tile(lv_obj_t* parent, const Tile* t, int idx, int x, int y, int w, int h,
                      bool selected, int badge) {
  lv_obj_t* card = lv_ui_card(parent, x, y, w, h);
  if (selected) {
    lv_obj_set_style_border_color(card, lv_color_hex(t->color), 0);
    lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(t->color), 0);
    lv_obj_set_style_shadow_opa(card, 120, 0);
    lv_obj_set_style_shadow_width(card, 20, 0);
  }
  lv_obj_t* chip = lv_ui_chip(card, t->color, t->icon, 34, t->enabled);
  lv_obj_center(chip);
  if (t->enabled && t->dest[0]) {
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, tile_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)idx);
  }

  if (badge > 0) {
    lv_obj_t* b = lv_obj_create(card);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(b, 16, 16);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(UI_BADGE), 0);
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

// live status-bar widgets, updated in place by home_tick() (registered as the
// screen's refresh hook). Valid only while the home screen is active.
static lv_obj_t* s_clock = NULL;
static lv_obj_t* s_batt  = NULL;

static const char* batt_symbol(int bp) {
  return bp < 0  ? LV_SYMBOL_BATTERY_EMPTY :
         bp > 80 ? LV_SYMBOL_BATTERY_FULL  :
         bp > 55 ? LV_SYMBOL_BATTERY_3     :
         bp > 30 ? LV_SYMBOL_BATTERY_2     :
         bp > 10 ? LV_SYMBOL_BATTERY_1     : LV_SYMBOL_BATTERY_EMPTY;
}
static void apply_batt(lv_obj_t* b, int bp) {
  lv_label_set_text(b, batt_symbol(bp));
  lv_obj_set_style_text_color(b, lv_color_hex(bp >= 0 && bp <= 20 ? UI_RED : UI_GREEN), 0);
}
static void home_tick(void) {
  if (s_clock) { char t[8]; lvd_clock_hhmm(t, sizeof(t)); lv_label_set_text(s_clock, t); }
  if (s_batt)  apply_batt(s_batt, lvd_batt_pct());
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
  lv_obj_set_style_text_color(menu, lv_color_hex(UI_TEXT), 0);
  lv_obj_align(menu, LV_ALIGN_LEFT_MID, 8, 0);

  lv_obj_t* p = lv_ui_pill(bar, "Public", UI_BLUE);
  lv_obj_align(p, LV_ALIGN_LEFT_MID, 30, 0);

  lv_obj_t* clock = lv_label_create(bar);
  char tbuf[8]; lvd_clock_hhmm(tbuf, sizeof(tbuf));
  lv_label_set_text(clock, tbuf);
  lv_obj_set_style_text_font(clock, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(clock, lv_color_hex(UI_TEXT), 0);
  lv_obj_align(clock, LV_ALIGN_RIGHT_MID, -56, 0);
  s_clock = clock;

  lv_obj_t* wifi = lv_label_create(bar);
  lv_label_set_text(wifi, LV_SYMBOL_WIFI);
  lv_obj_set_style_text_color(wifi, lv_color_hex(UI_CYAN), 0);
  lv_obj_align(wifi, LV_ALIGN_RIGHT_MID, -30, 0);

  lv_obj_t* batt = lv_label_create(bar);
  apply_batt(batt, lvd_batt_pct());
  lv_obj_align(batt, LV_ALIGN_RIGHT_MID, -8, 0);
  s_batt = batt;
}

static lv_obj_t* widget_card(lv_obj_t* scr, int x, int y, int w, int h, const char* title, uint32_t accent) {
  lv_obj_t* c = lv_ui_card(scr, x, y, w, h);
  lv_obj_t* cap = lv_label_create(c);
  lv_label_set_text(cap, title);
  lv_obj_set_style_text_font(cap, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(cap, lv_color_hex(accent), 0);
  lv_obj_align(cap, LV_ALIGN_TOP_LEFT, 2, 0);
  return c;
}

static void make_hero(lv_obj_t* scr, int y, int h) {
  int w = (320 - 8 * 2 - 8) / 2;

  lv_obj_t* a = widget_card(scr, 8, y, w, h, "Activity", UI_PURPLE);
  lv_obj_t* bc = lv_chart_create(a);
  lv_obj_set_size(bc, w - 18, h - 24);
  lv_obj_align(bc, LV_ALIGN_BOTTOM_MID, 0, 2);
  lv_obj_set_style_bg_opa(bc, 0, 0); lv_obj_set_style_border_width(bc, 0, 0);
  lv_chart_set_div_line_count(bc, 0, 0);
  lv_chart_set_type(bc, LV_CHART_TYPE_BAR);
  lv_chart_set_point_count(bc, 12);
  lv_chart_set_range(bc, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
  lv_chart_series_t* bs = lv_chart_add_series(bc, lv_color_hex(UI_PURPLE), LV_CHART_AXIS_PRIMARY_Y);
  int acts[12] = {20, 45, 30, 70, 55, 90, 40, 65, 80, 35, 60, 50};
  for (int i = 0; i < 12; i++) lv_chart_set_next_value(bc, bs, acts[i]);

  lv_obj_t* n = widget_card(scr, 8 + w + 8, y, w, h, "Noise", UI_GREEN);
  lv_obj_t* val = lv_label_create(n);
  lv_label_set_text(val, "-104");
  lv_obj_set_style_text_font(val, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(val, lv_color_hex(UI_GREEN), 0);
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
  lv_chart_series_t* ls = lv_chart_add_series(lc, lv_color_hex(UI_GREEN), LV_CHART_AXIS_PRIMARY_Y);
  for (int i = 0; i < 28; i++)
    lv_chart_set_next_value(lc, ls, -104 + (int)(7 * lv_trigo_sin(i * 900) / 32767) + (i * 7 % 5) - 2);
  lv_obj_set_style_line_width(lc, 2, LV_PART_ITEMS);
}

static void make_identitybar(lv_obj_t* scr) {
  lv_obj_t* who = lv_label_create(scr);
  lv_label_set_text_fmt(who, "%s  " LV_SYMBOL_RIGHT "  %s", lvd_node_name(), lvd_device_label());
  lv_obj_set_style_text_font(who, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(who, lv_color_hex(UI_MUTED), 0);
  lv_obj_align(who, LV_ALIGN_BOTTOM_LEFT, 8, -3);

  lv_obj_t* sig = lv_label_create(scr);
  lv_label_set_text(sig, LV_SYMBOL_BARS);
  lv_obj_set_style_text_color(sig, lv_color_hex(UI_LIME), 0);
  lv_obj_align(sig, LV_ALIGN_BOTTOM_RIGHT, -8, -3);
}

void lv_home_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
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
    make_tile(scr, &TILES[i], i, x, y, cw, rh, i == g_home_sel, i == 0 ? 2 : 0);
  }

  make_identitybar(scr);

  lv_ui_set_refresh(home_tick);   // live status bar (clock + battery) while home is up
}
