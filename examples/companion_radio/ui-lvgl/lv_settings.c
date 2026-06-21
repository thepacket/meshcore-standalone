// LVGL settings: category list (Material rows) + a detail screen built from
// real widgets (dropdown, value rows, slider, switch) -- never raw text.
#include "lv_ui.h"
#include <stdio.h>

// ---- a Material list row: coloured icon chip + title + subtitle + chevron ----
static lv_obj_t* list_row(lv_obj_t* list, uint32_t color, const char* icon,
                          const char* title, const char* subtitle, const char* dest) {
  lv_obj_t* row = lv_ui_card(list, -1, 0, 0, 50);
  lv_obj_set_width(row, lv_pct(100));
  lv_obj_set_height(row, 48);
  lv_obj_set_style_min_height(row, 0, 0);
  lv_obj_set_style_pad_all(row, 7, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  if (dest) lv_ui_clickable(row, dest);

  lv_obj_t* chip = lv_ui_chip(row, color, icon, 30, true);
  lv_obj_set_style_margin_right(chip, 10, 0);

  lv_obj_t* mid = lv_obj_create(row);
  lv_obj_remove_flag(mid, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(mid, 0, 0); lv_obj_set_style_border_width(mid, 0, 0);
  lv_obj_set_style_pad_all(mid, 0, 0);
  lv_obj_set_flex_grow(mid, 1);
  lv_obj_set_height(mid, 36);
  lv_obj_set_flex_flow(mid, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(mid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

  lv_obj_t* t = lv_label_create(mid);
  lv_label_set_text(t, title);
  lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(t, lv_color_hex(UI_TEXT), 0);
  if (subtitle) {
    lv_obj_t* s = lv_label_create(mid);
    lv_label_set_text(s, subtitle);
    lv_obj_set_style_text_font(s, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s, lv_color_hex(UI_MUTED), 0);
  }

  lv_obj_t* chev = lv_label_create(row);
  lv_label_set_text(chev, LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_color(chev, lv_color_hex(UI_MUTED), 0);
  return row;
}

static lv_obj_t* make_scroll_list(lv_obj_t* scr) {
  lv_obj_t* list = lv_obj_create(scr);
  lv_obj_set_pos(list, 4, 34);
  lv_obj_set_size(list, 320 - 8, 240 - 34 - 4);
  lv_obj_set_style_bg_opa(list, 0, 0);
  lv_obj_set_style_border_width(list, 0, 0);
  lv_obj_set_style_pad_all(list, 2, 0);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(list, 6, 0);
  return list;
}

void lv_settings_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_topbar(scr, "Settings", UI_INDIGO, NULL);
  lv_obj_t* list = make_scroll_list(scr);
  list_row(list, UI_GREEN,  ICON_CONTACTS,  "Public info", "Name, location policy", "settings_radio");
  list_row(list, UI_PURPLE, ICON_REPEATERS, "Radio",       "910.5 MHz - BW62.5 SF7", "settings_radio");
  list_row(list, UI_BLUE,   ICON_CHAT,      "Messaging",   "ACKs, retries", "settings_radio");
  list_row(list, UI_ORANGE, ICON_MAP,       "Position",    "GPS, share location", "settings_radio");
  list_row(list, UI_RED,    ICON_NOISE,     "Telemetry",   "Base, environment", "settings_radio");
  list_row(list, UI_INDIGO, ICON_SETTINGS,  "Device",      "Bluetooth, time, power", "settings_radio");
}

// ---- detail: a settings group built from widgets -----------------------------
static lv_obj_t* setting_card(lv_obj_t* list, const char* title) {
  lv_obj_t* c = lv_ui_card(list, -1, 0, 0, 46);
  lv_obj_set_width(c, lv_pct(100));
  lv_obj_set_height(c, 44);
  lv_obj_set_style_min_height(c, 0, 0);
  lv_obj_set_style_pad_hor(c, 12, 0);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(c, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t* l = lv_label_create(c);
  lv_label_set_text(l, title);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(UI_TEXT), 0);
  return c;
}

void lv_settings_radio_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_topbar(scr, "Radio", UI_PURPLE, NULL);
  lv_obj_t* list = make_scroll_list(scr);

  // Preset dropdown
  lv_obj_t* c1 = setting_card(list, "Preset");
  lv_obj_t* dd = lv_dropdown_create(c1);
  lv_dropdown_set_options(dd, "USA/Canada\nEU\nUK/CH\nAU/NZ\nCustom");
  lv_obj_set_width(dd, 130);
  lv_obj_set_style_bg_color(dd, lv_color_hex(0x18202c), 0);
  lv_obj_set_style_border_color(dd, lv_color_hex(UI_PURPLE), 0);
  lv_obj_set_style_border_width(dd, 1, 0);
  lv_obj_set_style_text_color(dd, lv_color_hex(UI_TEXT), 0);

  // Frequency value chip
  lv_obj_t* c2 = setting_card(list, "Frequency");
  lv_ui_pill(c2, "910.525 MHz", UI_PURPLE);

  // TX power slider
  lv_obj_t* c3 = setting_card(list, "TX power");
  lv_obj_set_height(c3, 52);
  lv_obj_set_flex_flow(c3, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(c3, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_t* sl = lv_slider_create(c3);
  lv_obj_set_width(sl, lv_pct(100));
  lv_slider_set_range(sl, 1, 30);
  lv_slider_set_value(sl, 22, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(sl, lv_color_hex(0x2a3343), LV_PART_MAIN);
  lv_obj_set_style_bg_color(sl, lv_color_hex(UI_AMBER), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(sl, lv_color_hex(UI_AMBER), LV_PART_KNOB);

  // Boosted gain switch
  lv_obj_t* c4 = setting_card(list, "RX boosted gain");
  lv_obj_t* sw = lv_switch_create(c4);
  lv_obj_add_state(sw, LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(sw, lv_color_hex(UI_EMERALD), LV_PART_INDICATOR | LV_STATE_CHECKED);
}
