// LVGL channel management: list group channels, view/share a channel's key as a
// QR, add (join or create) a channel, and remove non-Public channels. Bound to
// the firmware via the lvd_chan_* bridge (getChannel/addChannel/uiRemoveChannel).
#include "lv_ui.h"
#include "lv_data.h"
#include <stdio.h>
#include <string.h>

static int s_chan_sel = -1;   // channel index selected in the list (for detail/remove)

// label-on-top / value-below block; value wraps to its own line
static void kv_block(lv_obj_t* list, const char* k, const char* v) {
  lv_obj_t* c = lv_ui_md_card(list);
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
  lv_label_set_long_mode(vl, LV_LABEL_LONG_WRAP); lv_obj_set_width(vl, lv_pct(100));
}

// ============================ channel list ============================
static void chan_clicked(lv_event_t* e) {
  s_chan_sel = (int)(intptr_t)lv_event_get_user_data(e);
  if (lv_nav_cb) lv_nav_cb("chan_detail");
}
static void add_clicked(lv_event_t* e) { (void)e; if (lv_nav_cb) lv_nav_cb("chan_add"); }

static void chan_row(lv_obj_t* list, int i, const lvd_chan_t* c) {
  lv_obj_t* row = lv_ui_md_card(list);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(row, chan_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)i);
  lv_ui_press_fx(row);

  lv_obj_t* mid = lv_obj_create(row);
  lv_obj_remove_flag(mid, LV_OBJ_FLAG_SCROLLABLE); lv_obj_remove_flag(mid, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(mid, 0, 0); lv_obj_set_style_border_width(mid, 0, 0);
  lv_obj_set_style_pad_all(mid, 0, 0); lv_obj_set_flex_grow(mid, 1); lv_obj_set_height(mid, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(mid, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(mid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(mid, 2, 0);
  lv_obj_t* nm = lv_label_create(mid);
  lv_label_set_text(nm, c->name);
  lv_obj_set_style_text_font(nm, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(nm, lv_color_hex(MD_ON), 0);
  lv_obj_t* sb = lv_label_create(mid);
  lv_label_set_text(sb, c->info);
  lv_obj_set_style_text_font(sb, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(sb, lv_color_hex(MD_MUTED), 0);

  lv_obj_t* chev = lv_label_create(row);
  lv_label_set_text(chev, LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_color(chev, lv_color_hex(MD_MUTED), 0);
}

void lv_channels_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, "Channels");

  // add button
  lv_obj_t* addb = lv_ui_card(scr, 12, 40, 320 - 24, 30);
  lv_obj_set_style_pad_all(addb, 0, 0); lv_obj_set_style_radius(addb, 15, 0);
  lv_obj_set_style_bg_color(addb, lv_color_hex(UI_CYAN), 0); lv_obj_set_style_bg_opa(addb, 44, 0);
  lv_obj_set_style_border_color(addb, lv_color_hex(UI_CYAN), 0); lv_obj_set_style_border_opa(addb, 200, 0);
  lv_obj_add_flag(addb, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(addb, add_clicked, LV_EVENT_CLICKED, NULL);
  lv_ui_press_fx(addb);
  lv_obj_t* al = lv_label_create(addb);
  lv_label_set_text(al, LV_SYMBOL_PLUS "  Add channel");
  lv_obj_set_style_text_font(al, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(al, lv_color_hex(0xffffff), 0);
  lv_obj_center(al);

  // channel list
  lv_obj_t* list = lv_obj_create(scr);
  lv_obj_set_pos(list, 0, 78); lv_obj_set_size(list, 320, 240 - 78);
  lv_obj_set_style_bg_opa(list, 0, 0); lv_obj_set_style_border_width(list, 0, 0);
  lv_obj_set_style_pad_all(list, 12, 0);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_row(list, 8, 0);
  int n = lvd_chan_count();
  if (n <= 0) {
    lv_obj_t* h = lv_label_create(list);
    lv_label_set_text(h, "No channels.");
    lv_obj_set_style_text_color(h, lv_color_hex(MD_MUTED), 0);
    return;
  }
  lvd_chan_t c;
  for (int i = 0; i < n; i++) if (lvd_chan_get(i, &c)) chan_row(list, i, &c);
}

// ============================ channel detail ============================
static void remove_clicked(lv_event_t* e) {
  (void)e;
  if (lvd_chan_remove(s_chan_sel)) { lv_ui_toast("Channel removed"); if (lv_nav_cb) lv_nav_cb("back"); }
  else lv_ui_toast("Cannot remove");
}

void lv_chan_detail_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lvd_chan_t c;
  if (!lvd_chan_get(s_chan_sel, &c)) { if (lv_nav_cb) lv_nav_cb("back"); return; }
  lv_ui_md_topbar(scr, c.name);

  lv_obj_t* list = lv_ui_md_scroll(scr);
  lv_obj_set_style_pad_row(list, 8, 0);

  lv_ui_pill(list, c.info, c.is_public ? UI_CYAN : UI_PURPLE);

  const char* psk = lvd_chan_psk(s_chan_sel);
  kv_block(list, "Pre-shared key (others enter this to join)", psk);

  // QR of the key for easy sharing
  lv_obj_t* qr = lv_qrcode_create(list);
  lv_qrcode_set_size(qr, 150);
  lv_qrcode_set_dark_color(qr, lv_color_black());
  lv_qrcode_set_light_color(qr, lv_color_white());
  lv_qrcode_update(qr, psk, (uint32_t)strlen(psk));
  lv_obj_set_style_bg_color(qr, lv_color_white(), 0); lv_obj_set_style_bg_opa(qr, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(qr, 6, 0);
  lv_obj_set_style_margin_top(qr, 4, 0);

  if (!c.is_public) {
    lv_obj_t* rm = lv_ui_card(list, -1, 0, 0, 0);
    lv_obj_set_width(rm, lv_pct(100)); lv_obj_set_height(rm, 36); lv_obj_set_style_min_height(rm, 0, 0);
    lv_obj_set_style_pad_all(rm, 0, 0);
    lv_obj_set_style_bg_color(rm, lv_color_hex(UI_RED), 0); lv_obj_set_style_bg_opa(rm, 44, 0);
    lv_obj_set_style_border_color(rm, lv_color_hex(UI_RED), 0); lv_obj_set_style_border_opa(rm, 200, 0);
    lv_obj_add_flag(rm, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(rm, remove_clicked, LV_EVENT_CLICKED, NULL);
    lv_ui_press_fx(rm);
    lv_obj_t* rl = lv_label_create(rm);
    lv_label_set_text(rl, LV_SYMBOL_TRASH "  Remove channel");
    lv_obj_set_style_text_color(rl, lv_color_hex(0xffffff), 0);
    lv_obj_center(rl);
  }
}

// ============================ add channel ============================
// Two fields (name + key). The key is pre-filled with a fresh random base64 key
// so creating a new channel is one step; to join, paste the shared key instead.
// Names starting with '#' are hashtag channels: the key is derived from the
// name (first 16 bytes of sha256), so everyone typing "#topic" gets the same
// channel. Typed via the physical keyboard (tap a field to focus it).
static lv_obj_t* s_name_ta = NULL;
static lv_obj_t* s_psk_ta  = NULL;
static lv_obj_t* s_name_lbl = NULL, * s_key_lbl = NULL;
static lv_obj_t* s_kb = NULL;      // on-screen keyboard (only when enabled in settings)
static char s_rand_psk[50];   // the random-key preset, restored when name isn't a '#hashtag'

static void name_changed(lv_event_t* e) {
  (void)e;
  if (!s_name_ta || !s_psk_ta) return;
  const char* nm = lv_textarea_get_text(s_name_ta);
  if (nm && nm[0] == '#' && nm[1]) lv_textarea_set_text(s_psk_ta, lvd_chan_hashtag_psk(nm));
  else lv_textarea_set_text(s_psk_ta, s_rand_psk);
}

// The 150px keyboard covers everything below the topbar except one field, so
// while it is open only the active field is shown, parked just above the keys.
// Closing the keyboard (checkmark or hide key) restores the normal layout.
static void kb_close(lv_event_t* e) {
  (void)e;
  if (!s_kb) return;
  lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
  if (s_name_lbl) lv_obj_clear_flag(s_name_lbl, LV_OBJ_FLAG_HIDDEN);
  if (s_key_lbl)  lv_obj_clear_flag(s_key_lbl, LV_OBJ_FLAG_HIDDEN);
  if (s_name_ta) { lv_obj_set_pos(s_name_ta, 8, 56);  lv_obj_clear_flag(s_name_ta, LV_OBJ_FLAG_HIDDEN); }
  if (s_psk_ta)  { lv_obj_set_pos(s_psk_ta, 8, 130);  lv_obj_clear_flag(s_psk_ta, LV_OBJ_FLAG_HIDDEN); }
}
static void field_clicked(lv_event_t* e) {
  if (!s_kb) return;
  lv_obj_t* ta = (lv_obj_t*)lv_event_get_target(e);
  lv_keyboard_set_textarea(s_kb, ta);
  lv_ui_kbd_focus(ta);   // physical keyboard follows too
  lv_obj_t* other = (ta == s_name_ta) ? s_psk_ta : s_name_ta;
  if (other) lv_obj_add_flag(other, LV_OBJ_FLAG_HIDDEN);
  if (s_name_lbl) lv_obj_add_flag(s_name_lbl, LV_OBJ_FLAG_HIDDEN);
  if (s_key_lbl)  lv_obj_add_flag(s_key_lbl, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_pos(ta, 8, 48);
  lv_obj_clear_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t* add_field(lv_obj_t* scr, int y, const char* placeholder, const char* preset) {
  lv_obj_t* ta = lv_textarea_create(scr);
  lv_textarea_set_one_line(ta, true);
  lv_textarea_set_placeholder_text(ta, placeholder);
  if (preset) lv_textarea_set_text(ta, preset);
  lv_obj_set_pos(ta, 8, y); lv_obj_set_size(ta, 320 - 16, 34);
  lv_obj_set_style_bg_color(ta, lv_color_hex(0x18202c), 0);
  lv_obj_set_style_border_color(ta, lv_color_hex(MD_PRIMARY), 0);
  lv_obj_set_style_border_width(ta, 1, 0);
  lv_obj_set_style_text_color(ta, lv_color_hex(UI_TEXT), 0);
  lv_group_add_obj(lv_ui_kbd_group(), ta);   // physical keyboard types into the focused field
  return ta;
}

static void create_clicked(lv_event_t* e) {
  (void)e;
  const char* nm  = s_name_ta ? lv_textarea_get_text(s_name_ta) : "";
  const char* psk = s_psk_ta  ? lv_textarea_get_text(s_psk_ta)  : "";
  if (lvd_chan_add(nm, psk)) { lv_ui_toast("Channel added"); if (lv_nav_cb) lv_nav_cb("back"); }
  else lv_ui_toast("Invalid name or key");
}

void lv_chan_add_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, "Add channel");

  lv_obj_t* nlbl = lv_label_create(scr);
  lv_label_set_text(nlbl, "Name");
  lv_obj_set_style_text_font(nlbl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(nlbl, lv_color_hex(MD_MUTED), 0);
  lv_obj_set_pos(nlbl, 10, 40);
  s_name_lbl = nlbl;
  s_name_ta = add_field(scr, 56, "Channel name (#name = public hashtag)", NULL);
  lv_obj_add_event_cb(s_name_ta, name_changed, LV_EVENT_VALUE_CHANGED, NULL);

  lv_obj_t* klbl = lv_label_create(scr);
  lv_label_set_text(klbl, "Key (base64) - random by default; paste a shared key to join. '#name' derives it from the name");
  lv_label_set_long_mode(klbl, LV_LABEL_LONG_WRAP); lv_obj_set_width(klbl, 320 - 20);
  lv_obj_set_style_text_font(klbl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(klbl, lv_color_hex(MD_MUTED), 0);
  lv_obj_set_pos(klbl, 10, 98);
  s_key_lbl = klbl;
  strncpy(s_rand_psk, lvd_chan_new_psk(), sizeof(s_rand_psk) - 1);
  s_rand_psk[sizeof(s_rand_psk) - 1] = 0;
  s_psk_ta = add_field(scr, 130, "Key (base64)", s_rand_psk);

  lv_obj_t* mk = lv_ui_card(scr, 8, 176, 320 - 16, 38);
  lv_obj_set_style_pad_all(mk, 0, 0); lv_obj_set_style_radius(mk, 10, 0);
  lv_obj_set_style_bg_color(mk, lv_color_hex(UI_GREEN), 0); lv_obj_set_style_bg_opa(mk, 60, 0);
  lv_obj_set_style_border_color(mk, lv_color_hex(UI_GREEN), 0); lv_obj_set_style_border_opa(mk, 220, 0);
  lv_obj_add_flag(mk, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(mk, create_clicked, LV_EVENT_CLICKED, NULL);
  lv_ui_press_fx(mk);
  lv_obj_t* ml = lv_label_create(mk);
  lv_label_set_text(ml, LV_SYMBOL_OK "  Create / Join");
  lv_obj_set_style_text_font(ml, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(ml, lv_color_hex(0xffffff), 0);
  lv_obj_center(ml);

  s_kb = NULL;
  if (lvd_osk_enabled()) {
    s_kb = lv_keyboard_create(scr);
    lv_keyboard_set_mode(s_kb, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_set_size(s_kb, 320, 150);
    lv_obj_align(s_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);   // opens when a field is tapped
    lv_obj_add_event_cb(s_kb, kb_close, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(s_kb, kb_close, LV_EVENT_CANCEL, NULL);
    lv_obj_add_event_cb(s_name_ta, field_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(s_psk_ta, field_clicked, LV_EVENT_CLICKED, NULL);
  }

  lv_ui_kbd_focus(s_name_ta);   // start in the name field
}
