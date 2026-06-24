// LVGL Stats dashboard: consolidates the device's live instrumentation into one
// scrollable screen, mirroring the Android client's Stats tab — My Telemetry
// (Cayenne-LPP sensor readings), Noise floor (rolling graph), Radio, Device and
// Packet counters — using the shared Material panel kit (solid cards, titleMedium
// headers, label/value rows). Prototype values; the firmware build binds these
// rows to MyMesh getStats()/telemetry callbacks.
#include "lv_ui.h"
#include <stdio.h>

void lv_stats_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, "Statistics");
  lv_obj_t* list = lv_ui_md_scroll(scr);

  // ---- My Telemetry (Cayenne-LPP readings reported by this node) ----
  lv_obj_t* tel = lv_ui_md_section(list, "My telemetry", 0);
  lv_obj_t* ch = lv_label_create(tel);   // channel sub-header, like Android
  lv_label_set_text(ch, "Channel 1");
  lv_obj_set_style_text_font(ch, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(ch, lv_color_hex(MD_PRIMARY), 0);
  lv_ui_md_row(tel, "Battery",            "92% / 4.05v", MD_ON);
  lv_ui_md_row(tel, "Temperature",        "21.4\xC2\xB0""C / 70.5\xC2\xB0""F", MD_ON);
  lv_ui_md_row(tel, "Relative humidity",  "48%", MD_ON);
  lv_ui_md_row(tel, "Barometric pressure","1013 hPa", MD_ON);
  lv_ui_md_row(tel, "Altitude",           "78m / 256ft", MD_ON);

  // ---- Noise floor: rolling graph + current/min/max ----
  lv_obj_t* ns = lv_ui_md_card(list);
  lv_obj_t* nh = lv_obj_create(ns);   // header row: title left, current dBm right
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
  lv_obj_t* nv = lv_label_create(nh);
  lv_label_set_text(nv, "-104 dBm");
  lv_obj_set_style_text_font(nv, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(nv, lv_color_hex(MD_PRIMARY), 0);

  lv_obj_t* chart = lv_chart_create(ns);
  lv_obj_set_width(chart, lv_pct(100)); lv_obj_set_height(chart, 70);
  lv_obj_set_style_bg_opa(chart, 0, 0); lv_obj_set_style_border_width(chart, 0, 0);
  lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR);
  lv_chart_set_div_line_count(chart, 3, 0);
  lv_obj_set_style_line_color(chart, lv_color_hex(0x223046), LV_PART_MAIN);
  lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(chart, 40);
  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, -130, -40);
  lv_chart_series_t* s = lv_chart_add_series(chart, lv_color_hex(MD_PRIMARY), LV_CHART_AXIS_PRIMARY_Y);
  for (int i = 0; i < 40; i++)
    lv_chart_set_next_value(chart, s, -104 + (int)(8 * lv_trigo_sin(i * 700) / 32767) + (i * 11 % 6) - 3);
  lv_obj_set_style_line_width(chart, 3, LV_PART_ITEMS);
  lv_obj_t* nm = lv_label_create(ns);
  lv_label_set_text(nm, "min -118   max -92 dBm  (last 40)");
  lv_obj_set_style_text_font(nm, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(nm, lv_color_hex(MD_MUTED), 0);

  // ---- Radio ----
  lv_obj_t* rad = lv_ui_md_section(list, "Radio", 0);
  lv_ui_md_row(rad, "Last RSSI",  "-78 dBm", MD_ON);
  lv_ui_md_row(rad, "Last SNR",   "9.0 dB", MD_ON);
  lv_ui_md_row(rad, "TX airtime", "12s", MD_ON);
  lv_ui_md_row(rad, "RX airtime", "3m 4s", MD_ON);

  // ---- Device (core) ----
  lv_obj_t* dev = lv_ui_md_section(list, "Device", 0);
  lv_ui_md_row(dev, "Battery",  "4050 mV", MD_ON);
  lv_ui_md_row(dev, "Uptime",   "3h 12m", MD_ON);
  lv_ui_md_row(dev, "TX queue", "0", MD_ON);

  // ---- Packets ----
  lv_obj_t* pk = lv_ui_md_section(list, "Packets", 0);
  lv_ui_md_row(pk, "Received",            "1942", MD_ON);
  lv_ui_md_row(pk, "Sent",                "238", MD_ON);
  lv_ui_md_row(pk, "Recv flood / direct", "1503 / 439", MD_ON);
  lv_ui_md_row(pk, "Sent flood / direct", "180 / 58", MD_ON);
  lv_ui_md_row(pk, "Recv errors",         "7", MD_ON);
}
