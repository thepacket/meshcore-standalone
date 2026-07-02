// LVGL Stats dashboard: the device's live instrumentation on one scrollable
// screen (My telemetry, Noise floor with a rolling graph, Radio, Device and
// Packet counters), mirroring the Android client's Stats tab. Bound to the live
// firmware instrumentation via the lvd_stats_* bridge; updates in place ~1s.
#include "lv_ui.h"
#include "lv_data.h"
#include <stdio.h>
#include <stdlib.h>

// live widgets (valid only while the Stats screen is active)
static lv_obj_t* s_chart;  static lv_chart_series_t* s_series;
static lv_obj_t* s_bchart; static lv_chart_series_t* s_bseries;
static lv_obj_t* s_noise_cur, *s_noise_mm;
static lv_obj_t* s_rssi, *s_snr;
static lv_obj_t* s_tel_batt, *s_dev_batt, *s_uptime;
static lv_obj_t* s_recv, *s_sent, *s_errs, *s_loss, *s_radioerr;
static lv_obj_t* s_ram, *s_flash, *s_contacts, *s_channels;

static void fmt_batt_pct(char* b, int n, unsigned mv) {
  int pct = (int)((mv - 3300) * 100 / 900);   // 3.30V..4.20V
  if (pct < 0) pct = 0; else if (pct > 100) pct = 100;
  snprintf(b, n, "%d%% / %u.%02uv", pct, mv / 1000, (mv % 1000) / 10);
}
static void fmt_snr(char* b, int n, int snr_q) {
  int a = snr_q < 0 ? -snr_q : snr_q;
  snprintf(b, n, "%s%d.%d dB", snr_q < 0 ? "-" : "", a / 10, a % 10);
}
static void fmt_uptime(char* b, int n, unsigned s) {
  if (s >= 3600)   snprintf(b, n, "%uh %um", s / 3600, (s % 3600) / 60);
  else if (s >= 60) snprintf(b, n, "%um %us", s / 60, s % 60);
  else              snprintf(b, n, "%us", s);
}

static void stats_apply(const lvd_stats_t* st, int push_chart) {
  char b[40];
  snprintf(b, sizeof(b), "%d dBm", st->noise_floor);                       lv_label_set_text(s_noise_cur, b);
  snprintf(b, sizeof(b), "min %d   max %d dBm", st->noise_min, st->noise_max); lv_label_set_text(s_noise_mm, b);
  snprintf(b, sizeof(b), "%d dBm", st->last_rssi);                         lv_label_set_text(s_rssi, b);
  fmt_snr(b, sizeof(b), st->last_snr_q);                                   lv_label_set_text(s_snr, b);
  fmt_batt_pct(b, sizeof(b), st->batt_mv);                                 lv_label_set_text(s_tel_batt, b);
  snprintf(b, sizeof(b), "%u mV", st->batt_mv);                           lv_label_set_text(s_dev_batt, b);
  fmt_uptime(b, sizeof(b), st->uptime_secs);                              lv_label_set_text(s_uptime, b);
  snprintf(b, sizeof(b), "%u", st->pkt_recv);                            lv_label_set_text(s_recv, b);
  snprintf(b, sizeof(b), "%u", st->pkt_sent);                            lv_label_set_text(s_sent, b);
  snprintf(b, sizeof(b), "%u", st->pkt_recv_err);                        lv_label_set_text(s_errs, b);
  { unsigned den = st->pkt_recv + st->pkt_recv_err;                      // RX loss rate
    if (den) snprintf(b, sizeof(b), "%u.%u%%", st->pkt_recv_err * 100 / den, (st->pkt_recv_err * 1000 / den) % 10);
    else     snprintf(b, sizeof(b), "--");
    lv_label_set_text(s_loss, b); }
  lv_label_set_text(s_radioerr, lvd_stats_err_str());
  snprintf(b, sizeof(b), "%u KB", st->free_ram_kb);                      lv_label_set_text(s_ram, b);
  snprintf(b, sizeof(b), "%u / %u KB", st->flash_used_kb, st->flash_total_kb); lv_label_set_text(s_flash, b);
  snprintf(b, sizeof(b), "%d / %d", st->num_contacts, st->max_contacts); lv_label_set_text(s_contacts, b);
  snprintf(b, sizeof(b), "%d", st->num_channels);                        lv_label_set_text(s_channels, b);
  if (push_chart) {
    lv_chart_set_next_value(s_chart, s_series, st->noise_floor ? st->noise_floor : LV_CHART_POINT_NONE);
    lv_chart_set_next_value(s_bchart, s_bseries, st->batt_mv ? (int)st->batt_mv : LV_CHART_POINT_NONE);
  }
}

static void stats_tick(void) {
  if (!s_chart) return;
  lvd_stats_t st; lvd_stats_get(&st);
  stats_apply(&st, 1);
}

// Reset counters: zero packet counts + error flags, then refresh in place.
static void stats_reset_clicked(lv_event_t* e) {
  (void)e;
  lvd_stats_reset();
  lv_ui_toast("Counters reset");
  lvd_stats_t st; lvd_stats_get(&st);
  stats_apply(&st, 0);
}

void lv_stats_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, "Statistics");
  lv_obj_t* list = lv_ui_md_scroll(scr);

  // ---- My telemetry (this node's reported telemetry channels) ----
  lv_obj_t* tel = lv_ui_md_section(list, "My telemetry", 0);
  lv_obj_t* ch = lv_label_create(tel);
  lv_label_set_text(ch, "Channel 1");
  lv_obj_set_style_text_font(ch, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(ch, lv_color_hex(MD_PRIMARY), 0);
  s_tel_batt = lv_ui_md_row_v(tel, "Battery", "--", MD_ON);

  // ---- Noise floor: rolling graph + current/min/max ----
  lv_obj_t* ns = lv_ui_md_card(list);
  lv_obj_t* nh = lv_obj_create(ns);
  lv_obj_remove_flag(nh, LV_OBJ_FLAG_SCROLLABLE); lv_obj_remove_flag(nh, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_width(nh, lv_pct(100)); lv_obj_set_height(nh, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(nh, 0, 0); lv_obj_set_style_border_width(nh, 0, 0);
  lv_obj_set_style_pad_all(nh, 0, 0);
  lv_obj_set_flex_flow(nh, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(nh, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t* nt = lv_label_create(nh);
  lv_label_set_text(nt, "Noise floor");
  lv_obj_set_style_text_font(nt, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(nt, lv_color_hex(MD_ON), 0);
  s_noise_cur = lv_label_create(nh);
  lv_label_set_text(s_noise_cur, "-- dBm");
  lv_obj_set_style_text_font(s_noise_cur, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_noise_cur, lv_color_hex(MD_PRIMARY), 0);

  s_chart = lv_chart_create(ns);
  lv_obj_set_width(s_chart, lv_pct(100)); lv_obj_set_height(s_chart, 70);
  lv_obj_set_style_bg_opa(s_chart, 0, 0); lv_obj_set_style_border_width(s_chart, 0, 0);
  lv_obj_set_style_size(s_chart, 0, 0, LV_PART_INDICATOR);
  lv_chart_set_div_line_count(s_chart, 3, 0);
  lv_obj_set_style_line_color(s_chart, lv_color_hex(0x223046), LV_PART_MAIN);
  lv_chart_set_type(s_chart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(s_chart, 40);
  lv_chart_set_range(s_chart, LV_CHART_AXIS_PRIMARY_Y, -130, -40);
  s_series = lv_chart_add_series(s_chart, lv_color_hex(MD_PRIMARY), LV_CHART_AXIS_PRIMARY_Y);
  lv_obj_set_style_line_width(s_chart, 3, LV_PART_ITEMS);
  // seed from the recent history captured by the bridge
  { int hist[40]; int n = lvd_stats_noise_history(hist, 40);
    for (int i = 0; i < n; i++)
      lv_chart_set_next_value(s_chart, s_series, hist[i] ? hist[i] : LV_CHART_POINT_NONE); }

  s_noise_mm = lv_label_create(ns);
  lv_label_set_text(s_noise_mm, "min --   max -- dBm");
  lv_obj_set_style_text_font(s_noise_mm, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(s_noise_mm, lv_color_hex(MD_MUTED), 0);

  // ---- Battery: rolling mV trend ----
  lv_obj_t* bs = lv_ui_md_card(list);
  lv_obj_t* bt = lv_label_create(bs);
  lv_label_set_text(bt, "Battery trend");
  lv_obj_set_style_text_font(bt, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(bt, lv_color_hex(MD_ON), 0);
  s_bchart = lv_chart_create(bs);
  lv_obj_set_width(s_bchart, lv_pct(100)); lv_obj_set_height(s_bchart, 60);
  lv_obj_set_style_bg_opa(s_bchart, 0, 0); lv_obj_set_style_border_width(s_bchart, 0, 0);
  lv_obj_set_style_size(s_bchart, 0, 0, LV_PART_INDICATOR);
  lv_chart_set_div_line_count(s_bchart, 2, 0);
  lv_obj_set_style_line_color(s_bchart, lv_color_hex(0x223046), LV_PART_MAIN);
  lv_chart_set_type(s_bchart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(s_bchart, 40);
  lv_chart_set_range(s_bchart, LV_CHART_AXIS_PRIMARY_Y, 3200, 4250);   // typical Li-ion mV span
  s_bseries = lv_chart_add_series(s_bchart, lv_color_hex(UI_GREEN), LV_CHART_AXIS_PRIMARY_Y);
  lv_obj_set_style_line_width(s_bchart, 3, LV_PART_ITEMS);
  { int hist[40]; int n = lvd_stats_batt_history(hist, 40);
    for (int i = 0; i < n; i++)
      lv_chart_set_next_value(s_bchart, s_bseries, hist[i] ? hist[i] : LV_CHART_POINT_NONE); }

  // ---- Radio ----
  lv_obj_t* rad = lv_ui_md_section(list, "Radio", 0);
  s_rssi = lv_ui_md_row_v(rad, "Last RSSI", "--", MD_ON);
  s_snr  = lv_ui_md_row_v(rad, "Last SNR",  "--", MD_ON);

  // ---- Device (core) ----
  lv_obj_t* dev = lv_ui_md_section(list, "Device", 0);
  s_dev_batt = lv_ui_md_row_v(dev, "Battery", "--", MD_ON);
  s_uptime   = lv_ui_md_row_v(dev, "Uptime",  "--", MD_ON);

  // ---- Packets ----
  lv_obj_t* pk = lv_ui_md_section(list, "Packets", 0);
  s_recv = lv_ui_md_row_v(pk, "Received",    "--", MD_ON);
  s_sent = lv_ui_md_row_v(pk, "Sent",        "--", MD_ON);
  s_errs = lv_ui_md_row_v(pk, "Recv errors", "--", MD_ON);
  s_loss = lv_ui_md_row_v(pk, "Loss rate",   "--", MD_ON);
  s_radioerr = lv_ui_md_row_v(pk, "Radio errors", "--", MD_ON);
  // reset counters button
  lv_obj_t* rb = lv_ui_md_card(pk);
  lv_obj_set_height(rb, 36); lv_obj_set_style_min_height(rb, 0, 0);
  lv_obj_set_style_bg_color(rb, lv_color_hex(UI_RED), 0); lv_obj_set_style_bg_opa(rb, 40, 0);
  lv_obj_set_style_border_color(rb, lv_color_hex(UI_RED), 0); lv_obj_set_style_border_opa(rb, 200, 0);
  lv_obj_set_style_border_width(rb, 1, 0);
  lv_obj_add_flag(rb, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(rb, stats_reset_clicked, LV_EVENT_CLICKED, NULL);
  lv_ui_press_fx(rb);
  lv_obj_t* rl = lv_label_create(rb);
  lv_label_set_text(rl, LV_SYMBOL_REFRESH "  Reset counters");
  lv_obj_set_style_text_font(rl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(rl, lv_color_hex(0xffffff), 0);
  lv_obj_center(rl);

  // ---- Memory ----
  lv_obj_t* mem = lv_ui_md_section(list, "Memory", 0);
  s_ram   = lv_ui_md_row_v(mem, "RAM free",   "--", MD_ON);
  s_flash = lv_ui_md_row_v(mem, "Flash used", "--", MD_ON);

  // ---- Mesh ----
  lv_obj_t* msh = lv_ui_md_section(list, "Mesh", 0);
  s_contacts = lv_ui_md_row_v(msh, "Contacts", "--", MD_ON);
  s_channels = lv_ui_md_row_v(msh, "Channels", "--", MD_ON);

  lvd_stats_t st; lvd_stats_get(&st);
  stats_apply(&st, 0);   // chart already seeded above
  lv_ui_set_refresh(stats_tick);
}
