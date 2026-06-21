// LVGL peer-details panel: everything known about a contact/sender, opened by
// tapping their message. Header + quick actions + stat grid + location + key.
#include "lv_ui.h"
#include <stdio.h>
#include <string.h>

const char* lv_chat_active_peer(void);  // provided by lv_chat.c

// stat card: label on TOP, value below
static void stat_card(lv_obj_t* grid, const char* k, const char* v, uint32_t color) {
  lv_obj_t* c = lv_ui_card(grid, -1, 0, 0, 0);
  lv_obj_set_size(c, 98, 44);
  lv_obj_set_style_pad_all(c, 5, 0);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(c, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_t* kl = lv_label_create(c);
  lv_label_set_text(kl, k);
  lv_obj_set_style_text_font(kl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(kl, lv_color_hex(UI_MUTED), 0);
  lv_obj_t* vl = lv_label_create(c);
  lv_label_set_text(vl, v);
  lv_obj_set_style_text_font(vl, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(vl, lv_color_hex(color), 0);
}

// half-width key/value column (label on top), for side-by-side pairs
static void kv_col(lv_obj_t* row, const char* k, const char* v) {
  lv_obj_t* c = lv_ui_card(row, -1, 0, 0, 0);
  lv_obj_set_flex_grow(c, 1); lv_obj_set_height(c, LV_SIZE_CONTENT);
  lv_obj_set_style_min_height(c, 0, 0); lv_obj_set_style_pad_all(c, 8, 0);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(c, 2, 0);
  lv_obj_t* kl = lv_label_create(c);
  lv_label_set_text(kl, k);
  lv_obj_set_style_text_font(kl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(kl, lv_color_hex(UI_MUTED), 0);
  lv_obj_t* vl = lv_label_create(c);
  lv_label_set_text(vl, v);
  lv_obj_set_style_text_font(vl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(vl, lv_color_hex(UI_TEXT), 0);
}

// key/value block: label on TOP, value on the line below (wraps if long)
static void kv_block(lv_obj_t* list, const char* k, const char* v, bool wrap) {
  lv_obj_t* c = lv_ui_card(list, -1, 0, 0, 0);
  lv_obj_set_width(c, lv_pct(100)); lv_obj_set_height(c, LV_SIZE_CONTENT);
  lv_obj_set_style_min_height(c, 0, 0); lv_obj_set_style_pad_all(c, 8, 0);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(c, 2, 0);
  lv_obj_t* kl = lv_label_create(c);
  lv_label_set_text(kl, k);
  lv_obj_set_style_text_font(kl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(kl, lv_color_hex(UI_MUTED), 0);
  lv_obj_t* vl = lv_label_create(c);
  lv_label_set_text(vl, v);
  lv_obj_set_style_text_font(vl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(vl, lv_color_hex(UI_TEXT), 0);
  if (wrap) { lv_label_set_long_mode(vl, LV_LABEL_LONG_WRAP); lv_obj_set_width(vl, lv_pct(100)); }
}

// checkable favourite button: faint when off, filled/lighter when favourited
static void fav_btn(lv_obj_t* row, bool is_fav) {
  lv_obj_t* b = lv_ui_card(row, -1, 0, 0, 0);
  lv_obj_set_flex_grow(b, 1); lv_obj_set_height(b, 34);
  lv_obj_set_style_pad_all(b, 0, 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(UI_GREEN), 0);
  lv_obj_set_style_bg_opa(b, 30, 0);                       // not a favourite: faint
  lv_obj_set_style_bg_opa(b, 230, LV_STATE_CHECKED);       // favourite: filled/lighter
  lv_obj_set_style_border_color(b, lv_color_hex(UI_GREEN), 0);
  lv_obj_set_style_border_opa(b, 200, 0);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CHECKABLE);               // LVGL toggles LV_STATE_CHECKED
  if (is_fav) lv_obj_add_state(b, LV_STATE_CHECKED);
  lv_obj_t* l = lv_label_create(b);
  lv_label_set_text(l, LV_SYMBOL_OK "  Favourite");
  lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(0xffffff), 0);
  lv_obj_center(l);
}

static void act_btn(lv_obj_t* row, const char* icon, const char* txt, uint32_t color, const char* dest) {
  lv_obj_t* b = lv_ui_card(row, -1, 0, 0, 0);
  lv_obj_set_flex_grow(b, 1); lv_obj_set_height(b, 34);
  lv_obj_set_style_bg_color(b, lv_color_hex(color), 0); lv_obj_set_style_bg_opa(b, 44, 0);
  lv_obj_set_style_border_color(b, lv_color_hex(color), 0); lv_obj_set_style_border_opa(b, 200, 0);
  lv_obj_set_style_pad_all(b, 0, 0);
  if (dest) lv_ui_clickable(b, dest);
  lv_obj_t* l = lv_label_create(b);
  lv_label_set_text_fmt(l, "%s  %s", icon, txt);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(0xffffff), 0);
  lv_obj_center(l);
}

void lv_peer_create(lv_obj_t* scr) {
  const char* name = lv_chat_active_peer();
  lv_ui_screen_bg(scr);
  lv_ui_topbar(scr, name, UI_BLUE, NULL);

  lv_obj_t* list = lv_obj_create(scr);
  lv_obj_set_pos(list, 4, 34);
  lv_obj_set_size(list, 320 - 8, 240 - 34 - 4);
  lv_obj_set_style_bg_opa(list, 0, 0); lv_obj_set_style_border_width(list, 0, 0);
  lv_obj_set_style_pad_all(list, 2, 0);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(list, 6, 0);

  // header: avatar + name + type
  lv_obj_t* head = lv_ui_card(list, -1, 0, 0, 56);
  lv_obj_set_width(head, lv_pct(100)); lv_obj_set_height(head, 54);
  lv_obj_set_style_min_height(head, 0, 0); lv_obj_set_style_pad_all(head, 8, 0);
  lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(head, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t* chip = lv_ui_chip(head, UI_BLUE, ICON_CONTACTS, 36, true);
  lv_obj_set_style_margin_right(chip, 10, 0);
  lv_obj_t* col = lv_obj_create(head);
  lv_obj_remove_flag(col, LV_OBJ_FLAG_SCROLLABLE); lv_obj_remove_flag(col, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(col, 0, 0); lv_obj_set_style_border_width(col, 0, 0);
  lv_obj_set_style_pad_all(col, 0, 0); lv_obj_set_flex_grow(col, 1); lv_obj_set_height(col, 40);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_t* nm = lv_label_create(col);
  lv_label_set_text(nm, name);
  lv_obj_set_style_text_font(nm, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(nm, lv_color_hex(UI_TEXT), 0);
  lv_ui_pill(col, "Chat contact", UI_BLUE);

  // quick actions
  lv_obj_t* acts = lv_obj_create(list);
  lv_obj_remove_flag(acts, LV_OBJ_FLAG_SCROLLABLE); lv_obj_remove_flag(acts, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_width(acts, lv_pct(100)); lv_obj_set_height(acts, 38);
  lv_obj_set_style_bg_opa(acts, 0, 0); lv_obj_set_style_border_width(acts, 0, 0);
  lv_obj_set_style_pad_all(acts, 0, 0);
  lv_obj_set_flex_flow(acts, LV_FLEX_FLOW_ROW); lv_obj_set_style_pad_column(acts, 5, 0);
  act_btn(acts, LV_SYMBOL_LEFT, "Message", UI_BLUE, "back");
  act_btn(acts, "", "Trace", UI_AMBER, "trace");
  fav_btn(acts, true);   // checkable: filled when favourite, toggles on tap

  // signal stats grid
  lv_obj_t* grid = lv_obj_create(list);
  lv_obj_remove_flag(grid, LV_OBJ_FLAG_SCROLLABLE); lv_obj_remove_flag(grid, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_width(grid, lv_pct(100)); lv_obj_set_height(grid, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(grid, 0, 0); lv_obj_set_style_border_width(grid, 0, 0);
  lv_obj_set_style_pad_all(grid, 0, 0);
  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_row(grid, 6, 0); lv_obj_set_style_pad_column(grid, 6, 0);
  stat_card(grid, "RSSI", "-78 dBm", UI_GREEN);
  stat_card(grid, "SNR", "9.0 dB", UI_GREEN);
  stat_card(grid, "Distance", "4.2 km", UI_CYAN);
  stat_card(grid, "Hops", "2", UI_TEXT);
  stat_card(grid, "Last heard", "12s", UI_TEXT);
  stat_card(grid, "Path", "direct", UI_TEXT);

  // location: latitude + longitude side by side
  lv_obj_t* loc = lv_obj_create(list);
  lv_obj_remove_flag(loc, LV_OBJ_FLAG_SCROLLABLE); lv_obj_remove_flag(loc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_width(loc, lv_pct(100)); lv_obj_set_height(loc, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(loc, 0, 0); lv_obj_set_style_border_width(loc, 0, 0);
  lv_obj_set_style_pad_all(loc, 0, 0);
  lv_obj_set_flex_flow(loc, LV_FLEX_FLOW_ROW); lv_obj_set_style_pad_column(loc, 6, 0);
  kv_col(loc, "Longitude", "-0.0810 deg");
  kv_col(loc, "Latitude", "51.7960 deg");

  // identity: full key wraps to its own line
  kv_block(list, "Public key",
           "a37f12c49b0e5d612f8a44d3b7e190ca5e6b8847aa12cd3490ff7e2b1d0c4a59", true);
}
