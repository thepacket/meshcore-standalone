// LVGL "Scopes" management (called regions in the bridge/backend, to keep clear
// of the flood-scope symbols). A scope is a named snapshot of the radio preset +
// contacts + channels stored under /meshcore/regions/<name>/ on the SD card.
// This screen lists them, creates a new one from the current live state, selects
// one (saves the active scope, then switches live -- no reboot), and deletes a
// non-active scope. Bound to the firmware via the lvd_region_* bridge.
#include "lv_ui.h"
#include "lv_data.h"
#include <stdio.h>
#include <string.h>

static int  s_region_sel = -1;         // scope index selected for the detail screen
static char s_region_name[64] = "";    // its name (captured on row click; list may change)

// ============================ scope list ============================
static void region_clicked(lv_event_t* e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  int active = 0;
  if (!lvd_region_get(i, s_region_name, sizeof(s_region_name), &active)) return;
  s_region_sel = i;
  if (lv_nav_cb) lv_nav_cb("region_detail");
}
static void add_clicked(lv_event_t* e) { (void)e; if (lv_nav_cb) lv_nav_cb("region_add"); }

static void region_row(lv_obj_t* list, int i, const char* name, int active) {
  lv_obj_t* row = lv_ui_md_card(list);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(row, region_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)i);
  lv_ui_press_fx(row);

  lv_obj_t* mid = lv_obj_create(row);
  lv_obj_remove_flag(mid, LV_OBJ_FLAG_SCROLLABLE); lv_obj_remove_flag(mid, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(mid, 0, 0); lv_obj_set_style_border_width(mid, 0, 0);
  lv_obj_set_style_pad_all(mid, 0, 0); lv_obj_set_flex_grow(mid, 1); lv_obj_set_height(mid, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(mid, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(mid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(mid, 2, 0);
  lv_obj_t* nm = lv_label_create(mid);
  lv_label_set_text(nm, name);
  lv_obj_set_style_text_font(nm, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(nm, lv_color_hex(MD_ON), 0);
  lv_obj_t* sb = lv_label_create(mid);
  lv_label_set_text(sb, active ? "Active scope" : "Tap to select");
  lv_obj_set_style_text_font(sb, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(sb, lv_color_hex(active ? MD_PRIMARY : MD_MUTED), 0);

  if (active) {
    lv_obj_t* dot = lv_label_create(row);
    lv_label_set_text(dot, LV_SYMBOL_OK);
    lv_obj_set_style_text_color(dot, lv_color_hex(MD_PRIMARY), 0);
  }
  lv_obj_t* chev = lv_label_create(row);
  lv_label_set_text(chev, LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_color(chev, lv_color_hex(MD_MUTED), 0);
}

void lv_regions_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, "Scopes");

  // add button
  lv_obj_t* addb = lv_ui_card(scr, 12, 40, 320 - 24, 30);
  lv_obj_set_style_pad_all(addb, 0, 0); lv_obj_set_style_radius(addb, 15, 0);
  lv_obj_set_style_bg_color(addb, lv_color_hex(UI_CYAN), 0); lv_obj_set_style_bg_opa(addb, 44, 0);
  lv_obj_set_style_border_color(addb, lv_color_hex(UI_CYAN), 0); lv_obj_set_style_border_opa(addb, 200, 0);
  lv_obj_add_flag(addb, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(addb, add_clicked, LV_EVENT_CLICKED, NULL);
  lv_ui_press_fx(addb);
  lv_obj_t* al = lv_label_create(addb);
  lv_label_set_text(al, LV_SYMBOL_PLUS "  New scope (from current setup)");
  lv_obj_set_style_text_font(al, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(al, lv_color_hex(0xffffff), 0);
  lv_obj_center(al);

  // scope list
  lv_obj_t* list = lv_obj_create(scr);
  lv_obj_set_pos(list, 0, 78); lv_obj_set_size(list, 320, 240 - 78);
  lv_obj_set_style_bg_opa(list, 0, 0); lv_obj_set_style_border_width(list, 0, 0);
  lv_obj_set_style_pad_all(list, 12, 0);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_row(list, 8, 0);

  int n = lvd_region_count();
  if (n < 0) {
    lv_obj_t* h = lv_label_create(list);
    lv_label_set_text(h, "Insert an SD card to use scopes.");
    lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP); lv_obj_set_width(h, lv_pct(100));
    lv_obj_set_style_text_color(h, lv_color_hex(MD_MUTED), 0);
    return;
  }
  if (n == 0) {
    lv_obj_t* h = lv_label_create(list);
    lv_label_set_text(h, "No scopes yet.\n\nA scope stores a radio preset with its own contacts and "
                         "channels. Create one to snapshot your current setup, then switch between "
                         "scopes without losing either side's data.");
    lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP); lv_obj_set_width(h, lv_pct(100));
    lv_obj_set_style_text_color(h, lv_color_hex(MD_MUTED), 0);
    return;
  }
  for (int i = 0; i < n; i++) {
    char name[64]; int active = 0;
    if (lvd_region_get(i, name, sizeof(name), &active)) region_row(list, i, name, active);
  }
}

// ============================ scope detail ============================
static void select_clicked(lv_event_t* e) {
  (void)e;
  int r = lvd_region_select(s_region_sel);
  if (r == 1) {
    char t[80]; snprintf(t, sizeof(t), "Switched to \"%s\"", s_region_name);
    lv_ui_toast(t);
    if (lv_nav_cb) lv_nav_cb("back");
  } else if (r == -2) {
    lv_ui_toast("Scope snapshot is empty - not switching");   // current data left untouched
  } else {
    lv_ui_toast("Switch failed (no card?)");
  }
}
static void delete_clicked(lv_event_t* e) {
  (void)e;
  int r = lvd_region_delete(s_region_sel);
  if (r == 1) { lv_ui_toast("Scope deleted"); if (lv_nav_cb) lv_nav_cb("back"); }
  else if (r == -1) lv_ui_toast("Can't delete the active scope");
  else lv_ui_toast("Delete failed");
}

static lv_obj_t* action_btn(lv_obj_t* parent, const char* text, uint32_t col) {
  lv_obj_t* b = lv_ui_card(parent, -1, 0, 0, 0);
  lv_obj_set_width(b, lv_pct(100)); lv_obj_set_height(b, 40); lv_obj_set_style_min_height(b, 0, 0);
  lv_obj_set_style_pad_all(b, 0, 0); lv_obj_set_style_radius(b, 10, 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(col), 0); lv_obj_set_style_bg_opa(b, 50, 0);
  lv_obj_set_style_border_color(b, lv_color_hex(col), 0); lv_obj_set_style_border_opa(b, 210, 0);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_ui_press_fx(b);
  lv_obj_t* l = lv_label_create(b);
  lv_label_set_text(l, text);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(0xffffff), 0);
  lv_obj_center(l);
  return b;
}

void lv_region_detail_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  int active = 0;
  // re-resolve active state (it may have changed); fall back to the captured name
  lvd_region_get(s_region_sel, s_region_name, sizeof(s_region_name), &active);
  lv_ui_md_topbar(scr, s_region_name[0] ? s_region_name : "Scope");

  lv_obj_t* list = lv_ui_md_scroll(scr);
  lv_obj_set_style_pad_row(list, 10, 0);

  lv_ui_pill(list, active ? "Active scope" : "Saved scope", active ? UI_GREEN : UI_PURPLE);

  lv_obj_t* info = lv_label_create(list);
  lv_label_set_text(info, active
    ? "This is the scope you are using now. Its radio preset, contacts and channels are live."
    : "Selecting this scope saves your current scope first, then switches the radio preset, "
      "contacts and channels to this one -- live, no reboot.");
  lv_label_set_long_mode(info, LV_LABEL_LONG_WRAP); lv_obj_set_width(info, lv_pct(100));
  lv_obj_set_style_text_font(info, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(info, lv_color_hex(MD_MUTED), 0);

  if (!active) {
    lv_obj_t* sel = action_btn(list, LV_SYMBOL_OK "  Select this scope", UI_GREEN);
    lv_obj_add_event_cb(sel, select_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t* del = action_btn(list, LV_SYMBOL_TRASH "  Delete scope", UI_RED);
    lv_obj_add_event_cb(del, delete_clicked, LV_EVENT_CLICKED, NULL);
  }
}

// ============================ new scope ============================
// One text field for the name; the new scope snapshots the current live setup
// (radio preset + contacts + channels) and becomes active. Physical keyboard
// types into the field; the on-screen keyboard opens on tap when enabled.
static lv_obj_t* s_name_ta = NULL;
static lv_obj_t* s_name_lbl = NULL, * s_hint_lbl = NULL;
static lv_obj_t* s_kb = NULL;

static void kb_close(lv_event_t* e) {
  (void)e;
  if (!s_kb) return;
  lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
  if (s_name_lbl) lv_obj_clear_flag(s_name_lbl, LV_OBJ_FLAG_HIDDEN);
  if (s_hint_lbl) lv_obj_clear_flag(s_hint_lbl, LV_OBJ_FLAG_HIDDEN);
  if (s_name_ta) lv_obj_set_pos(s_name_ta, 8, 56);
}
static void field_clicked(lv_event_t* e) {
  if (!s_kb) return;
  lv_obj_t* ta = (lv_obj_t*)lv_event_get_target(e);
  lv_keyboard_set_textarea(s_kb, ta);
  lv_ui_kbd_focus(ta);
  if (s_name_lbl) lv_obj_add_flag(s_name_lbl, LV_OBJ_FLAG_HIDDEN);
  if (s_hint_lbl) lv_obj_add_flag(s_hint_lbl, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_pos(ta, 8, 48);
  lv_obj_clear_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
}

static void create_clicked(lv_event_t* e) {
  (void)e;
  const char* nm = s_name_ta ? lv_textarea_get_text(s_name_ta) : "";
  int r = lvd_region_create(nm);
  if (r == 1)       { lv_ui_toast("Scope created"); if (lv_nav_cb) lv_nav_cb("back"); }
  else if (r == -1)  lv_ui_toast("A scope with that name exists");
  else if (r == -2)  lv_ui_toast("Enter a valid name");
  else               lv_ui_toast("Create failed (no card?)");
}

void lv_region_add_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, "New scope");

  lv_obj_t* nlbl = lv_label_create(scr);
  lv_label_set_text(nlbl, "Name");
  lv_obj_set_style_text_font(nlbl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(nlbl, lv_color_hex(MD_MUTED), 0);
  lv_obj_set_pos(nlbl, 10, 40);
  s_name_lbl = nlbl;

  s_name_ta = lv_textarea_create(scr);
  lv_textarea_set_one_line(s_name_ta, true);
  lv_textarea_set_placeholder_text(s_name_ta, "e.g. EU 868, Home, Travel");
  lv_obj_set_pos(s_name_ta, 8, 56); lv_obj_set_size(s_name_ta, 320 - 16, 34);
  lv_obj_set_style_bg_color(s_name_ta, lv_color_hex(0x18202c), 0);
  lv_obj_set_style_border_color(s_name_ta, lv_color_hex(MD_PRIMARY), 0);
  lv_obj_set_style_border_width(s_name_ta, 1, 0);
  lv_obj_set_style_text_color(s_name_ta, lv_color_hex(UI_TEXT), 0);
  lv_group_add_obj(lv_ui_kbd_group(), s_name_ta);

  lv_obj_t* hint = lv_label_create(scr);
  lv_label_set_text(hint, "Snapshots your current radio preset, contacts and channels into a new "
                          "scope and switches to it. Re-tune the radio afterward for the new network.");
  lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP); lv_obj_set_width(hint, 320 - 20);
  lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(hint, lv_color_hex(MD_MUTED), 0);
  lv_obj_set_pos(hint, 10, 100);
  s_hint_lbl = hint;

  lv_obj_t* mk = lv_ui_card(scr, 8, 176, 320 - 16, 38);
  lv_obj_set_style_pad_all(mk, 0, 0); lv_obj_set_style_radius(mk, 10, 0);
  lv_obj_set_style_bg_color(mk, lv_color_hex(UI_GREEN), 0); lv_obj_set_style_bg_opa(mk, 60, 0);
  lv_obj_set_style_border_color(mk, lv_color_hex(UI_GREEN), 0); lv_obj_set_style_border_opa(mk, 220, 0);
  lv_obj_add_flag(mk, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(mk, create_clicked, LV_EVENT_CLICKED, NULL);
  lv_ui_press_fx(mk);
  lv_obj_t* ml = lv_label_create(mk);
  lv_label_set_text(ml, LV_SYMBOL_OK "  Create scope");
  lv_obj_set_style_text_font(ml, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(ml, lv_color_hex(0xffffff), 0);
  lv_obj_center(ml);

  s_kb = NULL;
  if (lvd_osk_enabled()) {
    s_kb = lv_keyboard_create(scr);
    lv_keyboard_set_mode(s_kb, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_set_size(s_kb, 320, 150);
    lv_obj_align(s_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_kb, kb_close, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(s_kb, kb_close, LV_EVENT_CANCEL, NULL);
    lv_obj_add_event_cb(s_name_ta, field_clicked, LV_EVENT_CLICKED, NULL);
  }
  lv_ui_kbd_focus(s_name_ta);
}
