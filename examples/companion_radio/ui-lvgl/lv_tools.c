// LVGL tools: Trace route, the live packet monitor, and a per-packet detail view.
#include "lv_ui.h"
#include "lv_data.h"
#include <stdio.h>
#include <string.h>

static lv_obj_t* full_list(lv_obj_t* scr, int top) {
  lv_obj_t* list = lv_obj_create(scr);
  lv_obj_set_pos(list, 4, top);
  lv_obj_set_size(list, 320 - 8, 240 - top - 4);
  lv_obj_set_style_bg_opa(list, 0, 0); lv_obj_set_style_border_width(list, 0, 0);
  lv_obj_set_style_pad_all(list, 2, 0);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(list, 5, 0);
  return list;
}

// ============================ Trace route ============================
static void hop_row(lv_obj_t* list, const char* left, const char* snr, uint32_t col) {
  lv_obj_t* c = lv_ui_md_card(list);
  lv_obj_set_width(c, lv_pct(100)); lv_obj_set_height(c, 36);
  lv_obj_set_style_min_height(c, 0, 0); lv_obj_set_style_pad_hor(c, 12, 0); lv_obj_set_style_pad_ver(c, 0, 0);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(c, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t* l = lv_label_create(c);
  lv_label_set_text(l, left);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(UI_TEXT), 0);
  lv_obj_t* p = lv_ui_pill(c, snr, col);
  (void)p;
}

static unsigned s_trace_seq = 0;
static void trace_build(lv_obj_t* scr);   // fwd

static void trace_note(lv_obj_t* list, const char* txt) {
  lv_obj_t* l = lv_label_create(list);
  lv_label_set_text(l, txt);
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(l, lv_pct(100));
  lv_obj_set_style_text_color(l, lv_color_hex(MD_MUTED), 0);
}

// rebuild the whole screen safely after a tap (deferred so we don't free the
// tapped object mid-event)
static void trace_rebuild_cb(void* p) { (void)p; lv_obj_t* s = lv_screen_active(); lv_obj_clean(s); trace_build(s); }
static void trace_rebuild(void) { lv_async_call(trace_rebuild_cb, NULL); }

static void tr_add_clicked(lv_event_t* e)  { lvd_trace_path_add((int)(intptr_t)lv_event_get_user_data(e)); trace_rebuild(); }
static void tr_send_clicked(lv_event_t* e) { (void)e; lvd_trace_go();        trace_rebuild(); }
static void tr_clear_clicked(lv_event_t* e){ (void)e; lvd_trace_path_clear(); trace_rebuild(); }

// repeater-name filter for the builder list (this screen only)
static char s_tr_repfilter[24] = "";
static void tr_repsearch_open(lv_event_t* e) { (void)e; if (lv_nav_cb) lv_nav_cb("tr_rep_search"); }
static void tr_repfilter_clear(lv_event_t* e) { (void)e; s_tr_repfilter[0] = 0; trace_rebuild(); }

static lv_obj_t* tr_btn(lv_obj_t* parent, const char* txt, uint32_t color) {
  lv_obj_t* b = lv_ui_card(parent, -1, 0, 0, 0);
  lv_obj_set_height(b, 34); lv_obj_set_flex_grow(b, 1); lv_obj_set_style_min_height(b, 0, 0);
  lv_obj_set_style_pad_all(b, 0, 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(color), 0); lv_obj_set_style_bg_opa(b, 48, 0);
  lv_obj_set_style_border_color(b, lv_color_hex(color), 0); lv_obj_set_style_border_opa(b, 200, 0);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_ui_press_fx(b);
  lv_obj_t* l = lv_label_create(b);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(0xffffff), 0);
  lv_obj_center(l);
  return b;
}

// a tappable repeater row in the builder (name + "+")
static void tr_rep_row(lv_obj_t* list, int i, const char* name) {
  lv_obj_t* c = lv_ui_md_card(list);
  lv_obj_set_width(c, lv_pct(100)); lv_obj_set_height(c, 40); lv_obj_set_style_min_height(c, 0, 0);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(c, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(c, tr_add_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)i);
  lv_ui_press_fx(c);
  lv_obj_t* nm = lv_label_create(c);
  lv_label_set_text(nm, name);
  lv_obj_set_style_text_font(nm, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(nm, lv_color_hex(MD_ON), 0);
  lv_obj_t* pl = lv_label_create(c);
  lv_label_set_text(pl, LV_SYMBOL_PLUS);
  lv_obj_set_style_text_color(pl, lv_color_hex(MD_PRIMARY), 0);
}

static void trace_build(lv_obj_t* scr) {
  s_trace_seq = lvd_trace_seq();
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, "Trace");
  lv_obj_t* list = full_list(scr, 42);
  int st = lvd_trace_state();

  if (st == 1) {   // tracing in progress
    char b[100]; snprintf(b, sizeof(b), "Tracing %s ...", lvd_trace_path_str());
    trace_note(list, b);
    return;
  }
  if (st == 2) {   // result
    lv_obj_t* h = lv_label_create(list);
    lv_label_set_text(h, lvd_trace_target());
    lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP); lv_obj_set_width(h, lv_pct(100));
    lv_obj_set_style_text_color(h, lv_color_hex(UI_AMBER), 0);
    lv_obj_set_style_text_font(h, &lv_font_montserrat_16, 0);
    int n = lvd_trace_count();
    lvd_hop_t hop;
    for (int i = 0; i < n; i++)
      if (lvd_trace_get(i, &hop))
        hop_row(list, hop.left, hop.snr, hop.quality == 2 ? UI_GREEN : hop.quality == 1 ? UI_AMBER : UI_RED);
    lv_obj_add_event_cb(tr_btn(list, "New trace", UI_BLUE), tr_clear_clicked, LV_EVENT_CLICKED, NULL);
    return;
  }

  // build mode (st 0, or 3 = send failed, or 4 = timed out)
  if (st == 3) trace_note(list, "Trace send failed (busy or path too long).");
  if (st == 4) trace_note(list, "Trace timed out (no response in 10s). Tap Send to retry.");

  lv_obj_t* pc = lv_ui_md_card(list);
  lv_obj_set_flex_flow(pc, LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_row(pc, 8, 0);
  int plen = lvd_trace_path_len();
  lv_obj_t* pl = lv_label_create(pc);
  if (plen == 0) lv_label_set_text(pl, "Build a path: tap repeaters below");
  else { char b[100]; snprintf(b, sizeof(b), "Path: %s", lvd_trace_path_str()); lv_label_set_text(pl, b); }
  lv_label_set_long_mode(pl, LV_LABEL_LONG_WRAP); lv_obj_set_width(pl, lv_pct(100));
  lv_obj_set_style_text_color(pl, lv_color_hex(plen ? MD_ON : MD_MUTED), 0);
  if (plen > 0) {
    lv_obj_t* br = lv_obj_create(pc);
    lv_obj_remove_flag(br, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(br, lv_pct(100)); lv_obj_set_height(br, 34);
    lv_obj_set_style_bg_opa(br, 0, 0); lv_obj_set_style_border_width(br, 0, 0); lv_obj_set_style_pad_all(br, 0, 0);
    lv_obj_set_flex_flow(br, LV_FLEX_FLOW_ROW); lv_obj_set_style_pad_column(br, 6, 0);
    lv_obj_add_event_cb(tr_btn(br, "Send", UI_GREEN), tr_send_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(tr_btn(br, "Clear", UI_RED), tr_clear_clicked, LV_EVENT_CLICKED, NULL);
  }

  int rn = lvd_rep_count(0);
  if (rn == 0) { trace_note(list, "No repeaters or rooms saved to build a path."); return; }

  // search field to filter the repeater list
  bool fa = s_tr_repfilter[0] != 0;
  lv_obj_t* sf = lv_ui_md_card(list);
  lv_obj_set_flex_flow(sf, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(sf, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_add_flag(sf, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(sf, tr_repsearch_open, LV_EVENT_CLICKED, NULL);
  lv_ui_press_fx(sf);
  lv_obj_t* si = lv_label_create(sf);
  lv_label_set_text(si, ICON_FINDER);
  lv_obj_set_style_text_font(si, &icons_fa, 0);
  lv_obj_set_style_text_color(si, lv_color_hex(MD_MUTED), 0);
  lv_obj_set_style_margin_right(si, 10, 0);
  lv_obj_t* sl = lv_label_create(sf);
  lv_label_set_text(sl, fa ? s_tr_repfilter : "Search repeaters");
  lv_obj_set_style_text_color(sl, lv_color_hex(fa ? MD_ON : MD_MUTED), 0);
  lv_obj_set_flex_grow(sl, 1);
  if (fa) {
    lv_obj_t* cl = lv_label_create(sf);
    lv_label_set_text(cl, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(cl, lv_color_hex(MD_MUTED), 0);
    lv_obj_add_flag(cl, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(cl, 10);
    lv_obj_add_event_cb(cl, tr_repfilter_clear, LV_EVENT_CLICKED, NULL);
    lv_ui_press_fx(cl);
  }

  lvd_replist_t r;
  int shown = 0;
  for (int i = 0; i < rn; i++) {
    if (!lvd_rep_get(0, i, &r)) continue;
    if (fa && !lvd_name_match(r.name, s_tr_repfilter)) continue;
    tr_rep_row(list, i, r.name);   // i = original index (for lvd_trace_path_add)
    shown++;
  }
  if (shown == 0) trace_note(list, "No repeaters match");
}

// repeater-search keyboard screen for the trace builder
static lv_obj_t* s_tr_search_ta = NULL;
static void tr_search_ready(lv_event_t* e) {
  (void)e;
  if (s_tr_search_ta) {
    const char* t = lv_textarea_get_text(s_tr_search_ta);
    strncpy(s_tr_repfilter, t ? t : "", sizeof(s_tr_repfilter) - 1);
    s_tr_repfilter[sizeof(s_tr_repfilter) - 1] = 0;
  }
  if (lv_nav_cb) lv_nav_cb("back");
}
static void tr_search_cancel(lv_event_t* e) { (void)e; if (lv_nav_cb) lv_nav_cb("back"); }

void lv_trace_rep_search_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, "Search repeaters");
  lv_obj_t* ta = lv_textarea_create(scr);
  lv_textarea_set_one_line(ta, true);
  lv_textarea_set_text(ta, s_tr_repfilter);
  lv_textarea_set_placeholder_text(ta, "Repeater name");
  lv_obj_set_pos(ta, 8, 38); lv_obj_set_size(ta, 320 - 16, 34);
  lv_obj_set_style_bg_color(ta, lv_color_hex(0x18202c), 0);
  lv_obj_set_style_border_color(ta, lv_color_hex(MD_PRIMARY), 0);
  lv_obj_set_style_border_width(ta, 1, 0);
  lv_obj_set_style_text_color(ta, lv_color_hex(UI_TEXT), 0);
  s_tr_search_ta = ta;
  lv_ui_kbd_focus(ta);   // route the physical keyboard into this field
  lv_obj_add_event_cb(ta, tr_search_ready, LV_EVENT_READY, NULL);   // physical Enter submits

  if (lvd_osk_enabled()) {
    lv_obj_t* kb = lv_keyboard_create(scr);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_set_size(kb, 320, 150);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(kb, tr_search_ready, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(kb, tr_search_cancel, LV_EVENT_CANCEL, NULL);
  }
}

static void trace_tick(void) {
  lvd_trace_poll();   // time out a stuck trace
  if (lvd_trace_seq() != s_trace_seq) { lv_obj_t* s = lv_screen_active(); lv_obj_clean(s); trace_build(s); }
}

void lv_trace_create(lv_obj_t* scr) {
  trace_build(scr);
  lv_ui_set_refresh(trace_tick);
}

// ============================ Packet monitor ============================
// The companion firmware isn't CLI-driven, so the old local "Console" tab had no
// backing and was dropped -- this screen is the live packet monitor only.
void lv_terminal_set_tab(int t) { (void)t; }   // retained for the nav signature

static void pkt_clicked(lv_event_t* e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  lvd_packet_select(i);
  if (lv_nav_cb) lv_nav_cb("pktdetail");
}

static void pkt_row(lv_obj_t* list, int idx, const char* type, uint32_t tcol, const char* meta) {
  lv_obj_t* c = lv_ui_md_card(list);
  lv_obj_set_width(c, lv_pct(100)); lv_obj_set_height(c, 32);
  lv_obj_set_style_min_height(c, 0, 0); lv_obj_set_style_pad_hor(c, 8, 0); lv_obj_set_style_pad_ver(c, 0, 0);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(c, pkt_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)idx);
  lv_ui_press_fx(c);
  lv_obj_t* tag = lv_ui_pill(c, type, tcol);
  lv_obj_set_style_margin_right(tag, 8, 0);
  lv_obj_t* m = lv_label_create(c);
  lv_label_set_text(m, meta);
  lv_obj_set_style_text_font(m, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(m, lv_color_hex(UI_MUTED), 0);
}

// live packet-monitor list (valid only while the Packets tab is active)
static lv_obj_t* s_pkt_list = NULL;
static unsigned  s_pkt_last = 0;

static void packets_fill(lv_obj_t* list) {
  int n = lvd_packet_count();
  s_pkt_last = lvd_packet_total();
  if (n <= 0) {
    const char* f = lvd_packet_path_filter();
    lv_obj_t* hint = lv_label_create(list);
    lv_label_set_text(hint, (f && f[0]) ? "No packets match that path" : "No packets yet");
    lv_obj_set_style_text_color(hint, lv_color_hex(MD_MUTED), 0);
    return;
  }
  lvd_packet_t p;
  for (int i = 0; i < n; i++)
    if (lvd_packet_get(i, &p)) pkt_row(list, i, p.type, p.color, p.meta);
}

// rebuild when a new packet arrives (monotonic total changed)
static void packets_tick(void) {
  if (s_pkt_list && lvd_packet_total() != s_pkt_last) {
    lv_obj_clean(s_pkt_list);
    packets_fill(s_pkt_list);
  }
}

static void pkt_open_search(lv_event_t* e) { (void)e; if (lv_nav_cb) lv_nav_cb("pkt_search"); }
static void pkt_clear_search_cb(void* p) {
  (void)p; lvd_packet_set_path_filter("");
  if (s_pkt_list) { lv_obj_clean(s_pkt_list); packets_fill(s_pkt_list); }
}
static void pkt_clear_search(lv_event_t* e) { (void)e; lv_async_call(pkt_clear_search_cb, NULL); }

void lv_terminal_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, "Packets");

  // search-by-path field
  const char* f = lvd_packet_path_filter();
  bool active = f && f[0];
  lv_obj_t* sf = lv_ui_card(scr, 4, 40, 320 - 8, 30);
  lv_obj_set_style_pad_hor(sf, 10, 0); lv_obj_set_style_pad_ver(sf, 0, 0);
  lv_obj_set_flex_flow(sf, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(sf, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_add_flag(sf, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(sf, pkt_open_search, LV_EVENT_CLICKED, NULL);
  lv_ui_press_fx(sf);
  lv_obj_t* ic = lv_label_create(sf);
  lv_label_set_text(ic, ICON_FINDER);
  lv_obj_set_style_text_font(ic, &icons_fa, 0);
  lv_obj_set_style_text_color(ic, lv_color_hex(MD_MUTED), 0);
  lv_obj_set_style_margin_right(ic, 10, 0);
  lv_obj_t* st = lv_label_create(sf);
  lv_label_set_text(st, active ? f : "Search path");
  lv_obj_set_style_text_color(st, lv_color_hex(active ? MD_ON : MD_MUTED), 0);
  lv_obj_set_flex_grow(st, 1);
  if (active) {
    lv_obj_t* cl = lv_label_create(sf);
    lv_label_set_text(cl, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(cl, lv_color_hex(MD_MUTED), 0);
    lv_obj_add_flag(cl, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(cl, 10);
    lv_obj_add_event_cb(cl, pkt_clear_search, LV_EVENT_CLICKED, NULL);
    lv_ui_press_fx(cl);
  }

  s_pkt_list = full_list(scr, 74);
  packets_fill(s_pkt_list);
  lv_ui_set_refresh(packets_tick);
}

// path-search keyboard screen
static lv_obj_t* s_pkt_search_ta = NULL;
static void pkt_search_ready(lv_event_t* e) {
  (void)e;
  if (s_pkt_search_ta) lvd_packet_set_path_filter(lv_textarea_get_text(s_pkt_search_ta));
  if (lv_nav_cb) lv_nav_cb("back");
}
static void pkt_search_cancel(lv_event_t* e) { (void)e; if (lv_nav_cb) lv_nav_cb("back"); }

void lv_pkt_search_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, "Search path");
  lv_obj_t* ta = lv_textarea_create(scr);
  lv_textarea_set_one_line(ta, true);
  lv_textarea_set_text(ta, lvd_packet_path_filter());
  lv_textarea_set_placeholder_text(ta, "Node name in path");
  lv_obj_set_pos(ta, 8, 38); lv_obj_set_size(ta, 320 - 16, 34);
  lv_obj_set_style_bg_color(ta, lv_color_hex(0x18202c), 0);
  lv_obj_set_style_border_color(ta, lv_color_hex(MD_PRIMARY), 0);
  lv_obj_set_style_border_width(ta, 1, 0);
  lv_obj_set_style_text_color(ta, lv_color_hex(UI_TEXT), 0);
  s_pkt_search_ta = ta;
  lv_ui_kbd_focus(ta);   // route the physical keyboard into this field
  lv_obj_add_event_cb(ta, pkt_search_ready, LV_EVENT_READY, NULL);   // physical Enter submits

  if (lvd_osk_enabled()) {
    lv_obj_t* kb = lv_keyboard_create(scr);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_set_size(kb, 320, 150);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(kb, pkt_search_ready, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(kb, pkt_search_cancel, LV_EVENT_CANCEL, NULL);
  }
}

// ---- packet detail (full breakdown + raw hex, like the Android dialog) ------
void lv_pkt_detail_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, "Packet");
  lv_obj_t* list = lv_ui_md_scroll(scr);

  lvd_kv_t kv[20];
  int n = lvd_packet_detail(kv, 20);
  if (n <= 0) {
    lv_obj_t* h = lv_label_create(list);
    lv_label_set_text(h, "No packet selected");
    lv_obj_set_style_text_color(h, lv_color_hex(MD_MUTED), 0);
    return;
  }
  lv_obj_t* card = lv_ui_md_card(list);
  for (int i = 0; i < n; i++) {
    if (strcmp(kv[i].label, "Path") == 0) {
      // path can be a long chain of names -- label + value wrapped to its own lines
      lv_obj_t* lab = lv_label_create(card);
      lv_label_set_text(lab, "Path");
      lv_obj_set_style_text_font(lab, &lv_font_montserrat_14, 0);
      lv_obj_set_style_text_color(lab, lv_color_hex(MD_MUTED), 0);
      lv_obj_t* val = lv_label_create(card);
      lv_label_set_text(val, kv[i].value);
      lv_label_set_long_mode(val, LV_LABEL_LONG_WRAP);
      lv_obj_set_width(val, lv_pct(100));
      lv_obj_set_style_text_font(val, &lv_font_montserrat_14, 0);
      lv_obj_set_style_text_color(val, lv_color_hex(MD_ON), 0);
    } else {
      lv_ui_md_row(card, kv[i].label, kv[i].value, MD_ON);
    }
  }

  lv_obj_t* raw = lv_ui_md_section(list, "Raw", 0);
  lv_obj_t* hx = lv_label_create(raw);
  lv_label_set_text(hx, lvd_packet_hex());
  lv_label_set_long_mode(hx, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(hx, lv_pct(100));
  lv_obj_set_style_text_font(hx, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(hx, lv_color_hex(UI_TEXT), 0);
}
