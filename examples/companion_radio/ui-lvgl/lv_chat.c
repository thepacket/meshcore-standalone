// LVGL chat screens: list (Channels/DMs tabs, Material rows) + conversation
// (gradient speech bubbles, coloured sender names, compose bar).
#include "lv_ui.h"
#include <stdio.h>
#include <string.h>

// ---- small helpers ----------------------------------------------------------
static lv_obj_t* avatar(lv_obj_t* parent, const char* txt, uint32_t color) {
  lv_obj_t* a = lv_obj_create(parent);
  lv_obj_remove_flag(a, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(a, 38, 38);
  lv_obj_set_style_radius(a, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(a, lv_color_hex(color), 0);
  lv_obj_set_style_bg_grad_color(a, lv_color_darken(lv_color_hex(color), 90), 0);
  lv_obj_set_style_bg_grad_dir(a, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_border_width(a, 0, 0);
  lv_obj_set_style_pad_all(a, 0, 0);
  lv_obj_set_style_shadow_color(a, lv_color_hex(color), 0);
  lv_obj_set_style_shadow_opa(a, 110, 0);
  lv_obj_set_style_shadow_width(a, 10, 0);
  lv_obj_t* l = lv_label_create(a);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(0xffffff), 0);
  lv_obj_center(l);
  return a;
}

static const uint32_t NAME_COLORS[] = {UI_BLUE, UI_GREEN, UI_AMBER, UI_PINK, UI_CYAN, UI_PURPLE};
static uint32_t name_color(const char* s) {
  uint32_t h = 5381; for (const char* p = s; *p; p++) h = ((h << 5) + h) + (uint8_t)*p;
  return NAME_COLORS[h % 6];
}

// =============================================================================
// Chat list
// =============================================================================
typedef struct { const char* name; const char* preview; const char* time; int unread; uint32_t color; bool channel; } Row;

static void add_row(lv_obj_t* list, const Row* r) {
  lv_obj_t* row = lv_ui_card(list, -1, 0, 0, 56);
  lv_obj_set_width(row, lv_pct(100));
  lv_obj_set_height(row, 54);
  lv_obj_set_style_min_height(row, 0, 0);
  lv_obj_set_style_pad_all(row, 8, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  char ini[8];
  if (r->channel) snprintf(ini, sizeof(ini), "#");
  else { ini[0] = r->name[0]; ini[1] = 0; }
  lv_obj_t* av = avatar(row, ini, r->color);
  lv_obj_set_style_margin_right(av, 10, 0);

  lv_obj_t* mid = lv_obj_create(row);
  lv_obj_remove_flag(mid, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(mid, 0, 0); lv_obj_set_style_border_width(mid, 0, 0);
  lv_obj_set_style_pad_all(mid, 0, 0);
  lv_obj_set_flex_grow(mid, 1);
  lv_obj_set_height(mid, 40);
  lv_obj_set_flex_flow(mid, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(mid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

  lv_obj_t* nm = lv_label_create(mid);
  lv_label_set_text(nm, r->name);
  lv_obj_set_style_text_font(nm, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(nm, lv_color_hex(UI_TEXT), 0);

  lv_obj_t* pv = lv_label_create(mid);
  lv_label_set_text(pv, r->preview);
  lv_label_set_long_mode(pv, LV_LABEL_LONG_DOT);
  lv_obj_set_width(pv, lv_pct(100));
  lv_obj_set_style_text_font(pv, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(pv, lv_color_hex(UI_MUTED), 0);

  lv_obj_t* right = lv_obj_create(row);
  lv_obj_remove_flag(right, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(right, 0, 0); lv_obj_set_style_border_width(right, 0, 0);
  lv_obj_set_style_pad_all(right, 0, 0);
  lv_obj_set_size(right, 36, 40);
  lv_obj_t* tm = lv_label_create(right);
  lv_label_set_text(tm, r->time);
  lv_obj_set_style_text_font(tm, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(tm, lv_color_hex(UI_MUTED), 0);
  lv_obj_align(tm, LV_ALIGN_TOP_RIGHT, 0, 0);
  if (r->unread > 0) {
    lv_obj_t* b = lv_ui_pill(right, "", UI_BADGE);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_t* bl = lv_obj_get_child(b, 0);
    lv_label_set_text_fmt(bl, "%d", r->unread);
    lv_obj_set_size(b, 20, 18);
    lv_obj_align(b, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  }
}

static void seg_tab(lv_obj_t* parent, const char* txt, bool active) {
  lv_obj_t* t = lv_obj_create(parent);
  lv_obj_remove_flag(t, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_grow(t, 1);
  lv_obj_set_height(t, lv_pct(100));
  lv_obj_set_style_radius(t, 8, 0);
  lv_obj_set_style_bg_color(t, lv_color_hex(UI_BLUE), 0);
  lv_obj_set_style_bg_opa(t, active ? 200 : 0, 0);
  lv_obj_set_style_border_width(t, 0, 0);
  lv_obj_set_style_pad_all(t, 0, 0);
  lv_obj_t* l = lv_label_create(t);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(active ? 0xffffff : UI_MUTED), 0);
  lv_obj_center(l);
}

void lv_chat_list_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_topbar(scr, "Chat", UI_BLUE, NULL);

  // segmented tabs
  lv_obj_t* seg = lv_obj_create(scr);
  lv_obj_remove_flag(seg, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(seg, 320 - 16, 28);
  lv_obj_set_pos(seg, 8, 36);
  lv_obj_set_style_radius(seg, 10, 0);
  lv_obj_set_style_bg_color(seg, lv_color_hex(UI_CARD), 0);
  lv_obj_set_style_bg_opa(seg, 16, 0);
  lv_obj_set_style_border_width(seg, 0, 0);
  lv_obj_set_style_pad_all(seg, 3, 0);
  lv_obj_set_flex_flow(seg, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(seg, 4, 0);
  seg_tab(seg, "Channels", true);
  seg_tab(seg, "DMs", false);

  // list
  lv_obj_t* list = lv_obj_create(scr);
  lv_obj_set_pos(list, 4, 70);
  lv_obj_set_size(list, 320 - 8, 240 - 70 - 4);
  lv_obj_set_style_bg_opa(list, 0, 0);
  lv_obj_set_style_border_width(list, 0, 0);
  lv_obj_set_style_pad_all(list, 2, 0);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(list, 6, 0);

  Row rows[] = {
    {"Public",      "Carol: Map here: meshcore.io", "now", 2, UI_BLUE,  true},
    {"#london",     "Dave: anyone near E1?",        "4m",  0, UI_PURPLE, true},
    {"Andy-Mobile", "You: ETA about 10 minutes",    "1m",  0, UI_GREEN, false},
    {"GW-Hertford", "Repeater online",              "2h",  0, UI_AMBER, false},
  };
  for (unsigned i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) add_row(list, &rows[i]);
}

// =============================================================================
// Conversation
// =============================================================================
static void add_bubble(lv_obj_t* scroll, const char* sender, const char* text,
                       bool outgoing, const char* status) {
  lv_obj_t* row = lv_obj_create(scroll);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_width(row, lv_pct(100));
  lv_obj_set_height(row, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(row, 0, 0); lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);

  lv_obj_t* bub = lv_obj_create(row);
  lv_obj_remove_flag(bub, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_width(bub, LV_SIZE_CONTENT);
  lv_obj_set_style_max_width(bub, 224, 0);
  lv_obj_set_height(bub, LV_SIZE_CONTENT);
  lv_obj_set_style_radius(bub, 12, 0);
  lv_obj_set_style_pad_all(bub, 7, 0);
  lv_obj_set_style_border_width(bub, 0, 0);
  lv_obj_set_flex_flow(bub, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(bub, 1, 0);
  if (outgoing) {
    lv_obj_set_style_bg_color(bub, lv_color_hex(UI_BLUE), 0);
    lv_obj_set_style_bg_grad_color(bub, lv_color_hex(UI_INDIGO), 0);
    lv_obj_set_style_bg_grad_dir(bub, LV_GRAD_DIR_VER, 0);
    lv_obj_align(bub, LV_ALIGN_TOP_RIGHT, 0, 0);
  } else {
    lv_obj_set_style_bg_color(bub, lv_color_hex(UI_CARD), 0);
    lv_obj_set_style_bg_opa(bub, 22, 0);
    lv_obj_align(bub, LV_ALIGN_TOP_LEFT, 0, 0);
  }

  if (!outgoing && sender) {
    lv_obj_t* s = lv_label_create(bub);
    lv_label_set_text(s, sender);
    lv_obj_set_style_text_font(s, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s, lv_color_hex(name_color(sender)), 0);
  }
  lv_obj_t* t = lv_label_create(bub);
  lv_label_set_text(t, text);
  lv_label_set_long_mode(t, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(t, LV_SIZE_CONTENT);
  lv_obj_set_style_max_width(t, 210, 0);
  lv_obj_set_style_text_font(t, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(t, lv_color_hex(outgoing ? 0xffffff : UI_TEXT), 0);

  if (outgoing && status) {
    lv_obj_t* st = lv_label_create(bub);
    lv_label_set_text(st, status);
    lv_obj_set_style_text_font(st, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(st, lv_color_hex(0xd6e2ff), 0);
    lv_obj_set_style_align(st, LV_ALIGN_RIGHT_MID, 0);
  }
}

void lv_chat_conv_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_topbar(scr, "Public", UI_BLUE, NULL);

  int composeH = 32;
  lv_obj_t* scroll = lv_obj_create(scr);
  lv_obj_set_pos(scroll, 4, 32);
  lv_obj_set_size(scroll, 320 - 8, 240 - 32 - composeH - 4);
  lv_obj_set_style_bg_opa(scroll, 0, 0);
  lv_obj_set_style_border_width(scroll, 0, 0);
  lv_obj_set_style_pad_all(scroll, 4, 0);
  lv_obj_set_flex_flow(scroll, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(scroll, 6, 0);

  add_bubble(scroll, "Alice", "Anyone around the north side?", false, NULL);
  add_bubble(scroll, "Bob", "Yep, copy you 5 by 9", false, NULL);
  add_bubble(scroll, NULL, "On the hill now, strong signal", true, "delivered");
  add_bubble(scroll, "Carol", "Map here: meshcore.io", false, NULL);
  add_bubble(scroll, NULL, "Nice, on my way", true, "sending");

  // compose bar
  lv_obj_t* bar = lv_ui_card(scr, 4, 240 - composeH - 2, 320 - 8, composeH);
  lv_obj_set_style_pad_all(bar, 4, 0);
  lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t* input = lv_label_create(bar);
  lv_label_set_text(input, "Message");
  lv_obj_set_flex_grow(input, 1);
  lv_obj_set_style_text_font(input, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(input, lv_color_hex(UI_MUTED), 0);
  lv_obj_set_style_pad_left(input, 6, 0);

  lv_obj_t* send = lv_obj_create(bar);
  lv_obj_remove_flag(send, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(send, 24, 24);
  lv_obj_set_style_radius(send, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(send, lv_color_hex(UI_BLUE), 0);
  lv_obj_set_style_border_width(send, 0, 0);
  lv_obj_set_style_pad_all(send, 0, 0);
  lv_obj_t* si = lv_label_create(send);
  lv_label_set_text(si, LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_color(si, lv_color_hex(0xffffff), 0);
  lv_obj_center(si);
}
