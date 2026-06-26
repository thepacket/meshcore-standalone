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
  {ICON_CHANNELS,  UI_CYAN,    true,  "channels"},
  {ICON_HEARD,     UI_TEAL,    true,  "heard"},
  {ICON_MAP,       UI_ORANGE,  false, ""},
  {ICON_ADVERT,    UI_PINK,    true,  "discover"},
  {ICON_SETTINGS,  UI_INDIGO,  true,  "settings"},
  {ICON_TRACE,     UI_AMBER,   true,  "trace"},
  {ICON_PACKETS,   UI_EMERALD, true,  "terminal"},
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
    lv_ui_press_fx(card);
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
static lv_obj_t* s_rxtx  = NULL;   // "rx N  tx M" packet counters
// live hero widgets (Latest = last-RX RSSI/SNR + strength bar, RF = live channel
// RSSI scope sampled fast on its own timer)
static lv_obj_t* s_latest;   static lv_obj_t* s_strength;
static lv_obj_t* s_rf_chart; static lv_chart_series_t* s_rf_s;
static lv_obj_t* s_rf_val;   static lv_timer_t* s_rf_timer = NULL;

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
  if (s_rxtx)  { char b[48]; snprintf(b, sizeof(b), "RX %u  RE %u  TX %u  CT %d", lvd_pkt_recv(), lvd_pkt_recv_err(), lvd_pkt_sent(), lvd_contact_total()); lv_label_set_text(s_rxtx, b); }
  if (s_latest) {
    if (lvd_pkt_recv() == 0) {
      lv_label_set_text(s_latest, "RSSI --  SNR --");
      lv_bar_set_value(s_strength, 0, LV_ANIM_OFF);
    } else {
      int rssi = lvd_last_rssi();
      int v10 = lvd_last_snr_q() * 10 / 4, a = v10 < 0 ? -v10 : v10;
      char b[40]; snprintf(b, sizeof(b), "RSSI %d  SNR %s%d.%d", rssi, v10 < 0 ? "-" : "", a / 10, a % 10);
      lv_label_set_text(s_latest, b);
      int pct = (rssi + 120) * 100 / 70; if (pct < 0) pct = 0; else if (pct > 100) pct = 100;
      lv_bar_set_value(s_strength, pct, LV_ANIM_OFF);   // min left, max right
      uint32_t col = pct > 66 ? UI_GREEN : (pct > 33 ? UI_AMBER : UI_RED);
      lv_obj_set_style_bg_color(s_strength, lv_color_hex(col), LV_PART_INDICATOR);
    }
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

  // live packet counters (left of the status bar)
  lv_obj_t* rxtx = lv_label_create(bar);
  lv_label_set_text(rxtx, "RX 0  RE 0  TX 0  CT 0");
  lv_obj_set_style_text_font(rxtx, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(rxtx, lv_color_hex(UI_MUTED), 0);
  lv_obj_align(rxtx, LV_ALIGN_LEFT_MID, 8, 0);
  s_rxtx = rxtx;

  lv_obj_t* clock = lv_label_create(bar);
  char tbuf[8]; lvd_clock_hhmm(tbuf, sizeof(tbuf));
  lv_label_set_text(clock, tbuf);
  lv_obj_set_style_text_font(clock, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(clock, lv_color_hex(UI_TEXT), 0);
  lv_obj_align(clock, LV_ALIGN_RIGHT_MID, -34, 0);   // just left of the battery icon
  s_clock = clock;

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

// Fast RF scope: sample the instantaneous channel RSSI and shift it onto the
// chart ~7x/s, so real RF energy (packets, interference) shows as live spikes.
// Uses the same fixed scale as the full scope (RF_MIN_DBM .. lv_ui_rf_max()) so
// the two match; idle noise sits near the floor, signals rise toward the max.
static void rf_tick(lv_timer_t* t) {
  (void)t;
  if (!s_rf_chart) return;
  int rssi = lvd_rf_rssi();
  lv_chart_set_next_value(s_rf_chart, s_rf_s, rssi);
  lv_chart_set_range(s_rf_chart, LV_CHART_AXIS_PRIMARY_Y, RF_MIN_DBM, lv_ui_rf_max());
  if (s_rf_val) { char b[12]; snprintf(b, sizeof(b), "%d", rssi); lv_label_set_text(s_rf_val, b); }
}
// when the chart is destroyed (leaving home / rebuild), kill its sampler so it
// never pushes into a freed object
static void rf_chart_deleted(lv_event_t* e) {
  (void)e;
  if (s_rf_timer) { lv_timer_del(s_rf_timer); s_rf_timer = NULL; }
  s_rf_chart = NULL;
}

static void make_hero(lv_obj_t* scr, int y, int h) {
  int w = (320 - 8 * 2 - 8) / 2;

  lv_obj_t* a = widget_card(scr, 8, y, w, h, "", UI_PURPLE);
  s_latest = lv_label_create(a);
  lv_label_set_text(s_latest, "RSSI --  SNR --");
  lv_obj_set_style_text_font(s_latest, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(s_latest, lv_color_hex(UI_TEXT), 0);
  lv_obj_align(s_latest, LV_ALIGN_TOP_LEFT, 2, 4);
  s_strength = lv_bar_create(a);                 // strength meter: min left, max right
  lv_obj_set_size(s_strength, w - 18, 6);
  lv_obj_align(s_strength, LV_ALIGN_BOTTOM_MID, 0, -2);
  lv_obj_set_style_bg_color(s_strength, lv_color_hex(0x2a3343), LV_PART_MAIN);
  lv_obj_set_style_radius(s_strength, 3, LV_PART_MAIN);
  lv_obj_set_style_radius(s_strength, 3, LV_PART_INDICATOR);
  lv_bar_set_range(s_strength, 0, 100);
  lv_bar_set_value(s_strength, 0, LV_ANIM_OFF);

  // RF card: no title/value labels -- the plot fills the whole panel
  lv_obj_t* n = lv_ui_card(scr, 8 + w + 8, y, w, h);
  lv_obj_set_style_pad_all(n, 3, 0);
  s_rf_val = NULL;
  s_rf_chart = lv_chart_create(n);
  lv_obj_set_size(s_rf_chart, w - 8, h - 8);
  lv_obj_center(s_rf_chart);
  lv_obj_remove_flag(s_rf_chart, LV_OBJ_FLAG_CLICKABLE);   // let taps reach the card (opens scope)
  lv_obj_remove_flag(s_rf_chart, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(s_rf_chart, 0, 0); lv_obj_set_style_border_width(s_rf_chart, 0, 0);
  lv_obj_set_style_size(s_rf_chart, 0, 0, LV_PART_INDICATOR);
  lv_chart_set_div_line_count(s_rf_chart, 0, 0);
  lv_chart_set_type(s_rf_chart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(s_rf_chart, 40);
  lv_chart_set_range(s_rf_chart, LV_CHART_AXIS_PRIMARY_Y, RF_MIN_DBM, lv_ui_rf_max());  // same scale as the scope
  s_rf_s = lv_chart_add_series(s_rf_chart, lv_color_hex(UI_GREEN), LV_CHART_AXIS_PRIMARY_Y);
  lv_obj_set_style_line_width(s_rf_chart, 2, LV_PART_ITEMS);

  // drive the scope from its own fast timer; tear it down with the chart
  lv_obj_add_event_cb(s_rf_chart, rf_chart_deleted, LV_EVENT_DELETE, NULL);
  if (s_rf_timer) lv_timer_del(s_rf_timer);
  s_rf_timer = lv_timer_create(rf_tick, 150, NULL);

  // tap the card -> full RF scope with a manual scale slider
  lv_obj_add_flag(n, LV_OBJ_FLAG_CLICKABLE);
  lv_ui_clickable(n, "rfscope");
}

static void make_identitybar(lv_obj_t* scr) {
  lv_obj_t* who = lv_label_create(scr);
  lv_label_set_text_fmt(who, "%s  " LV_SYMBOL_RIGHT "  %s", lvd_node_name(), lvd_device_label());
  lv_obj_set_style_text_font(who, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(who, lv_color_hex(UI_MUTED), 0);
  lv_obj_align(who, LV_ALIGN_BOTTOM_LEFT, 8, -3);
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
    make_tile(scr, &TILES[i], i, x, y, cw, rh, i == g_home_sel, i == 0 ? lvd_unread_count() : 0);
  }

  make_identitybar(scr);

  lv_ui_set_refresh(home_tick);   // live status bar (clock + battery) while home is up
}
