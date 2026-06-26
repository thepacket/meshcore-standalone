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

static lv_obj_t* s_trace_list = NULL;
static unsigned  s_trace_seq = 0;

static void trace_note(lv_obj_t* list, const char* txt) {
  lv_obj_t* l = lv_label_create(list);
  lv_label_set_text(l, txt);
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(l, lv_pct(100));
  lv_obj_set_style_text_color(l, lv_color_hex(MD_MUTED), 0);
}

static void trace_fill(lv_obj_t* list) {
  s_trace_seq = lvd_trace_seq();
  int st = lvd_trace_state();
  const char* tgt = lvd_trace_target();
  if (st == 0) { trace_note(list, "Open a contact's details and tap Trace to map the route."); return; }
  if (st == 3) { char b[80]; snprintf(b, sizeof(b), "No known path to %s.\nTrace needs a direct (multi-hop) route.", tgt[0] ? tgt : "that node"); trace_note(list, b); return; }
  if (st == 1) { char b[48]; snprintf(b, sizeof(b), "Tracing %s ...", tgt); trace_note(list, b); return; }

  char hdr[64]; snprintf(hdr, sizeof(hdr), "%s  -  %d hop%s", tgt, lvd_trace_count() - 1,
                         (lvd_trace_count() - 1) == 1 ? "" : "s");
  lv_obj_t* h = lv_label_create(list);
  lv_label_set_text(h, hdr);
  lv_obj_set_style_text_color(h, lv_color_hex(UI_AMBER), 0);
  lv_obj_set_style_text_font(h, &lv_font_montserrat_16, 0);

  int n = lvd_trace_count();
  lvd_hop_t hop;
  for (int i = 0; i < n; i++)
    if (lvd_trace_get(i, &hop))
      hop_row(list, hop.left, hop.snr, hop.quality == 2 ? UI_GREEN : hop.quality == 1 ? UI_AMBER : UI_RED);
}

static void trace_tick(void) {
  if (s_trace_list && lvd_trace_seq() != s_trace_seq) { lv_obj_clean(s_trace_list); trace_fill(s_trace_list); }
}

void lv_trace_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, "Trace");
  s_trace_list = full_list(scr, 42);
  trace_fill(s_trace_list);
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
  lv_obj_t* kb = lv_keyboard_create(scr);
  lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
  lv_keyboard_set_textarea(kb, ta);
  lv_obj_set_size(kb, 320, 150);
  lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_event_cb(kb, pkt_search_ready, LV_EVENT_READY, NULL);
  lv_obj_add_event_cb(kb, pkt_search_cancel, LV_EVENT_CANCEL, NULL);
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
