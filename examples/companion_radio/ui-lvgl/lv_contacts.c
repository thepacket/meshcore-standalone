// LVGL Contacts list: mirrors the Android client's Chats > Contacts sub-tab —
// an Import-contact button then Material cards (leading type icon, name, type
// subtitle). Tap a row to open the peer-details panel, which carries Message /
// Trace / Favourite plus the Share / Reset-path / Export / Remove ops. The
// firmware build enumerates live contacts via the lvd_contact_* bridge.
#include "lv_ui.h"
#include "lv_data.h"
#include <stdio.h>

void lv_chat_set_peer(const char* name);  // peer-details target (lv_chat.c / lv_diag.c)

// colour + icon per contact type (ADV_TYPE_*: 1 chat, 2 repeater, 3 room, 4 sensor)
static uint32_t type_color(int t) {
  return t == 2 ? UI_PURPLE : t == 3 ? UI_CYAN : t == 4 ? UI_ORANGE : UI_BLUE;
}
static const char* type_icon(int t) {
  return t == 2 ? ICON_REPEATERS : t == 3 ? ICON_CHAT : t == 4 ? ICON_NOISE : ICON_CONTACTS;
}

static void contact_clicked(lv_event_t* e) {
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  lvd_contact_t c;
  if (lvd_contact_get(idx, &c)) { lv_chat_set_peer(c.name); if (lv_nav_cb) lv_nav_cb("peer"); }
}

static void contact_row(lv_obj_t* list, const lvd_contact_t* ct, int idx) {
  lv_obj_t* row = lv_ui_md_card(list);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(row, contact_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)idx);

  lv_obj_t* ic = lv_label_create(row);
  lv_label_set_text(ic, type_icon(ct->type));
  lv_obj_set_style_text_font(ic, &icons_fa, 0);
  lv_obj_set_style_text_color(ic, lv_color_hex(type_color(ct->type)), 0);
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
  lv_label_set_text(ty, ct->subtitle);
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

// live-refresh state (valid only while the Contacts screen is active)
static lv_obj_t* s_list = NULL;
static int       s_last = -1;

static void contacts_fill(lv_obj_t* list) {
  lv_obj_t* head = lv_obj_create(list);   // left-aligned Import button row
  lv_obj_remove_flag(head, LV_OBJ_FLAG_SCROLLABLE); lv_obj_remove_flag(head, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_width(head, lv_pct(100)); lv_obj_set_height(head, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(head, 0, 0); lv_obj_set_style_border_width(head, 0, 0);
  lv_obj_set_style_pad_all(head, 0, 0);
  lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(head, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  outlined_btn(head, LV_SYMBOL_DOWNLOAD "  Import contact");

  int n = lvd_contact_count();
  s_last = n;
  if (n <= 0) {
    lv_obj_t* hint = lv_label_create(list);
    lv_label_set_text(hint, "No contacts yet");
    lv_obj_set_style_text_color(hint, lv_color_hex(MD_MUTED), 0);
    return;
  }
  lvd_contact_t c;
  for (int i = 0; i < n; i++) if (lvd_contact_get(i, &c)) contact_row(list, &c, i);
}

// rebuild only when the contact count changes (e.g. a new node was added)
static void contacts_tick(void) {
  if (s_list && lvd_contact_count() != s_last) {
    lv_obj_clean(s_list);
    contacts_fill(s_list);
  }
}

void lv_contacts_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, "Contacts");
  s_list = lv_ui_md_scroll(scr);
  contacts_fill(s_list);
  lv_ui_set_refresh(contacts_tick);
}
