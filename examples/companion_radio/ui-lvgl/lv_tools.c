// LVGL tools: Trace route, and Terminal (Console + Packets tabs / packet monitor).
#include "lv_ui.h"
#include "lv_data.h"
#include <stdio.h>

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

// ============================ Terminal ============================
static int g_term_tab = 0;   // 0 = Console, 1 = Packets
void lv_terminal_set_tab(int t) { g_term_tab = (t == 1) ? 1 : 0; }

static void term_tab(lv_obj_t* seg, const char* txt, bool active, const char* dest) {
  lv_obj_t* t = lv_obj_create(seg);
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

static void log_line(lv_obj_t* list, const char* txt, uint32_t color) {
  lv_obj_t* l = lv_label_create(list);
  lv_label_set_text(l, txt);
  lv_obj_set_width(l, lv_pct(100));
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
}

static void pkt_row(lv_obj_t* list, const char* type, uint32_t tcol, const char* meta) {
  lv_obj_t* c = lv_ui_md_card(list);
  lv_obj_set_width(c, lv_pct(100)); lv_obj_set_height(c, 32);
  lv_obj_set_style_min_height(c, 0, 0); lv_obj_set_style_pad_hor(c, 8, 0); lv_obj_set_style_pad_ver(c, 0, 0);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
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
    lv_obj_t* hint = lv_label_create(list);
    lv_label_set_text(hint, "No packets yet");
    lv_obj_set_style_text_color(hint, lv_color_hex(MD_MUTED), 0);
    return;
  }
  lvd_packet_t p;
  for (int i = 0; i < n; i++)
    if (lvd_packet_get(i, &p)) pkt_row(list, p.type, p.color, p.meta);
}

// rebuild when a new packet arrives (monotonic total changed)
static void packets_tick(void) {
  if (s_pkt_list && lvd_packet_total() != s_pkt_last) {
    lv_obj_clean(s_pkt_list);
    packets_fill(s_pkt_list);
  }
}

void lv_terminal_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, "Terminal");

  lv_obj_t* seg = lv_obj_create(scr);
  lv_obj_remove_flag(seg, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(seg, 320 - 24, 28); lv_obj_set_pos(seg, 12, 42);
  lv_obj_set_style_radius(seg, 10, 0);
  lv_obj_set_style_bg_color(seg, lv_color_hex(MD_SURFACE), 0); lv_obj_set_style_bg_opa(seg, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(seg, 0, 0); lv_obj_set_style_pad_all(seg, 3, 0);
  lv_obj_set_flex_flow(seg, LV_FLEX_FLOW_ROW); lv_obj_set_style_pad_column(seg, 4, 0);
  term_tab(seg, "Console", g_term_tab == 0, "terminal");
  term_tab(seg, "Packets", g_term_tab == 1, "packets");

  if (g_term_tab == 0) {
    int composeH = 30;
    lv_obj_t* list = lv_obj_create(scr);
    lv_obj_set_pos(list, 4, 70); lv_obj_set_size(list, 320 - 8, 240 - 70 - composeH - 6);
    lv_obj_set_style_bg_opa(list, 0, 0); lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 6, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_row(list, 2, 0);
    log_line(list, "[12:01] ADV  GW-Hertford  (2 hops)", UI_PURPLE);
    log_line(list, "  rx rssi -78  snr 9.0", UI_MUTED);
    log_line(list, "[12:01] TXT  Public: \"on the hill\"", UI_GREEN);
    log_line(list, "[12:02] ACK  -> Andy-Mobile", UI_CYAN);
    log_line(list, "> advert", UI_AMBER);
    log_line(list, "OK - Advert sent", UI_TEXT);
    log_line(list, "> clock", UI_AMBER);
    log_line(list, "12:02 - 14/1/2026 UTC", UI_TEXT);

    // command bar
    lv_obj_t* bar = lv_ui_card(scr, 4, 240 - composeH - 2, 320 - 8, composeH);
    lv_obj_set_style_pad_all(bar, 4, 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* prompt = lv_label_create(bar);
    lv_label_set_text(prompt, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(prompt, lv_color_hex(UI_EMERALD), 0);
    lv_obj_set_style_pad_left(prompt, 4, 0);
    lv_obj_t* in = lv_label_create(bar);
    lv_label_set_text(in, "type a command");
    lv_obj_set_flex_grow(in, 1);
    lv_obj_set_style_pad_left(in, 8, 0);
    lv_obj_set_style_text_font(in, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(in, lv_color_hex(UI_MUTED), 0);
  } else {
    s_pkt_list = full_list(scr, 70);
    packets_fill(s_pkt_list);
    lv_ui_set_refresh(packets_tick);
  }
}
