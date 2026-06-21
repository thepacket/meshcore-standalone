// LVGL settings: complete panel covering every configurable field found in the
// MeshCore companion firmware (NodePrefs + CMD_SET_*/CMD_GET_*). Category list +
// per-group detail rendered from a data table, one widget per field type.
//
// Prototype values are static; the firmware build will bind these rows to the
// real SettingsModel forwarders (same field set).
#include "lv_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { F_BOOL, F_ENUM, F_VAL, F_INFO, F_ACTION, F_ACTION_WARN } FType;

typedef struct {
  const char* label;
  FType       type;
  const char* value;   // VAL/INFO text, or ENUM options ('\n'-separated)
  int         sel;     // BOOL state, or ENUM selected index
} Field;

typedef struct {
  const char* title;
  uint32_t    color;
  const char* icon;
  const Field* fields;
  int          n;
} Group;

#define ENUM_TELEM "Deny\nFlags\nAll"

static const Field F_PUBLIC[] = {
  {"Node name",        F_VAL,    "MyNode", 0},
  {"Location policy",  F_ENUM,   "Hidden\nShared", 0},
  {"Send advert",      F_ACTION, NULL, 0},
  {"Send advert (flood)", F_ACTION, NULL, 0},
};
static const Field F_RADIO[] = {
  {"Preset",        F_ENUM, "USA/Canada\nEU\nUK/CH\nAU/NZ\nCustom", 0},
  {"Frequency",     F_VAL,  "910.525 MHz", 0},
  {"Bandwidth",     F_ENUM, "7.8\n10.4\n15.6\n20.8\n31.25\n41.7\n62.5\n125\n250\n500", 6},
  {"Spreading factor", F_VAL, "7", 0},
  {"Coding rate",   F_VAL,  "5", 0},
  {"TX power",      F_VAL,  "22 dBm", 0},
  {"Client repeat", F_BOOL, NULL, 0},
  {"RX boosted gain", F_BOOL, NULL, 1},
};
static const Field F_CONTACTS[] = {
  {"Manual add",          F_BOOL, NULL, 0},
  {"Auto-add users",      F_BOOL, NULL, 1},
  {"Auto-add repeaters",  F_BOOL, NULL, 1},
  {"Auto-add rooms",      F_BOOL, NULL, 1},
  {"Auto-add sensors",    F_BOOL, NULL, 1},
  {"Overwrite oldest",    F_BOOL, NULL, 0},
  {"Auto-add max hops",   F_VAL,  "3", 0},
};
static const Field F_MSG[] = {
  {"Multi-acks",     F_VAL,  "1", 0},
  {"Path hash mode", F_ENUM, "Off\nMode 1\nMode 2", 0},
};
static const Field F_POSITION[] = {
  {"Latitude",     F_VAL,  "0.0000 deg", 0},
  {"Longitude",    F_VAL,  "0.0000 deg", 0},
  {"GPS enabled",  F_BOOL, NULL, 0},
  {"GPS interval", F_VAL,  "0 s", 0},
};
static const Field F_TELEM[] = {
  {"Base",        F_ENUM, ENUM_TELEM, 0},
  {"Location",    F_ENUM, ENUM_TELEM, 0},
  {"Environment", F_ENUM, ENUM_TELEM, 0},
};
static const Field F_TUNING[] = {
  {"Airtime factor", F_VAL,    "1.0", 0},
  {"RX delay base",  F_VAL,    "500 ms", 0},
  {"Default scope",  F_INFO,   "(none)", 0},
  {"Clear default scope", F_ACTION, NULL, 0},
};
static const Field F_SECURITY[] = {
  {"BLE pin", F_VAL, "123456", 0},
};
static const Field F_DEVICE[] = {
  {"Buzzer quiet",    F_BOOL, NULL, 0},
  {"Set time",        F_ACTION, NULL, 0},
  {"Battery/storage", F_INFO, "4050 mV  120/1536 KB", 0},
  {"Firmware",        F_INFO, "v1.16.0", 0},
  {"Device",          F_INFO, "LilyGo T-Deck", 0},
  {"Reboot",          F_ACTION_WARN, NULL, 0},
  {"Factory reset",   F_ACTION_WARN, NULL, 0},
};
static const Field F_CHANNELS[] = {
  {"Public",       F_INFO, "shared PSK", 0},
  {"Add channel",  F_ACTION, NULL, 0},
};

#define GRP(t, col, ic, arr) {t, col, ic, arr, (int)(sizeof(arr)/sizeof(arr[0]))}
static const Group GROUPS[] = {
  GRP("Public info", UI_GREEN,   ICON_CONTACTS,  F_PUBLIC),
  GRP("Radio",       UI_PURPLE,  ICON_REPEATERS, F_RADIO),
  GRP("Contacts",    UI_BLUE,    ICON_CONTACTS,  F_CONTACTS),
  GRP("Messaging",   UI_CYAN,    ICON_CHAT,      F_MSG),
  GRP("Position",    UI_ORANGE,  ICON_MAP,       F_POSITION),
  GRP("Telemetry",   UI_RED,     ICON_NOISE,     F_TELEM),
  GRP("Tuning",      UI_AMBER,   ICON_SIGNAL,    F_TUNING),
  GRP("Security",    UI_TEAL,    ICON_FINDER,    F_SECURITY),
  GRP("Device",      UI_INDIGO,  ICON_SETTINGS,  F_DEVICE),
  GRP("Channels",    UI_LIME,    ICON_CHAT,      F_CHANNELS),
};
static const int N_GROUPS = (int)(sizeof(GROUPS) / sizeof(GROUPS[0]));
static const char* SG_DEST[] = {"sg0","sg1","sg2","sg3","sg4","sg5","sg6","sg7","sg8","sg9"};

int  lv_settings_group_count(void) { return N_GROUPS; }

static lv_obj_t* scroll_list(lv_obj_t* scr) {
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

// ---- category list ----------------------------------------------------------
void lv_settings_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_topbar(scr, "Settings", UI_INDIGO, NULL);
  lv_obj_t* list = scroll_list(scr);
  for (int i = 0; i < N_GROUPS; i++) {
    const Group* g = &GROUPS[i];
    lv_obj_t* row = lv_ui_card(list, -1, 0, 0, 50);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, 48);
    lv_obj_set_style_min_height(row, 0, 0);
    lv_obj_set_style_pad_all(row, 7, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_ui_clickable(row, SG_DEST[i]);

    lv_obj_t* chip = lv_ui_chip(row, g->color, g->icon, 30, true);
    lv_obj_set_style_margin_right(chip, 10, 0);

    lv_obj_t* mid = lv_obj_create(row);
    lv_obj_remove_flag(mid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(mid, 0, 0); lv_obj_set_style_border_width(mid, 0, 0);
    lv_obj_set_style_pad_all(mid, 0, 0); lv_obj_set_flex_grow(mid, 1); lv_obj_set_height(mid, 36);
    lv_obj_set_flex_flow(mid, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(mid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t* t = lv_label_create(mid);
    lv_label_set_text(t, g->title);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(t, lv_color_hex(UI_TEXT), 0);
    lv_obj_t* s = lv_label_create(mid);
    lv_label_set_text_fmt(s, "%d settings", g->n);
    lv_obj_set_style_text_font(s, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s, lv_color_hex(UI_MUTED), 0);

    lv_obj_t* chev = lv_label_create(row);
    lv_label_set_text(chev, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(chev, lv_color_hex(UI_MUTED), 0);
  }
}

// ---- per-field widgets ------------------------------------------------------
static lv_obj_t* field_card(lv_obj_t* list, const char* label) {
  lv_obj_t* c = lv_ui_card(list, -1, 0, 0, 44);
  lv_obj_set_width(c, lv_pct(100));
  lv_obj_set_height(c, 42);
  lv_obj_set_style_min_height(c, 0, 0);
  lv_obj_set_style_pad_hor(c, 12, 0);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(c, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t* l = lv_label_create(c);
  lv_label_set_text(l, label);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(UI_TEXT), 0);
  return c;
}

static void action_button(lv_obj_t* list, const char* label, uint32_t color) {
  lv_obj_t* b = lv_ui_card(list, -1, 0, 0, 40);
  lv_obj_set_width(b, lv_pct(100));
  lv_obj_set_height(b, 38);
  lv_obj_set_style_min_height(b, 0, 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(b, 40, 0);
  lv_obj_set_style_border_color(b, lv_color_hex(color), 0);
  lv_obj_set_style_border_opa(b, 200, 0);
  lv_obj_t* l = lv_label_create(b);
  lv_label_set_text(l, label);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(0xffffff), 0);
  lv_obj_center(l);
}

void lv_settings_group_create(lv_obj_t* scr, int idx) {
  if (idx < 0 || idx >= N_GROUPS) idx = 0;
  const Group* g = &GROUPS[idx];
  lv_ui_screen_bg(scr);
  lv_ui_topbar(scr, g->title, g->color, NULL);
  lv_obj_t* list = scroll_list(scr);

  for (int i = 0; i < g->n; i++) {
    const Field* f = &g->fields[i];
    switch (f->type) {
      case F_ACTION:      action_button(list, f->label, g->color); break;
      case F_ACTION_WARN: action_button(list, f->label, UI_RED);   break;
      case F_BOOL: {
        lv_obj_t* c = field_card(list, f->label);
        lv_obj_t* sw = lv_switch_create(c);
        if (f->sel) lv_obj_add_state(sw, LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(sw, lv_color_hex(0x2a3343), LV_PART_MAIN);
        lv_obj_set_style_bg_color(sw, lv_color_hex(g->color), LV_PART_INDICATOR | LV_STATE_CHECKED);
        break;
      }
      case F_ENUM: {
        lv_obj_t* c = field_card(list, f->label);
        lv_obj_t* dd = lv_dropdown_create(c);
        lv_dropdown_set_options(dd, f->value);
        lv_dropdown_set_selected(dd, f->sel);
        lv_obj_set_width(dd, 124);
        lv_obj_set_style_bg_color(dd, lv_color_hex(0x18202c), 0);
        lv_obj_set_style_border_color(dd, lv_color_hex(g->color), 0);
        lv_obj_set_style_border_width(dd, 1, 0);
        lv_obj_set_style_text_color(dd, lv_color_hex(UI_TEXT), 0);
        lv_obj_set_style_text_font(dd, &lv_font_montserrat_14, 0);
        break;
      }
      case F_VAL: {
        lv_obj_t* c = field_card(list, f->label);
        lv_ui_pill(c, f->value, g->color);
        break;
      }
      case F_INFO: {
        lv_obj_t* c = field_card(list, f->label);
        lv_obj_t* v = lv_label_create(c);
        lv_label_set_text(v, f->value);
        lv_obj_set_style_text_font(v, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(v, lv_color_hex(UI_MUTED), 0);
        break;
      }
    }
  }
}
