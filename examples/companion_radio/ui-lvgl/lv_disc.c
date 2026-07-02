// LVGL Discover results: actively polls direct neighbours (zero-hop
// NODE_DISCOVER_REQ) on entry and every 60s. Responders are auto-added as
// contacts; tap a row to open its peer card (trace / telemetry / message).
#include "lv_ui.h"
#include "lv_data.h"
#include <stdio.h>

void lv_chat_set_peer(const char* name);  // peer-details target (lv_chat.c)

static lv_obj_t* s_list = NULL;
static int       s_last = -1;

static void disc_fill(lv_obj_t* list);   // fwd

// tap a responder to open its peer card (responders are already saved contacts)
static void disc_clicked(lv_event_t* e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  lvd_disc_t d;
  if (lvd_disc_get(i, &d)) { lv_chat_set_peer(d.name); if (lv_nav_cb) lv_nav_cb("peer"); }
}

static uint32_t type_color(int t) {
  return t == 2 ? UI_PURPLE : t == 3 ? UI_CYAN : t == 4 ? UI_ORANGE : UI_BLUE;
}
static const char* type_icon(int t) {
  return t == 2 ? ICON_REPEATERS : t == 3 ? ICON_CHAT : t == 4 ? ICON_NOISE : ICON_CONTACTS;
}

static void disc_row(lv_obj_t* list, const lvd_disc_t* d, int idx) {
  lv_obj_t* row = lv_ui_md_card(list);   // tap -> peer card (responders are contacts)
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(row, disc_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)idx);
  lv_ui_press_fx(row);

  // colour-graded signal dot (SNR bucket), matching the Heard screen
  uint32_t sc = d->bars >= 3 ? 0x4ade80 : (d->bars == 2 ? 0xf59e0b : 0xfb7185);
  lv_obj_t* dot = lv_obj_create(row);
  lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE); lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(dot, 10, 10); lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(dot, lv_color_hex(sc), 0); lv_obj_set_style_border_width(dot, 0, 0);
  lv_obj_set_style_margin_right(dot, 8, 0);

  lv_obj_t* ic = lv_label_create(row);
  lv_label_set_text(ic, type_icon(d->type));
  lv_obj_set_style_text_font(ic, &icons_fa, 0);
  lv_obj_set_style_text_color(ic, lv_color_hex(type_color(d->type)), 0);
  lv_obj_set_style_margin_right(ic, 12, 0);

  lv_obj_t* mid = lv_obj_create(row);
  lv_obj_remove_flag(mid, LV_OBJ_FLAG_SCROLLABLE); lv_obj_remove_flag(mid, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(mid, 0, 0); lv_obj_set_style_border_width(mid, 0, 0);
  lv_obj_set_style_pad_all(mid, 0, 0); lv_obj_set_flex_grow(mid, 1); lv_obj_set_height(mid, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(mid, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(mid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(mid, 2, 0);
  lv_obj_t* nm = lv_label_create(mid);
  // fresh (answered the current scan) responders get a "NEW" prefix
  if (d->fresh) lv_label_set_text_fmt(nm, "%s  " LV_SYMBOL_REFRESH, d->name);
  else          lv_label_set_text(nm, d->name);
  lv_obj_set_style_text_font(nm, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(nm, lv_color_hex(d->fresh ? 0x4ade80 : MD_ON), 0);
  lv_obj_t* sb = lv_label_create(mid);
  lv_label_set_text(sb, d->subtitle);
  lv_obj_set_style_text_font(sb, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(sb, lv_color_hex(MD_MUTED), 0);

  // right column: age, and distance+bearing when known
  lv_obj_t* rt = lv_obj_create(row);
  lv_obj_remove_flag(rt, LV_OBJ_FLAG_SCROLLABLE); lv_obj_remove_flag(rt, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(rt, 0, 0); lv_obj_set_style_border_width(rt, 0, 0);
  lv_obj_set_style_pad_all(rt, 0, 0); lv_obj_set_height(rt, LV_SIZE_CONTENT); lv_obj_set_width(rt, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(rt, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(rt, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
  lv_obj_t* ag = lv_label_create(rt);
  lv_label_set_text(ag, d->age);
  lv_obj_set_style_text_font(ag, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(ag, lv_color_hex(MD_MUTED), 0);
  if (d->dist[0]) {
    lv_obj_t* ds = lv_label_create(rt);
    lv_label_set_text(ds, d->dist);
    lv_obj_set_style_text_font(ds, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ds, lv_color_hex(MD_PRIMARY), 0);
  }
}

static int        s_secs = 0;   // ticks (~1s) since the last auto discovery request
static lv_obj_t*  s_status = NULL;  // header line: neighbour summary + next-scan countdown

static void disc_update_status(void) {
  if (!s_status) return;
  int secs = lvd_disc_next_secs();
  char b[80];
  if (secs > 0) snprintf(b, sizeof(b), "Next scan %ds  \xC2\xB7  %s", secs, lvd_disc_summary());
  else          snprintf(b, sizeof(b), "Scanning...  \xC2\xB7  %s", lvd_disc_summary());
  lv_label_set_text(s_status, b);
}
static void disc_scan_cb(lv_event_t* e) { (void)e; s_secs = 0; lvd_disc_request(); disc_update_status(); }

static void disc_fill(lv_obj_t* list) {
  int n = lvd_disc_count();
  s_last = n;
  if (n <= 0) {
    lv_obj_t* hint = lv_label_create(list);
    lv_label_set_text(hint, "Discovering nodes...\nNeighbours reply within a few seconds.");
    lv_obj_set_style_text_color(hint, lv_color_hex(MD_MUTED), 0);
    return;
  }
  lvd_disc_t d;
  for (int i = 0; i < n; i++) if (lvd_disc_get(i, &d)) disc_row(list, &d, i);
}

static void disc_tick(void) {
  if (++s_secs >= 60) { s_secs = 0; lvd_disc_request(); }   // re-poll the mesh every 60s
  if (s_list && lvd_disc_count() != s_last) { lv_obj_clean(s_list); disc_fill(s_list); }
  disc_update_status();   // refresh the countdown + summary each second
}

static void disc_clear_cb(lv_event_t* e) { (void)e; lvd_disc_clear(); if (s_list) { lv_obj_clean(s_list); disc_fill(s_list); } }

static void disc_btn(lv_obj_t* row, const char* txt, uint32_t color, lv_event_cb_t cb) {
  lv_obj_t* b = lv_ui_card(row, -1, 0, 0, 0);
  lv_obj_set_width(b, 84); lv_obj_set_height(b, 30); lv_obj_set_style_min_height(b, 0, 0);
  lv_obj_set_style_pad_all(b, 0, 0); lv_obj_set_style_radius(b, 15, 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(color), 0); lv_obj_set_style_bg_opa(b, 44, 0);
  lv_obj_set_style_border_color(b, lv_color_hex(color), 0); lv_obj_set_style_border_opa(b, 200, 0);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
  lv_ui_press_fx(b);
  lv_obj_t* l = lv_label_create(b);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(0xffffff), 0);
  lv_obj_center(l);
}

void lv_disc_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, "Discover");
  s_secs = 0;
  lvd_disc_request();              // actively poll neighbours on entry (not a passive list)

  // controls: discovery auto-repeats every 60s (paced -- repeaters ignore us if
  // we ask more often), so there's no manual trigger; just a status + Clear.
  lv_obj_t* ctl = lv_obj_create(scr);
  lv_obj_remove_flag(ctl, LV_OBJ_FLAG_SCROLLABLE); lv_obj_remove_flag(ctl, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_pos(ctl, 12, 40); lv_obj_set_size(ctl, 320 - 24, 30);
  lv_obj_set_style_bg_opa(ctl, 0, 0); lv_obj_set_style_border_width(ctl, 0, 0);
  lv_obj_set_style_pad_all(ctl, 0, 0);
  lv_obj_set_flex_flow(ctl, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(ctl, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  s_status = lv_label_create(ctl);
  lv_label_set_long_mode(s_status, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_font(s_status, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(s_status, lv_color_hex(MD_MUTED), 0);
  lv_obj_set_flex_grow(s_status, 1);
  disc_update_status();
  disc_btn(ctl, "Scan now", UI_BLUE, disc_scan_cb);
  disc_btn(ctl, "Clear", UI_RED, disc_clear_cb);

  // responders, below the controls
  s_list = lv_obj_create(scr);
  lv_obj_set_pos(s_list, 0, 78); lv_obj_set_size(s_list, 320, 240 - 78);
  lv_obj_set_style_bg_opa(s_list, 0, 0); lv_obj_set_style_border_width(s_list, 0, 0);
  lv_obj_set_style_pad_all(s_list, 12, 0);
  lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_row(s_list, 10, 0);
  disc_fill(s_list);
  lv_ui_set_refresh(disc_tick);
}
