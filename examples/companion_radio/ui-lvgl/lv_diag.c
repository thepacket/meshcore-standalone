// LVGL diagnostics: Noise scope (chart), Signal (coverage gauges), Heard (list).
#include "lv_ui.h"
#include "lv_data.h"
#include <stdio.h>
#include <stdint.h>

void lv_chat_set_peer(const char* name);  // peer-details target (lv_chat.c)

static void heard_clicked(lv_event_t* e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  lvd_heard_t s;
  if (lvd_heard_get(i, &s)) { lv_chat_set_peer(s.name); if (lv_nav_cb) lv_nav_cb("peer"); }
}

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
  lv_ui_md_topbar(scr, "Noise");

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

// ---- Signal: per-repeater coverage gauges (live, from the heard table) ------
static void coverage_row(lv_obj_t* list, const lvd_sig_t* r) {
  lv_obj_t* row = lv_ui_card(list, -1, 0, 0, 60);
  lv_obj_set_width(row, lv_pct(100));
  lv_obj_set_height(row, 58);
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

  // RSSI · SNR line (the bar is RSSI-based; SNR shown for link quality)
  lv_obj_t* info = lv_label_create(row);
  lv_label_set_text(info, r->info);
  lv_obj_set_style_text_font(info, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(info, lv_color_hex(MD_MUTED), 0);

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

static lv_obj_t* s_sig_list = NULL;
static int       s_sig_last = -1, s_sig_ticks = 0;

static void sig_fill(lv_obj_t* list) {
  int n = lvd_signal_count();
  s_sig_last = n;
  if (n <= 0) {
    lv_obj_t* h = lv_label_create(list);
    lv_label_set_text(h, "No repeaters heard recently");
    lv_obj_set_style_text_color(h, lv_color_hex(MD_MUTED), 0);
    return;
  }
  lvd_sig_t s;
  for (int i = 0; i < n; i++) if (lvd_signal_get(i, &s)) coverage_row(list, &s);
}
static void sig_tick(void) {
  if (!s_sig_list) return;
  int n = lvd_signal_count();
  if (n != s_sig_last || (++s_sig_ticks % 5) == 0) { lv_obj_clean(s_sig_list); sig_fill(s_sig_list); }
}

void lv_signal_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, "Signal");
  s_sig_list = full_list(scr);
  s_sig_ticks = 0;
  sig_fill(s_sig_list);
  lv_ui_set_refresh(sig_tick);
}

// ---- Heard: recent stations (Material cards, like the Android HeardRow) ------
static void heard_row(lv_obj_t* list, const lvd_heard_t* s, int idx) {
  lv_obj_t* row = lv_ui_md_card(list);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(row, 12, 0);
  lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(row, heard_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)idx);
  lv_ui_press_fx(row);

  // colour-graded signal dot: solid dot inside a translucent circle (Android style)
  uint32_t col = s->bars >= 3 ? 0x4ade80 : (s->bars == 2 ? 0xf59e0b : 0xfb7185);
  lv_obj_t* halo = lv_obj_create(row);
  lv_obj_remove_flag(halo, LV_OBJ_FLAG_SCROLLABLE); lv_obj_remove_flag(halo, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(halo, 34, 34); lv_obj_set_style_radius(halo, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(halo, lv_color_hex(col), 0); lv_obj_set_style_bg_opa(halo, 50, 0);
  lv_obj_set_style_border_width(halo, 0, 0); lv_obj_set_style_pad_all(halo, 0, 0);
  lv_obj_set_style_margin_right(halo, 12, 0);
  lv_obj_t* dot = lv_obj_create(halo);
  lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE); lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(dot, 12, 12); lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(dot, lv_color_hex(col), 0);
  lv_obj_set_style_border_width(dot, 0, 0); lv_obj_center(dot);

  lv_obj_t* mid = lv_obj_create(row);
  lv_obj_remove_flag(mid, LV_OBJ_FLAG_SCROLLABLE); lv_obj_remove_flag(mid, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(mid, 0, 0); lv_obj_set_style_border_width(mid, 0, 0);
  lv_obj_set_style_pad_all(mid, 0, 0); lv_obj_set_flex_grow(mid, 1); lv_obj_set_height(mid, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(mid, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(mid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_t* nm = lv_label_create(mid);
  lv_label_set_text(nm, s->name);
  lv_obj_set_style_text_font(nm, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(nm, lv_color_hex(MD_ON), 0);
  lv_obj_t* sg = lv_label_create(mid);
  lv_label_set_text(sg, s->meta);
  lv_obj_set_style_text_font(sg, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(sg, lv_color_hex(MD_MUTED), 0);

  lv_obj_t* rt = lv_obj_create(row);
  lv_obj_remove_flag(rt, LV_OBJ_FLAG_SCROLLABLE); lv_obj_remove_flag(rt, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(rt, 0, 0); lv_obj_set_style_border_width(rt, 0, 0);
  lv_obj_set_style_pad_all(rt, 0, 0); lv_obj_set_height(rt, LV_SIZE_CONTENT); lv_obj_set_width(rt, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(rt, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(rt, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
  lv_obj_t* ag = lv_label_create(rt);
  lv_label_set_text(ag, s->age);
  lv_obj_set_style_text_font(ag, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(ag, lv_color_hex(MD_MUTED), 0);
  if (s->dist[0]) {
    lv_obj_t* ds = lv_label_create(rt);
    lv_label_set_text(ds, s->dist);
    lv_obj_set_style_text_font(ds, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ds, lv_color_hex(MD_PRIMARY), 0);
  }
}

// live-refresh state for the Heard list (valid only while the screen is active)
static lv_obj_t* s_heard_list  = NULL;
static int       s_heard_last  = -1;
static int       s_heard_ticks = 0;

static void heard_fill(lv_obj_t* list) {
  int n = lvd_heard_count();
  s_heard_last = n;
  if (n <= 0) {
    lv_obj_t* hint = lv_label_create(list);
    lv_label_set_text(hint, "No stations heard yet");
    lv_obj_set_style_text_color(hint, lv_color_hex(MD_MUTED), 0);
    return;
  }
  lvd_heard_t s;
  for (int i = 0; i < n; i++) if (lvd_heard_get(i, &s)) heard_row(list, &s, i);
}

// Rebuild the list when a new station appears (count changed), and every ~5s to
// refresh the age labels. Cheap (the list is <=16 rows); scroll resets on rebuild.
static void heard_tick(void) {
  if (!s_heard_list) return;
  int n = lvd_heard_count();
  if (n != s_heard_last || (++s_heard_ticks % 5) == 0) {
    lv_obj_clean(s_heard_list);
    heard_fill(s_heard_list);
  }
}

void lv_heard_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, "Heard");
  s_heard_list  = lv_ui_md_scroll(scr);
  s_heard_ticks = 0;
  heard_fill(s_heard_list);
  lv_ui_set_refresh(heard_tick);
}
