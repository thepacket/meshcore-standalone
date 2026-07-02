// LVGL settings: complete panel covering every configurable field found in the
// MeshCore companion firmware (NodePrefs + CMD_SET_*/CMD_GET_*). Category list,
// per-group detail (one widget per field type), and a keyboard editor for the
// value fields. Edits persist in an in-memory overlay (prototype); the firmware
// build binds the same rows to the SettingsModel forwarders.
#include "lv_ui.h"
#include "lv_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { F_BOOL, F_ENUM, F_VAL, F_INFO, F_ACTION, F_ACTION_WARN } FType;

typedef struct {
  const char* label;
  FType       type;
  const char* value;   // VAL/INFO initial text, or ENUM options ('\n'-separated)
  int         sel;     // BOOL state, or ENUM selected index
} Field;

typedef struct {
  const char* title;
  uint32_t    color;
  const char* icon;
  const Field* fields;
  int          n;
} Group;

#define ENUM_TELEM "Deny\nAllow-listed\nEveryone"

static const Field F_PUBLIC[] = {
  {"Node name",        F_VAL,    "MyNode", 0},
  {"Location policy",  F_ENUM,   "Hidden\nShared", 0},
  {"Send advert",      F_ACTION, NULL, 0},
  {"Send advert (flood)", F_ACTION, NULL, 0},
};
static const Field F_RADIO[] = {
  {"Preset",        F_ENUM, "USA/Canada\nEU\nUK/CH\nAU/NZ\nCustom", 0},
  {"Frequency",     F_VAL,  "910.525", 0},
  {"Bandwidth",     F_ENUM, "7.8\n10.4\n15.6\n20.8\n31.25\n41.7\n62.5\n125\n250\n500", 6},
  {"Spreading factor", F_VAL, "7", 0},
  {"Coding rate",   F_VAL,  "5", 0},
  {"TX power",      F_VAL,  "22", 0},
  {"Client repeat", F_BOOL, NULL, 0},
  {"RX boosted gain", F_BOOL, NULL, 1},
  {"Channel activity detection", F_BOOL, NULL, 1},
};
static const Field F_CONTACTS[] = {
  {"Manual add",          F_BOOL, NULL, 0},
  {"Auto-add users",      F_BOOL, NULL, 1},
  {"Auto-add repeaters",  F_BOOL, NULL, 1},
  {"Auto-add rooms",      F_BOOL, NULL, 1},
  {"Auto-add sensors",    F_BOOL, NULL, 1},
  {"Overwrite oldest",    F_BOOL, NULL, 0},
  {"Auto-add max hops",   F_VAL,  "3", 0},
  {"Auto-add discovered", F_BOOL, NULL, 1},   // save Discover responders as contacts
};
static const Field F_MSG[] = {
  {"Multi-acks",     F_VAL,  "1", 0},
  {"Path hash mode", F_ENUM, "Off\nMode 1\nMode 2", 0},
};
static const Field F_POSITION[] = {
  {"Latitude",     F_VAL,  "0.0000", 0},
  {"Longitude",    F_VAL,  "0.0000", 0},
  {"GPS enabled",  F_BOOL, NULL, 0},
  {"GPS interval", F_VAL,  "0", 0},
};
static const Field F_TELEM[] = {
  {"Base",        F_ENUM, ENUM_TELEM, 0},
  {"Location",    F_ENUM, ENUM_TELEM, 0},
  {"Environment", F_ENUM, ENUM_TELEM, 0},
};
static const Field F_TUNING[] = {
  {"Airtime factor", F_VAL,    "1.0", 0},
  {"RX delay base",  F_VAL,    "500", 0},
  {"Default scope",  F_INFO,   "(none)", 0},
  {"Clear default scope", F_ACTION, NULL, 0},
};
static const Field F_SECURITY[] = {
  {"BLE pin", F_VAL, "123456", 0},
  {"Active BLE pin", F_INFO, "(BLE off)", 0},
};
static const Field F_TIME[] = {
  {"Set time (UTC)",  F_VAL, "", 0},        // edit as "YYYY-MM-DD HH:MM" (or epoch secs)
  {"Sync from GPS",  F_BOOL, NULL, 1},      // when GPS has a fix, take time from it (highest priority)
  {"Time source 1",  F_VAL, "(none)", 0},   // else named repeaters used as clock references
  {"Time source 2",  F_VAL, "(none)", 0},
  {"Time source 3",  F_VAL, "(none)", 0},
};
static const Field F_WIFI[] = {
  {"Enabled",        F_BOOL,   NULL, 0},        // STA connection for internet features
  {"Network",        F_VAL,    "(none)", 0},    // SSID (or pick one via Scan)
  {"Password",       F_VAL,    "", 0},
  {"Scan networks",  F_ACTION, NULL, 0},        // -> wifi_scan picker
  {"Status",         F_INFO,   "off", 0},       // IP + RSSI once connected
  {"Ping test",      F_ACTION, NULL, 0},        // 4 ICMP pings to 8.8.8.8 (internet check)
  {"Ping",           F_INFO,   "--", 0},        // live result of the last ping test
  {"NTP clock sync", F_BOOL,   NULL, 1},        // set the mesh RTC from pool.ntp.org
};
static const Field F_DEVICE[] = {
  {"On-screen keyboard", F_BOOL, NULL, 1},   // show the touch keyboard for text entry
  {"Buzzer quiet",    F_BOOL, NULL, 0},
  {"Battery/storage", F_INFO, "4050 mV  120/1536 KB", 0},
  {"Contacts",        F_INFO, "0 / 100 used", 0},
  {"Firmware",        F_INFO, "v1.16.0", 0},
  {"Device",          F_INFO, "LilyGo T-Deck", 0},
  {"Reboot",          F_ACTION_WARN, NULL, 0},
  {"Factory reset",   F_ACTION_WARN, NULL, 0},
};
static const Field F_CHANNELS[] = {
  {"Public",       F_INFO, "shared PSK", 0},
  {"Add channel",  F_ACTION, NULL, 0},
};
static const Field F_DATA[] = {
  {"Backup config",    F_ACTION, NULL, 0},   // config -> /meshcore/config.txt on SD
  {"Restore config",   F_ACTION, NULL, 0},   // config <- SD
  {"Backup contacts",  F_ACTION, NULL, 0},   // contacts + channels -> /meshcore/appdata.txt on SD
  {"Restore contacts", F_ACTION, NULL, 0},   // contacts + channels <- SD
  {"Export packets",   F_ACTION, NULL, 0},   // packet-capture ring dump to USB serial
  {"Debug logs",      F_INFO,   "USB serial", 0},   // where MESH_DEBUG output goes (build-flag gated)
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
  GRP("Security",    UI_TEAL,    ICON_SHIELD,    F_SECURITY),
  GRP("Time",        UI_AMBER,   ICON_CLOCK,     F_TIME),
  GRP("Wi-Fi",       UI_CYAN,    ICON_WIFI,      F_WIFI),
  GRP("Device",      UI_INDIGO,  ICON_SETTINGS,  F_DEVICE),
  GRP("Channels",    UI_LIME,    ICON_CHAT,      F_CHANNELS),
  GRP("Data",        UI_EMERALD, ICON_TERMINAL,  F_DATA),
};
#define N_GROUPS  ((int)(sizeof(GROUPS) / sizeof(GROUPS[0])))
#define MAX_FIELDS 8
static const char* SG_DEST[] = {"sg0","sg1","sg2","sg3","sg4","sg5","sg6","sg7","sg8","sg9","sg10","sg11","sg12"};

// ---- mutable value overlay (prototype state; persists across navigation) ----
// width fits a 63-char Wi-Fi passphrase (the longest editable value)
static char g_val[N_GROUPS][MAX_FIELDS][68];
static int  g_sel[N_GROUPS][MAX_FIELDS];
static bool g_inited = false;
static int  g_edit_g = 0, g_edit_f = 0;   // field currently being edited

static void store_init(void) {
  if (g_inited) return;
  for (int g = 0; g < N_GROUPS; g++)
    for (int f = 0; f < GROUPS[g].n; f++) {
      const Field* fl = &GROUPS[g].fields[f];
      if (fl->value) { strncpy(g_val[g][f], fl->value, sizeof(g_val[g][f]) - 1); g_val[g][f][sizeof(g_val[g][f]) - 1] = 0; }
      g_sel[g][f] = fl->sel;
    }
  g_inited = true;
}

static bool field_numeric(const Field* f) {
  if (f->type != F_VAL) return false;
  if (strcmp(f->label, "Node name") == 0) return false;
  if (strncmp(f->label, "Time source", 11) == 0) return false;  // repeater names are text
  if (strcmp(f->label, "Set time (UTC)") == 0) return false;    // "YYYY-MM-DD HH:MM" is text
  if (strcmp(f->label, "Network") == 0)  return false;          // Wi-Fi SSID
  if (strcmp(f->label, "Password") == 0) return false;          // Wi-Fi passphrase
  return true;
}

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
  store_init();
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, "Settings");
  lv_obj_t* list = lv_ui_md_scroll(scr);
  lv_obj_set_style_pad_row(list, 8, 0);
  for (int i = 0; i < N_GROUPS; i++) {
    const Group* g = &GROUPS[i];
    lv_obj_t* row = lv_ui_md_card(list);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_ui_clickable(row, SG_DEST[i]);

    lv_obj_t* ic = lv_label_create(row);
    lv_label_set_text(ic, g->icon);
    lv_obj_set_style_text_font(ic, &icons_fa, 0);
    lv_obj_set_style_text_color(ic, lv_color_hex(MD_PRIMARY), 0);
    lv_obj_set_style_margin_right(ic, 14, 0);

    lv_obj_t* mid = lv_obj_create(row);
    lv_obj_remove_flag(mid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(mid, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(mid, 0, 0); lv_obj_set_style_border_width(mid, 0, 0);
    lv_obj_set_style_pad_all(mid, 0, 0); lv_obj_set_flex_grow(mid, 1); lv_obj_set_height(mid, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(mid, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(mid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(mid, 2, 0);
    lv_obj_t* t = lv_label_create(mid);
    lv_label_set_text(t, g->title);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(t, lv_color_hex(MD_ON), 0);
    lv_obj_t* s = lv_label_create(mid);
    lv_label_set_text_fmt(s, "%d settings", g->n);
    lv_obj_set_style_text_font(s, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s, lv_color_hex(MD_MUTED), 0);

    lv_obj_t* chev = lv_label_create(row);
    lv_label_set_text(chev, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(chev, lv_color_hex(MD_MUTED), 0);
  }
}

// ---- event handlers ---------------------------------------------------------
static void on_enum_changed(lv_event_t* e) {
  int ud = (int)(intptr_t)lv_event_get_user_data(e);
  lv_obj_t* dd = (lv_obj_t*)lv_event_get_target(e);
  int g = ud >> 8, f = ud & 0xff;
  g_sel[g][f] = lv_dropdown_get_selected(dd);
  lvd_cfg_set(GROUPS[g].title, GROUPS[g].fields[f].label, NULL, g_sel[g][f]);
}
static void on_bool_changed(lv_event_t* e) {
  int ud = (int)(intptr_t)lv_event_get_user_data(e);
  lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
  int g = ud >> 8, f = ud & 0xff;
  g_sel[g][f] = lv_obj_has_state(sw, LV_STATE_CHECKED) ? 1 : 0;
  lvd_cfg_set(GROUPS[g].title, GROUPS[g].fields[f].label, NULL, g_sel[g][f]);
}
static void on_edit_clicked(lv_event_t* e) {
  int ud = (int)(intptr_t)lv_event_get_user_data(e);
  g_edit_g = ud >> 8; g_edit_f = ud & 0xff;
  if (lv_nav_cb) lv_nav_cb("edit");
}
// ---- factory reset confirm modal ---------------------------------------------
static void fr_cancel(lv_event_t* e) {
  lv_obj_del((lv_obj_t*)lv_event_get_user_data(e));   // the modal overlay
}
static void fr_confirm(lv_event_t* e) {
  (void)e;
  lv_ui_toast("Erasing...");
  lvd_factory_reset();   // formats the FS then reboots; no coming back
}
static void confirm_factory_reset(void) {
  lv_obj_t* m = lv_obj_create(lv_screen_active());    // dim overlay
  lv_obj_set_size(m, 320, 240); lv_obj_set_pos(m, 0, 0);
  lv_obj_set_style_bg_color(m, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(m, 170, 0);
  lv_obj_set_style_border_width(m, 0, 0);
  lv_obj_add_flag(m, LV_OBJ_FLAG_CLICKABLE);          // swallow taps behind the card

  lv_obj_t* card = lv_obj_create(m);
  lv_obj_set_size(card, 280, 130); lv_obj_center(card);
  lv_obj_set_style_bg_color(card, lv_color_hex(0x18202c), 0);
  lv_obj_set_style_border_color(card, lv_color_hex(UI_RED), 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_radius(card, 12, 0);

  lv_obj_t* q = lv_label_create(card);
  lv_label_set_text(q, "Factory reset?\nErases ALL contacts, channels,\nmessages and settings, then reboots.");
  lv_obj_set_style_text_font(q, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(q, lv_color_hex(UI_TEXT), 0);
  lv_obj_align(q, LV_ALIGN_TOP_MID, 0, 0);

  lv_obj_t* cancel = lv_button_create(card);
  lv_obj_set_size(cancel, 110, 36); lv_obj_align(cancel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_style_bg_color(cancel, lv_color_hex(0x2a3343), 0);
  lv_obj_add_event_cb(cancel, fr_cancel, LV_EVENT_CLICKED, m);
  lv_obj_t* cl = lv_label_create(cancel); lv_label_set_text(cl, "Cancel"); lv_obj_center(cl);

  lv_obj_t* erase = lv_button_create(card);
  lv_obj_set_size(erase, 110, 36); lv_obj_align(erase, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_obj_set_style_bg_color(erase, lv_color_hex(UI_RED), 0);
  lv_obj_add_event_cb(erase, fr_confirm, LV_EVENT_CLICKED, NULL);
  lv_obj_t* el = lv_label_create(erase); lv_label_set_text(el, "Erase"); lv_obj_center(el);
}

static void on_action_clicked(lv_event_t* e) {
  int ud = (int)(intptr_t)lv_event_get_user_data(e);
  const char* label = GROUPS[ud >> 8].fields[ud & 0xff].label;
  // actions that are UI-side (navigation / confirmation), not config commands
  if (strcmp(label, "Add channel") == 0)   { if (lv_nav_cb) lv_nav_cb("chan_add"); return; }
  if (strcmp(label, "Scan networks") == 0) { if (lv_nav_cb) lv_nav_cb("wifi_scan"); return; }
  if (strcmp(label, "Factory reset") == 0) { confirm_factory_reset(); return; }
  lvd_cfg_action(GROUPS[ud >> 8].title, label);
  if (strncmp(label, "Export", 6) == 0) lv_ui_toast("Printed to USB serial");
}

// ---- per-field widgets ------------------------------------------------------
static lv_obj_t* field_card(lv_obj_t* list, const char* label) {
  lv_obj_t* c = lv_ui_md_card(list);
  lv_obj_set_style_pad_ver(c, 10, 0);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(c, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t* l = lv_label_create(c);
  lv_label_set_text(l, label);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(MD_ON), 0);
  return c;
}

static void action_button(lv_obj_t* list, const char* label, uint32_t color, int ud) {
  lv_obj_t* b = lv_ui_md_card(list);
  lv_obj_set_height(b, 40);
  lv_obj_set_style_bg_color(b, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(b, 40, 0);
  lv_obj_set_style_border_color(b, lv_color_hex(color), 0);
  lv_obj_set_style_border_opa(b, 200, 0);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(b, on_action_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)ud);
  lv_ui_press_fx(b);
  lv_obj_t* l = lv_label_create(b);
  lv_label_set_text(l, label);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(0xffffff), 0);
  lv_obj_center(l);
}

// live rows on the Wi-Fi group screen: Status (connect progress + IP) and Ping
static lv_obj_t* s_wifi_status_lbl = NULL;
static lv_obj_t* s_wifi_ping_lbl = NULL;
static void wifi_group_refresh(void) {
  char v[68]; int sel = 0;
  if (s_wifi_status_lbl && lvd_cfg_get("Wi-Fi", "Status", v, sizeof(v), &sel))
    lv_label_set_text(s_wifi_status_lbl, v);
  if (s_wifi_ping_lbl && lvd_cfg_get("Wi-Fi", "Ping", v, sizeof(v), &sel))
    lv_label_set_text(s_wifi_ping_lbl, v);
}

void lv_settings_group_create(lv_obj_t* scr, int idx) {
  store_init();
  if (idx < 0 || idx >= N_GROUPS) idx = 0;
  const Group* g = &GROUPS[idx];
  s_wifi_status_lbl = s_wifi_ping_lbl = NULL;
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, g->title);
  lv_obj_t* list = lv_ui_md_scroll(scr);
  lv_obj_set_style_pad_row(list, 8, 0);

  for (int i = 0; i < g->n; i++) {
    const Field* f = &g->fields[i];
    int ud = (idx << 8) | i;
    // overlay live config onto the prototype default for any bound field
    { char lv[68]; int ls = g_sel[idx][i];
      if (lvd_cfg_get(g->title, f->label, lv, sizeof(lv), &ls)) {
        strncpy(g_val[idx][i], lv, sizeof(g_val[0][0]) - 1);
        g_val[idx][i][sizeof(g_val[0][0]) - 1] = 0;
        g_sel[idx][i] = ls;
      } }
    switch (f->type) {
      case F_ACTION:      action_button(list, f->label, g->color, ud); break;
      case F_ACTION_WARN: action_button(list, f->label, UI_RED,   ud); break;
      case F_BOOL: {
        lv_obj_t* c = field_card(list, f->label);
        lv_obj_t* sw = lv_switch_create(c);
        if (g_sel[idx][i]) lv_obj_add_state(sw, LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(sw, lv_color_hex(0x2a3343), LV_PART_MAIN);
        lv_obj_set_style_bg_color(sw, lv_color_hex(g->color), LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_add_event_cb(sw, on_bool_changed, LV_EVENT_VALUE_CHANGED, (void*)(intptr_t)ud);
        break;
      }
      case F_ENUM: {
        lv_obj_t* c = field_card(list, f->label);
        lv_obj_t* dd = lv_dropdown_create(c);
        lv_dropdown_set_options(dd, f->value);
        lv_dropdown_set_selected(dd, g_sel[idx][i]);
        lv_obj_set_width(dd, 124);
        lv_obj_set_style_bg_color(dd, lv_color_hex(0x18202c), 0);
        lv_obj_set_style_border_color(dd, lv_color_hex(g->color), 0);
        lv_obj_set_style_border_width(dd, 1, 0);
        lv_obj_set_style_text_color(dd, lv_color_hex(UI_TEXT), 0);
        lv_obj_set_style_text_font(dd, &lv_font_montserrat_14, 0);
        lv_obj_add_event_cb(dd, on_enum_changed, LV_EVENT_VALUE_CHANGED, (void*)(intptr_t)ud);
        break;
      }
      case F_VAL: {
        lv_obj_t* c = field_card(list, f->label);
        lv_ui_pill(c, g_val[idx][i], g->color);
        lv_ui_clickable(c, NULL);   // make the row tappable
        lv_obj_add_event_cb(c, on_edit_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)ud);
        lv_ui_press_fx(c);
        // a faint pencil hint
        lv_obj_t* pen = lv_label_create(c);
        lv_label_set_text(pen, LV_SYMBOL_EDIT);
        lv_obj_set_style_text_color(pen, lv_color_hex(UI_MUTED), 0);
        break;
      }
      case F_INFO: {
        lv_obj_t* c = field_card(list, f->label);
        lv_obj_t* v = lv_label_create(c);
        lv_label_set_text(v, g_val[idx][i]);
        lv_obj_set_style_text_font(v, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(v, lv_color_hex(UI_MUTED), 0);
        if (strcmp(g->title, "Wi-Fi") == 0) {
          if (strcmp(f->label, "Status") == 0) s_wifi_status_lbl = v;
          if (strcmp(f->label, "Ping") == 0)   s_wifi_ping_lbl = v;
        }
        break;
      }
    }
  }
  if (s_wifi_status_lbl) lv_ui_set_refresh(wifi_group_refresh);
}

// ---- keyboard editor for value fields ---------------------------------------
static lv_obj_t* s_edit_ta = NULL;

static void on_kb_ready(lv_event_t* e) {
  (void)e;
  if (s_edit_ta) {
    const char* txt = lv_textarea_get_text(s_edit_ta);
    strncpy(g_val[g_edit_g][g_edit_f], txt, sizeof(g_val[0][0]) - 1);
    g_val[g_edit_g][g_edit_f][sizeof(g_val[0][0]) - 1] = 0;
    lvd_cfg_set(GROUPS[g_edit_g].title, GROUPS[g_edit_g].fields[g_edit_f].label,
                g_val[g_edit_g][g_edit_f], 0);
  }
  if (lv_nav_cb) lv_nav_cb("back");
}
static void on_kb_cancel(lv_event_t* e) { (void)e; if (lv_nav_cb) lv_nav_cb("back"); }

void lv_settings_edit_create(lv_obj_t* scr) {
  const Field* f = &GROUPS[g_edit_g].fields[g_edit_f];
  uint32_t accent = MD_PRIMARY;
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, f->label);

  lv_obj_t* ta = lv_textarea_create(scr);
  lv_textarea_set_one_line(ta, true);
  lv_textarea_set_text(ta, g_val[g_edit_g][g_edit_f]);
  lv_obj_set_pos(ta, 8, 38);
  lv_obj_set_size(ta, 320 - 16, 34);
  lv_obj_set_style_bg_color(ta, lv_color_hex(0x18202c), 0);
  lv_obj_set_style_border_color(ta, lv_color_hex(accent), 0);
  lv_obj_set_style_border_width(ta, 1, 0);
  lv_obj_set_style_text_color(ta, lv_color_hex(UI_TEXT), 0);
  s_edit_ta = ta;
  lv_ui_kbd_focus(ta);   // route the physical keyboard into this field
  lv_obj_add_event_cb(ta, on_kb_ready, LV_EVENT_READY, NULL);   // physical Enter applies

  if (lvd_osk_enabled()) {
    lv_obj_t* kb = lv_keyboard_create(scr);
    lv_keyboard_set_mode(kb, field_numeric(f) ? LV_KEYBOARD_MODE_NUMBER : LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_set_size(kb, 320, 150);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(kb, on_kb_ready, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(kb, on_kb_cancel, LV_EVENT_CANCEL, NULL);
  }
}

// ---- Wi-Fi network picker (async scan results, strongest first) --------------
static lv_obj_t* s_wifi_list = NULL;
static int s_wifi_seen = -3;   // last rendered scan state (+count), for the 1/s refresh

static int wifi_field(const char* label) {   // field index within the Wi-Fi group
  for (int g = 0; g < N_GROUPS; g++) {
    if (strcmp(GROUPS[g].title, "Wi-Fi") != 0) continue;
    for (int f = 0; f < GROUPS[g].n; f++)
      if (strcmp(GROUPS[g].fields[f].label, label) == 0) return (g << 8) | f;
  }
  return -1;
}

static void on_wifi_net_clicked(lv_event_t* e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  char ssid[33]; int rssi = 0, secure = 0;
  if (!lvd_wifi_scan_get(i, ssid, sizeof(ssid), &rssi, &secure)) return;
  lvd_wifi_set_ssid(ssid);
  lvd_wifi_set_enabled(1);   // picking a network implies "use it"
  if (secure) {
    // hop straight into the passphrase editor (picker -> back -> edit)
    int gf = wifi_field("Password");
    if (gf >= 0) {
      g_edit_g = gf >> 8; g_edit_f = gf & 0xff;
      g_val[g_edit_g][g_edit_f][0] = 0;   // start the editor empty for a new network
      if (lv_nav_cb) { lv_nav_cb("back"); lv_nav_cb("edit"); }
    }
  } else {
    lvd_wifi_set_pass("");
    lv_ui_toast("Connecting...");
    if (lv_nav_cb) lv_nav_cb("back");
  }
}
static void on_wifi_rescan(lv_event_t* e) {
  (void)e;
  lvd_wifi_scan_start();
  s_wifi_seen = -3;   // force a repaint
}

static void wifi_scan_fill(void) {
  if (!s_wifi_list) return;
  lv_obj_clean(s_wifi_list);
  int st = lvd_wifi_scan_state();
  if (st != 2) {
    lv_obj_t* c = lv_ui_md_card(s_wifi_list);
    lv_obj_t* l = lv_label_create(c);
    lv_label_set_text(l, st == 1 ? LV_SYMBOL_REFRESH "  Scanning..." : "Scan idle");
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(UI_MUTED), 0);
    return;
  }
  // "scan again" row on top, then the results
  lv_obj_t* re = lv_ui_md_card(s_wifi_list);
  lv_obj_set_height(re, 40);
  lv_obj_add_flag(re, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(re, on_wifi_rescan, LV_EVENT_CLICKED, NULL);
  lv_ui_press_fx(re);
  lv_obj_t* rl = lv_label_create(re);
  lv_label_set_text(rl, LV_SYMBOL_REFRESH "  Scan again");
  lv_obj_set_style_text_font(rl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(rl, lv_color_hex(MD_PRIMARY), 0);
  lv_obj_center(rl);

  int n = lvd_wifi_scan_count();
  if (n == 0) {
    lv_obj_t* c = lv_ui_md_card(s_wifi_list);
    lv_obj_t* l = lv_label_create(c);
    lv_label_set_text(l, "No networks found");
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(UI_MUTED), 0);
    return;
  }
  for (int i = 0; i < n; i++) {
    char ssid[33]; int rssi = 0, secure = 0;
    if (!lvd_wifi_scan_get(i, ssid, sizeof(ssid), &rssi, &secure)) break;
    lv_obj_t* row = lv_ui_md_card(s_wifi_list);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, on_wifi_net_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    lv_ui_press_fx(row);

    lv_obj_t* name = lv_label_create(row);
    lv_label_set_text_fmt(name, LV_SYMBOL_WIFI "  %s", ssid[0] ? ssid : "(hidden)");
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    lv_obj_set_flex_grow(name, 1);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(name, lv_color_hex(MD_ON), 0);

    lv_obj_t* meta = lv_label_create(row);
    lv_label_set_text_fmt(meta, "%d dBm  %s", rssi, secure ? "sec" : "open");
    lv_obj_set_style_text_font(meta, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(meta, lv_color_hex(UI_MUTED), 0);
  }
}

static void wifi_scan_refresh(void) {   // 1/s: repaint when the scan finishes
  int st = lvd_wifi_scan_state();
  int sig = (st << 12) | (st == 2 ? lvd_wifi_scan_count() : 0);
  if (sig != s_wifi_seen) { s_wifi_seen = sig; wifi_scan_fill(); }
}

void lv_wifi_scan_create(lv_obj_t* scr) {
  store_init();
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, "Wi-Fi networks");
  s_wifi_list = lv_ui_md_scroll(scr);
  lv_obj_set_style_pad_row(s_wifi_list, 8, 0);
  if (lvd_wifi_scan_state() != 1) lvd_wifi_scan_start();
  s_wifi_seen = -3;
  wifi_scan_fill();
  lv_ui_set_refresh(wifi_scan_refresh);
}
