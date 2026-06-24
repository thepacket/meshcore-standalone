// LVGL Contacts list: mirrors the Android client's Chats > Contacts sub-tab —
// an Import-contact button then Material cards (leading type icon, name, type
// subtitle). Tap a row to open the peer-details panel, which carries Message /
// Trace / Favourite plus the Share / Reset-path / Export / Remove ops. Prototype
// data; the firmware build enumerates live contacts.
#include "lv_ui.h"
#include <stdio.h>

void lv_chat_set_peer(const char* name);  // peer-details target (lv_chat.c / lv_diag.c)

typedef struct { const char* name; const char* type; uint32_t color; const char* icon; } Contact;

static const Contact CONTACTS[] = {
  {"Andy-Mobile",  "Chat / direct / 9.0 dB", UI_BLUE,   ICON_CONTACTS},
  {"Field-Team",   "Room / 2 hops",          UI_CYAN,   ICON_CHAT},
  {"GW-Hertford",  "Repeater / favourite",   UI_PURPLE, ICON_REPEATERS},
  {"Hilltop-Relay","Repeater / 2 hops",      UI_PURPLE, ICON_REPEATERS},
  {"Sensor-Barn",  "Sensor / telemetry",     UI_ORANGE, ICON_NOISE},
};
#define N_CONTACTS ((int)(sizeof(CONTACTS)/sizeof(CONTACTS[0])))

static void contact_clicked(lv_event_t* e) {
  const char* n = (const char*)lv_event_get_user_data(e);
  lv_chat_set_peer(n);
  if (lv_nav_cb) lv_nav_cb("peer");
}

static void contact_row(lv_obj_t* list, const Contact* ct) {
  lv_obj_t* row = lv_ui_md_card(list);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(row, contact_clicked, LV_EVENT_CLICKED, (void*)ct->name);

  lv_obj_t* ic = lv_label_create(row);
  lv_label_set_text(ic, ct->icon);
  lv_obj_set_style_text_font(ic, &icons_fa, 0);
  lv_obj_set_style_text_color(ic, lv_color_hex(ct->color), 0);
  lv_obj_set_style_margin_right(ic, 14, 0);

  lv_obj_t* mid = lv_obj_create(row);
  lv_obj_remove_flag(mid, LV_OBJ_FLAG_SCROLLABLE); lv_obj_remove_flag(mid, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(mid, 0, 0); lv_obj_set_style_border_width(mid, 0, 0);
  lv_obj_set_style_pad_all(mid, 0, 0); lv_obj_set_flex_grow(mid, 1); lv_obj_set_height(mid, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(mid, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(mid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(mid, 2, 0);
  lv_obj_t* nm = lv_label_create(mid);
  lv_label_set_text(nm, ct->name);
  lv_obj_set_style_text_font(nm, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(nm, lv_color_hex(MD_ON), 0);
  lv_obj_t* ty = lv_label_create(mid);
  lv_label_set_text(ty, ct->type);
  lv_obj_set_style_text_font(ty, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(ty, lv_color_hex(MD_MUTED), 0);

  lv_obj_t* chev = lv_label_create(row);
  lv_label_set_text(chev, LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_color(chev, lv_color_hex(MD_MUTED), 0);
}

// outlined button (M3 OutlinedButton look) used for the Import action
static void outlined_btn(lv_obj_t* parent, const char* label) {
  lv_obj_t* b = lv_obj_create(parent);
  lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_width(b, LV_SIZE_CONTENT); lv_obj_set_height(b, 34);
  lv_obj_set_style_radius(b, 17, 0);
  lv_obj_set_style_bg_opa(b, 0, 0);
  lv_obj_set_style_border_color(b, lv_color_hex(MD_PRIMARY), 0);
  lv_obj_set_style_border_opa(b, 180, 0);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_set_style_pad_hor(b, 16, 0);
  lv_ui_clickable(b, NULL);
  lv_obj_t* l = lv_label_create(b);
  lv_label_set_text(l, label);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(MD_PRIMARY), 0);
  lv_obj_center(l);
}

void lv_contacts_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, "Contacts");
  lv_obj_t* list = lv_ui_md_scroll(scr);

  lv_obj_t* head = lv_obj_create(list);   // left-aligned Import button row
  lv_obj_remove_flag(head, LV_OBJ_FLAG_SCROLLABLE); lv_obj_remove_flag(head, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_width(head, lv_pct(100)); lv_obj_set_height(head, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(head, 0, 0); lv_obj_set_style_border_width(head, 0, 0);
  lv_obj_set_style_pad_all(head, 0, 0);
  lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(head, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  outlined_btn(head, LV_SYMBOL_DOWNLOAD "  Import contact");

  for (int i = 0; i < N_CONTACTS; i++) contact_row(list, &CONTACTS[i]);
}
