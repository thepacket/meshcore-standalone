// LVGL Discover results: announces this node (zero-hop self-advert) on entry,
// then lists recently-heard nodes not yet saved as contacts. Tap a row to add it
// to contacts (it then moves to the Contacts screen). Bound to the firmware via
// the lvd_disc_* bridge (getHeardCandidates / addHeardContact).
#include "lv_ui.h"
#include "lv_data.h"
#include <stdio.h>

static lv_obj_t* s_list = NULL;
static int       s_last = -1;

static void disc_fill(lv_obj_t* list);   // fwd

static uint32_t type_color(int t) {
  return t == 2 ? UI_PURPLE : t == 3 ? UI_CYAN : t == 4 ? UI_ORANGE : UI_BLUE;
}
static const char* type_icon(int t) {
  return t == 2 ? ICON_REPEATERS : t == 3 ? ICON_CHAT : t == 4 ? ICON_NOISE : ICON_CONTACTS;
}

static void disc_row(lv_obj_t* list, const lvd_disc_t* d, int idx) {
  (void)idx;
  lv_obj_t* row = lv_ui_md_card(list);   // display-only: responders are auto-added as contacts
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t* ic = lv_label_create(row);
  lv_label_set_text(ic, type_icon(d->type));
  lv_obj_set_style_text_font(ic, &icons_fa, 0);
  lv_obj_set_style_text_color(ic, lv_color_hex(type_color(d->type)), 0);
  lv_obj_set_style_margin_right(ic, 14, 0);

  lv_obj_t* mid = lv_obj_create(row);
  lv_obj_remove_flag(mid, LV_OBJ_FLAG_SCROLLABLE); lv_obj_remove_flag(mid, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(mid, 0, 0); lv_obj_set_style_border_width(mid, 0, 0);
  lv_obj_set_style_pad_all(mid, 0, 0); lv_obj_set_flex_grow(mid, 1); lv_obj_set_height(mid, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(mid, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(mid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(mid, 2, 0);
  lv_obj_t* nm = lv_label_create(mid);
  lv_label_set_text(nm, d->name);
  lv_obj_set_style_text_font(nm, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(nm, lv_color_hex(MD_ON), 0);
  lv_obj_t* sb = lv_label_create(mid);
  lv_label_set_text(sb, d->subtitle);
  lv_obj_set_style_text_font(sb, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(sb, lv_color_hex(MD_MUTED), 0);
}

static int s_secs = 0;   // ticks (~1s) since the last auto discovery request

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
  lv_obj_t* cap = lv_label_create(ctl);
  lv_label_set_text(cap, "Auto-refresh every 60s");
  lv_obj_set_style_text_font(cap, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(cap, lv_color_hex(MD_MUTED), 0);
  lv_obj_set_flex_grow(cap, 1);
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
