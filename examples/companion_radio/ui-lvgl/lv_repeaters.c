// LVGL repeaters / room admin: Saved/Scan list (real repeater & room contacts),
// and a detail screen with remote login, status (parsed RepeaterStats) and a CLI
// console -- bound to the firmware via lvd_rep_* (uiLogin/uiRequestStatus/
// uiSendCommand + their async results).
#include "lv_ui.h"
#include "lv_data.h"
#include <stdio.h>
#include <stdint.h>

// ---- list -------------------------------------------------------------------
static int g_rep_tab = 0;   // 0 = Saved, 1 = Scan
void lv_repeaters_set_tab(int t) { g_rep_tab = (t == 1) ? 1 : 0; }

static lv_obj_t* s_list = NULL;
static unsigned  s_list_seq = 0;

static void rep_clicked(lv_event_t* e) {
  int packed = (int)(intptr_t)lv_event_get_user_data(e);
  lvd_rep_open(packed >> 16, packed & 0xffff);
  if (lv_nav_cb) lv_nav_cb("repeater_detail");
}

static void rep_row(lv_obj_t* list, int scan, int idx, const lvd_replist_t* n) {
  lv_obj_t* row = lv_ui_md_card(list);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(row, rep_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)((scan << 16) | idx));
  lv_ui_press_fx(row);

  lv_obj_t* ic = lv_label_create(row);
  lv_label_set_text(ic, ICON_REPEATERS);
  lv_obj_set_style_text_font(ic, &icons_fa, 0);
  lv_obj_set_style_text_color(ic, lv_color_hex(scan ? MD_PRIMARY : UI_PURPLE), 0);
  lv_obj_set_style_margin_right(ic, 14, 0);

  lv_obj_t* nm = lv_label_create(row);
  lv_label_set_text(nm, n->name);
  lv_obj_set_flex_grow(nm, 1);
  lv_obj_set_style_text_font(nm, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(nm, lv_color_hex(MD_ON), 0);

  lv_ui_pill(row, scan ? "+ ADD" : n->type, scan ? MD_PRIMARY : UI_PURPLE);
}

static void list_fill(lv_obj_t* list) {
  s_list_seq = lvd_rep_seq();
  int n = lvd_rep_count(g_rep_tab);
  if (n <= 0) {
    lv_obj_t* h = lv_label_create(list);
    lv_label_set_text(h, g_rep_tab ? "No new repeaters heard" : "No saved repeaters or rooms");
    lv_obj_set_style_text_color(h, lv_color_hex(MD_MUTED), 0);
    return;
  }
  lvd_replist_t r;
  for (int i = 0; i < n; i++) if (lvd_rep_get(g_rep_tab, i, &r)) rep_row(list, g_rep_tab, i, &r);
}
static void list_tick(void) {
  if (s_list && lvd_rep_seq() != s_list_seq) { lv_obj_clean(s_list); list_fill(s_list); }
}

static void seg_tab(lv_obj_t* parent, const char* txt, bool active, const char* dest) {
  lv_obj_t* t = lv_obj_create(parent);
  lv_obj_remove_flag(t, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_grow(t, 1); lv_obj_set_height(t, lv_pct(100));
  lv_obj_set_style_radius(t, 8, 0);
  lv_obj_set_style_bg_color(t, lv_color_hex(MD_PRIMARY), 0);
  lv_obj_set_style_bg_opa(t, active ? 200 : 0, 0);
  lv_obj_set_style_border_width(t, 0, 0); lv_obj_set_style_pad_all(t, 0, 0);
  if (!active && dest) lv_ui_clickable(t, dest);
  lv_obj_t* l = lv_label_create(t);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(active ? 0xffffff : UI_MUTED), 0);
  lv_obj_center(l);
}

void lv_repeaters_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, "Repeaters");

  lv_obj_t* seg = lv_obj_create(scr);
  lv_obj_remove_flag(seg, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(seg, 320 - 24, 28); lv_obj_set_pos(seg, 12, 42);
  lv_obj_set_style_radius(seg, 10, 0);
  lv_obj_set_style_bg_color(seg, lv_color_hex(MD_SURFACE), 0); lv_obj_set_style_bg_opa(seg, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(seg, 0, 0); lv_obj_set_style_pad_all(seg, 3, 0);
  lv_obj_set_flex_flow(seg, LV_FLEX_FLOW_ROW); lv_obj_set_style_pad_column(seg, 4, 0);
  seg_tab(seg, "Saved", g_rep_tab == 0, "repeaters");
  seg_tab(seg, "Scan",  g_rep_tab == 1, "scan");

  s_list = lv_obj_create(scr);
  lv_obj_set_pos(s_list, 0, 78); lv_obj_set_size(s_list, 320, 240 - 78);
  lv_obj_set_style_bg_opa(s_list, 0, 0); lv_obj_set_style_border_width(s_list, 0, 0);
  lv_obj_set_style_pad_hor(s_list, 12, 0); lv_obj_set_style_pad_ver(s_list, 2, 0);
  lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_row(s_list, 8, 0);
  list_fill(s_list);
  lv_ui_set_refresh(list_tick);
}

// ---- detail -----------------------------------------------------------------
static lv_obj_t* s_detail = NULL;
static unsigned  s_detail_seq = 0;

static void status_clicked(lv_event_t* e) { (void)e; lvd_rep_request_status(); }

static lv_obj_t* act_btn(lv_obj_t* row, const char* txt, uint32_t color) {
  lv_obj_t* b = lv_ui_card(row, -1, 0, 0, 0);
  lv_obj_set_flex_grow(b, 1); lv_obj_set_height(b, 32); lv_obj_set_style_min_height(b, 0, 0);
  lv_obj_set_style_pad_all(b, 0, 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(color), 0); lv_obj_set_style_bg_opa(b, 48, 0);
  lv_obj_set_style_border_color(b, lv_color_hex(color), 0); lv_obj_set_style_border_opa(b, 200, 0);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_ui_press_fx(b);
  lv_obj_t* l = lv_label_create(b);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(0xffffff), 0);
  lv_obj_center(l);
  return b;
}

static void stat_card(lv_obj_t* grid, const char* k, const char* v, uint32_t color) {
  lv_obj_t* c = lv_ui_md_card(grid);
  lv_obj_set_size(c, 99, 42); lv_obj_set_style_min_height(c, 0, 0); lv_obj_set_style_pad_all(c, 5, 0);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(c, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_t* vl = lv_label_create(c);
  lv_label_set_text(vl, v);
  lv_obj_set_style_text_font(vl, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(vl, lv_color_hex(color), 0);
  lv_obj_t* kl = lv_label_create(c);
  lv_label_set_text(kl, k);
  lv_obj_set_style_text_font(kl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(kl, lv_color_hex(UI_MUTED), 0);
}

static void detail_fill(lv_obj_t* list) {
  s_detail_seq = lvd_rep_seq();

  // login status
  int st = lvd_rep_login_state();
  const char* stxt = st == 2 ? "Logged in" : st == 1 ? "Logging in..." : st == 3 ? "Login failed" : "Not logged in";
  uint32_t scol = st == 2 ? UI_GREEN : st == 3 ? UI_RED : UI_MUTED;
  lv_obj_t* badge = lv_ui_pill(list, stxt, scol);
  (void)badge;

  // actions
  lv_obj_t* acts = lv_obj_create(list);
  lv_obj_remove_flag(acts, LV_OBJ_FLAG_SCROLLABLE); lv_obj_remove_flag(acts, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_width(acts, lv_pct(100)); lv_obj_set_height(acts, 34);
  lv_obj_set_style_bg_opa(acts, 0, 0); lv_obj_set_style_border_width(acts, 0, 0); lv_obj_set_style_pad_all(acts, 0, 0);
  lv_obj_set_flex_flow(acts, LV_FLEX_FLOW_ROW); lv_obj_set_style_pad_column(acts, 5, 0);
  lv_ui_clickable(act_btn(acts, "Login", UI_INDIGO), "rep_login");
  lv_obj_add_event_cb(act_btn(acts, "Status", UI_BLUE), status_clicked, LV_EVENT_CLICKED, NULL);
  lv_ui_clickable(act_btn(acts, "CLI", UI_AMBER), "rep_cli");

  // status stat cards
  lvd_repstat_t s; lvd_rep_status_get(&s);
  lv_obj_t* grid = lv_obj_create(list);
  lv_obj_remove_flag(grid, LV_OBJ_FLAG_SCROLLABLE); lv_obj_remove_flag(grid, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_width(grid, lv_pct(100)); lv_obj_set_height(grid, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(grid, 0, 0); lv_obj_set_style_border_width(grid, 0, 0); lv_obj_set_style_pad_all(grid, 0, 0);
  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_row(grid, 6, 0); lv_obj_set_style_pad_column(grid, 6, 0);
  stat_card(grid, "Battery",  s.have ? s.batt    : "--", UI_GREEN);
  stat_card(grid, "Uptime",   s.have ? s.uptime  : "--", UI_TEXT);
  stat_card(grid, "Recv",     s.have ? s.recv    : "--", UI_TEXT);
  stat_card(grid, "Sent",     s.have ? s.sent    : "--", UI_TEXT);
  stat_card(grid, "Air tx",   s.have ? s.airtime : "--", UI_TEXT);
  stat_card(grid, "Last SNR", s.have ? s.snr     : "--", UI_GREEN);
  if (!s.have) {
    lv_obj_t* h = lv_label_create(list);
    lv_label_set_text(h, "Tap Status to fetch live stats (login first if required).");
    lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP); lv_obj_set_width(h, lv_pct(100));
    lv_obj_set_style_text_font(h, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(h, lv_color_hex(MD_MUTED), 0);
  }

  // CLI console
  int cn = lvd_rep_cli_count();
  if (cn > 0) {
    lv_obj_t* con = lv_ui_md_card(list);
    lv_obj_set_width(con, lv_pct(100));
    lv_obj_set_flex_flow(con, LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_row(con, 2, 0);
    for (int i = 0; i < cn; i++) {
      lv_obj_t* l = lv_label_create(con);
      lv_label_set_text(l, lvd_rep_cli_line(i));
      lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP); lv_obj_set_width(l, lv_pct(100));
      lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
      const char* t = lvd_rep_cli_line(i);
      lv_obj_set_style_text_color(l, lv_color_hex(t[0] == '>' ? UI_AMBER : UI_TEXT), 0);
    }
  }
}
static void detail_tick(void) {
  if (s_detail && lvd_rep_seq() != s_detail_seq) { lv_obj_clean(s_detail); detail_fill(s_detail); }
}

void lv_repeater_detail_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, lvd_rep_name());
  s_detail = lv_ui_md_scroll(scr);
  lv_obj_set_style_pad_row(s_detail, 8, 0);
  detail_fill(s_detail);
  lv_ui_set_refresh(detail_tick);
}

// ---- keyboard screens: login password + CLI command ------------------------
static lv_obj_t* s_kb_ta = NULL;
static int       s_kb_mode = 0;   // 0 = login, 1 = cli

static void kb_ready(lv_event_t* e) {
  (void)e;
  if (s_kb_ta) {
    const char* txt = lv_textarea_get_text(s_kb_ta);
    if (s_kb_mode == 0) lvd_rep_login(txt);
    else if (txt && txt[0]) lvd_rep_send_cmd(txt);
  }
  if (lv_nav_cb) lv_nav_cb("back");
}
static void kb_cancel(lv_event_t* e) { (void)e; if (lv_nav_cb) lv_nav_cb("back"); }

static void kb_screen(lv_obj_t* scr, const char* title, const char* placeholder, bool password, int mode) {
  s_kb_mode = mode;
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, title);
  lv_obj_t* ta = lv_textarea_create(scr);
  lv_textarea_set_one_line(ta, true);
  lv_textarea_set_password_mode(ta, password);
  lv_textarea_set_placeholder_text(ta, placeholder);
  lv_obj_set_pos(ta, 8, 38); lv_obj_set_size(ta, 320 - 16, 34);
  lv_obj_set_style_bg_color(ta, lv_color_hex(0x18202c), 0);
  lv_obj_set_style_border_color(ta, lv_color_hex(MD_PRIMARY), 0);
  lv_obj_set_style_border_width(ta, 1, 0);
  lv_obj_set_style_text_color(ta, lv_color_hex(UI_TEXT), 0);
  s_kb_ta = ta;
  lv_obj_t* kb = lv_keyboard_create(scr);
  lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
  lv_keyboard_set_textarea(kb, ta);
  lv_obj_set_size(kb, 320, 150);
  lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_event_cb(kb, kb_ready, LV_EVENT_READY, NULL);
  lv_obj_add_event_cb(kb, kb_cancel, LV_EVENT_CANCEL, NULL);
}

void lv_rep_login_create(lv_obj_t* scr) { kb_screen(scr, "Login", "Admin password", true, 0); }
void lv_rep_cli_create(lv_obj_t* scr)   { kb_screen(scr, "CLI command", "e.g. status", false, 1); }
