// LVGL tools: Trace route, and Terminal (Console + Packets tabs / packet monitor).
#include "lv_ui.h"
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

void lv_trace_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, "Trace");

  lv_obj_t* hdr = lv_ui_md_card(scr);
  lv_obj_set_pos(hdr, 12, 42); lv_obj_set_size(hdr, 320 - 24, 34);
  lv_obj_set_style_pad_ver(hdr, 0, 0);
  lv_obj_set_flex_flow(hdr, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_hor(hdr, 12, 0);
  lv_obj_t* nm = lv_label_create(hdr);
  lv_label_set_text(nm, "Repeater-7");
  lv_obj_set_style_text_font(nm, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(nm, lv_color_hex(UI_AMBER), 0);
  lv_ui_pill(hdr, "3 hops", UI_AMBER);

  lv_obj_t* list = full_list(scr, 74);
  hop_row(list, "1.  id A3", "+8.0 dB", UI_GREEN);
  hop_row(list, "2.  id 7F", "+5.0 dB", UI_GREEN);
  hop_row(list, "3.  id 12", "+3.0 dB", UI_AMBER);
  hop_row(list, LV_SYMBOL_DOWN "  to you", "+7.0 dB", UI_BLUE);
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
    lv_obj_t* list = full_list(scr, 70);
    pkt_row(list, "TXT", UI_BLUE,   "FLD  h2  pl16  -78dBm  9.0  3s");
    pkt_row(list, "ADV", UI_PURPLE, "FLD  h0  pl10  -99dBm  4.7  17s");
    pkt_row(list, "GRP", UI_CYAN,   "TFL  h1  pl8   -78dBm  9.2  20s");
    pkt_row(list, "ACK", UI_GREEN,  "DIR  h3  pl2   -105dBm 2.5  1m");
    pkt_row(list, "TRC", UI_AMBER,  "FLD  h4  pl5   -70dBm  11.0 2m");
  }
}
