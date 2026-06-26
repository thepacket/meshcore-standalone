// LVGL RF scope: a large live plot of the instantaneous channel RSSI with a
// manual scale slider, so you can zoom into the noise floor to see weak signals
// or zoom out to fit strong ones. Opened by tapping the home "RF" card.
#include "lv_ui.h"
#include "lv_data.h"
#include <stdio.h>

#define SC_POINTS 120            // ~12s of history at 100ms

static lv_obj_t*        s_sc_chart;
static lv_chart_series_t* s_sc_s;
static lv_obj_t*        s_sc_read;
static lv_obj_t*        s_sc_maxlbl;
static lv_timer_t*      s_sc_timer = NULL;
static int s_sc_min = 999, s_sc_max = -999;

static void sc_apply_range(void) {
  if (s_sc_chart) lv_chart_set_range(s_sc_chart, LV_CHART_AXIS_PRIMARY_Y, RF_MIN_DBM, lv_ui_rf_max());
}

static void sc_tick(lv_timer_t* t) {
  (void)t;
  if (!s_sc_chart) return;
  int rssi = lvd_rf_rssi();
  lv_chart_set_next_value(s_sc_chart, s_sc_s, rssi);
  if (rssi < s_sc_min) s_sc_min = rssi;
  if (rssi > s_sc_max) s_sc_max = rssi;
  if (s_sc_read) {
    char b[56];
    snprintf(b, sizeof(b), "%d dBm    min %d    max %d", rssi, s_sc_min, s_sc_max);
    lv_label_set_text(s_sc_read, b);
  }
}

static void sc_chart_deleted(lv_event_t* e) {
  (void)e;
  if (s_sc_timer) { lv_timer_del(s_sc_timer); s_sc_timer = NULL; }
  s_sc_chart = NULL;
}

// slider directly sets the max displayed RSSI (dBm); lower = zoom into weak
// signals near the floor, higher = fit strong signals.
static void sc_update_maxlbl(void) {
  if (s_sc_maxlbl) { char b[24]; snprintf(b, sizeof(b), "Max %d dBm", lv_ui_rf_max()); lv_label_set_text(s_sc_maxlbl, b); }
}
static void sc_slider_cb(lv_event_t* e) {
  lv_ui_rf_set_max(lv_slider_get_value((lv_obj_t*)lv_event_get_target(e)));
  sc_apply_range();
  sc_update_maxlbl();
}

void lv_rfscope_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, "RF scope");
  s_sc_min = 999; s_sc_max = -999;

  // the plot
  s_sc_chart = lv_chart_create(scr);
  lv_obj_set_size(s_sc_chart, 320 - 16, 150);
  lv_obj_set_pos(s_sc_chart, 8, 40);
  lv_obj_set_style_bg_color(s_sc_chart, lv_color_hex(0x101822), 0);
  lv_obj_set_style_bg_opa(s_sc_chart, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(s_sc_chart, 8, 0);
  lv_obj_set_style_border_width(s_sc_chart, 0, 0);
  lv_obj_set_style_line_color(s_sc_chart, lv_color_hex(0x223040), LV_PART_MAIN);  // grid
  lv_obj_set_style_size(s_sc_chart, 0, 0, LV_PART_INDICATOR);
  lv_chart_set_div_line_count(s_sc_chart, 5, 0);
  lv_chart_set_type(s_sc_chart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(s_sc_chart, SC_POINTS);
  s_sc_s = lv_chart_add_series(s_sc_chart, lv_color_hex(UI_GREEN), LV_CHART_AXIS_PRIMARY_Y);
  lv_obj_set_style_line_width(s_sc_chart, 2, LV_PART_ITEMS);
  sc_apply_range();
  lv_obj_add_event_cb(s_sc_chart, sc_chart_deleted, LV_EVENT_DELETE, NULL);

  // live readout
  s_sc_read = lv_label_create(scr);
  lv_label_set_text(s_sc_read, "-- dBm");
  lv_obj_set_style_text_font(s_sc_read, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_sc_read, lv_color_hex(UI_GREEN), 0);
  lv_obj_set_pos(s_sc_read, 10, 196);

  // max-RSSI slider with a live value label
  s_sc_maxlbl = lv_label_create(scr);
  lv_obj_set_style_text_font(s_sc_maxlbl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(s_sc_maxlbl, lv_color_hex(UI_MUTED), 0);
  lv_obj_align(s_sc_maxlbl, LV_ALIGN_BOTTOM_LEFT, 10, -8);
  sc_update_maxlbl();

  lv_obj_t* sl = lv_slider_create(scr);
  lv_obj_set_size(sl, 320 - 110, 10);
  lv_obj_align(sl, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
  lv_slider_set_range(sl, -120, -30);        // value == max displayed RSSI (dBm)
  lv_slider_set_value(sl, lv_ui_rf_max(), LV_ANIM_OFF);
  lv_obj_set_style_bg_color(sl, lv_color_hex(MD_PRIMARY), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(sl, lv_color_hex(MD_PRIMARY), LV_PART_KNOB);
  lv_obj_add_event_cb(sl, sc_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

  // sampler (faster than the home card for a finer scope)
  if (s_sc_timer) lv_timer_del(s_sc_timer);
  s_sc_timer = lv_timer_create(sc_tick, 100, NULL);
}
