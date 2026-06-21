// LVGL diagnostics: Noise scope (chart), Signal (coverage gauges), Heard (list).
#include "lv_ui.h"
#include <stdio.h>

static lv_obj_t* full_list(lv_obj_t* scr) {
  lv_obj_t* list = lv_obj_create(scr);
  lv_obj_set_pos(list, 4, 34);
  lv_obj_set_size(list, 320 - 8, 240 - 34 - 4);
  lv_obj_set_style_bg_opa(list, 0, 0);
  lv_obj_set_style_border_width(list, 0, 0);
  lv_obj_set_style_pad_all(list, 2, 0);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(list, 6, 0);
  return list;
}

static void stat_pill(lv_obj_t* parent, const char* k, const char* v, uint32_t color) {
  lv_obj_t* p = lv_ui_card(parent, -1, 0, 0, 0);
  lv_obj_set_flex_grow(p, 1);
  lv_obj_set_height(p, 38);
  lv_obj_set_style_pad_all(p, 4, 0);
  lv_obj_set_flex_flow(p, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(p, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t* vl = lv_label_create(p);
  lv_label_set_text(vl, v);
  lv_obj_set_style_text_font(vl, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(vl, lv_color_hex(color), 0);
  lv_obj_t* kl = lv_label_create(p);
  lv_label_set_text(kl, k);
  lv_obj_set_style_text_font(kl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(kl, lv_color_hex(UI_MUTED), 0);
}

void lv_noise_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_topbar(scr, "Noise", UI_RED, NULL);

  // stat row
  lv_obj_t* row = lv_obj_create(scr);
  lv_obj_set_pos(row, 6, 36); lv_obj_set_size(row, 320 - 12, 42);
  lv_obj_set_style_bg_opa(row, 0, 0); lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 6, 0);
  stat_pill(row, "now dBm", "-104", UI_GREEN);
  stat_pill(row, "min", "-118", UI_CYAN);
  stat_pill(row, "max", "-92", UI_AMBER);

  // big scope chart
  lv_obj_t* card = lv_ui_card(scr, 6, 84, 320 - 12, 240 - 84 - 6);
  lv_obj_t* chart = lv_chart_create(card);
  lv_obj_set_size(chart, 320 - 12 - 20, 240 - 84 - 6 - 20);
  lv_obj_center(chart);
  lv_obj_set_style_bg_opa(chart, 0, 0); lv_obj_set_style_border_width(chart, 0, 0);
  lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR);
  lv_chart_set_div_line_count(chart, 4, 0);
  lv_obj_set_style_line_color(chart, lv_color_hex(0x223046), LV_PART_MAIN);
  lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(chart, 60);
  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, -120, -40);
  lv_chart_series_t* s = lv_chart_add_series(chart, lv_color_hex(UI_GREEN), LV_CHART_AXIS_PRIMARY_Y);
  for (int i = 0; i < 60; i++)
    lv_chart_set_next_value(chart, s, -104 + (int)(9 * lv_trigo_sin(i * 500) / 32767) + (i * 13 % 7) - 3);
  lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS);
}

// ---- Signal: per-repeater coverage gauges -----------------------------------
typedef struct { const char* name; int rssi; const char* age; bool heard; } Rep;

static void coverage_row(lv_obj_t* list, const Rep* r) {
  lv_obj_t* row = lv_ui_card(list, -1, 0, 0, 52);
  lv_obj_set_width(row, lv_pct(100));
  lv_obj_set_height(row, 50);
  lv_obj_set_style_min_height(row, 0, 0);
  lv_obj_set_style_pad_hor(row, 10, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(row, 4, 0);

  lv_obj_t* head = lv_obj_create(row);
  lv_obj_remove_flag(head, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_width(head, lv_pct(100)); lv_obj_set_height(head, 16);
  lv_obj_set_style_bg_opa(head, 0, 0); lv_obj_set_style_border_width(head, 0, 0);
  lv_obj_set_style_pad_all(head, 0, 0);
  lv_obj_t* nm = lv_label_create(head);
  lv_label_set_text(nm, r->name);
  lv_obj_set_style_text_font(nm, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(nm, lv_color_hex(r->heard ? UI_TEXT : UI_MUTED), 0);
  lv_obj_align(nm, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_t* ag = lv_label_create(head);
  lv_label_set_text(ag, r->age);
  lv_obj_set_style_text_font(ag, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(ag, lv_color_hex(UI_MUTED), 0);
  lv_obj_align(ag, LV_ALIGN_RIGHT_MID, 0, 0);

  int pct = r->heard ? (r->rssi + 120) * 100 / 70 : 0;   // -120..-50 -> 0..100
  if (pct < 0) pct = 0; if (pct > 100) pct = 100;
  uint32_t col = pct > 66 ? UI_GREEN : (pct > 33 ? UI_AMBER : UI_RED);
  lv_obj_t* bar = lv_bar_create(row);
  lv_obj_set_width(bar, lv_pct(100)); lv_obj_set_height(bar, 8);
  lv_obj_set_style_bg_color(bar, lv_color_hex(0x2a3343), LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar, lv_color_hex(col), LV_PART_INDICATOR);
  lv_obj_set_style_radius(bar, 4, LV_PART_MAIN);
  lv_obj_set_style_radius(bar, 4, LV_PART_INDICATOR);
  lv_bar_set_value(bar, pct, LV_ANIM_OFF);
}

void lv_signal_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_topbar(scr, "Signal", UI_LIME, NULL);
  lv_obj_t* list = full_list(scr);
  Rep reps[] = {
    {"GW-Hertford", -72, "12s", true},
    {"Hilltop-Relay", -96, "2m", true},
    {"Town Square", -110, "30m", true},
    {"Field-Node", 0, "stale", false},
  };
  for (unsigned i = 0; i < sizeof(reps)/sizeof(reps[0]); i++) coverage_row(list, &reps[i]);
}

// ---- Heard: recent stations -------------------------------------------------
typedef struct { const char* name; const char* meta; int bars; } Stn;

static void heard_row(lv_obj_t* list, const Stn* s) {
  lv_obj_t* row = lv_ui_card(list, -1, 0, 0, 46);
  lv_obj_set_width(row, lv_pct(100));
  lv_obj_set_height(row, 44);
  lv_obj_set_style_min_height(row, 0, 0);
  lv_obj_set_style_pad_hor(row, 10, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  uint32_t col = s->bars >= 3 ? UI_GREEN : (s->bars == 2 ? UI_AMBER : UI_RED);
  lv_obj_t* dot = lv_obj_create(row);
  lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(dot, 10, 10); lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(dot, lv_color_hex(col), 0);
  lv_obj_set_style_border_width(dot, 0, 0);
  lv_obj_set_style_shadow_color(dot, lv_color_hex(col), 0);
  lv_obj_set_style_shadow_width(dot, 8, 0); lv_obj_set_style_shadow_opa(dot, 160, 0);
  lv_obj_set_style_margin_right(dot, 10, 0);

  lv_obj_t* mid = lv_obj_create(row);
  lv_obj_remove_flag(mid, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(mid, 0, 0); lv_obj_set_style_border_width(mid, 0, 0);
  lv_obj_set_style_pad_all(mid, 0, 0); lv_obj_set_flex_grow(mid, 1); lv_obj_set_height(mid, 34);
  lv_obj_set_flex_flow(mid, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(mid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_t* nm = lv_label_create(mid);
  lv_label_set_text(nm, s->name);
  lv_obj_set_style_text_font(nm, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(nm, lv_color_hex(UI_TEXT), 0);
  lv_obj_t* mt = lv_label_create(mid);
  lv_label_set_text(mt, s->meta);
  lv_obj_set_style_text_font(mt, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(mt, lv_color_hex(UI_MUTED), 0);
}

void lv_heard_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_topbar(scr, "Heard", UI_TEAL, NULL);
  lv_obj_t* list = full_list(scr);
  Stn st[] = {
    {"Repeater-7",  "9.0 dB  -78 dBm  4.2 km  12s", 4},
    {"Andy-Mobile", "6.0 dB  -92 dBm  0.8 km  45s", 2},
    {"GW-Hertford", "2.0 dB  -108 dBm  10m",         1},
  };
  for (unsigned i = 0; i < sizeof(st)/sizeof(st[0]); i++) heard_row(list, &st[i]);
}
