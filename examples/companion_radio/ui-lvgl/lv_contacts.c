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
// short text tag instead of an icon-font glyph -- the 22px icons_fa glyph is the
// priciest thing to rasterise per row and tanked scroll FPS; a montserrat text
// pill (like the packet monitor uses) renders far cheaper.
static const char* type_tag(int t) {
  return t == 2 ? "RPT" : t == 3 ? "RM" : t == 4 ? "SNS" : "CHAT";
}

// Open on a plain tap (or long-press). Safe now because the list no longer
// touch-scrolls -- scrolling is done with the on-screen arrows, so the finger
// is never dragging across a row.
static void contact_clicked(lv_event_t* e) {
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  lvd_contact_t c;
  if (lvd_contact_get(idx, &c)) { lv_chat_set_peer(c.name); if (lv_nav_cb) lv_nav_cb("peer"); }
}

// Lightweight row for the (possibly 100+) contacts list: fixed height, NO flex
// layout and NO press-fx (both churn during a scroll drag and starve the touch
// gesture detector, causing accidental row taps). Children are manually aligned.
static void contact_row(lv_obj_t* list, const lvd_contact_t* ct, int idx) {
  lv_obj_t* row = lv_ui_md_card(list);
  lv_obj_set_layout(row, LV_LAYOUT_NONE);      // manual positioning, no per-row flex
  lv_obj_set_size(row, lv_pct(100), 38);
  lv_obj_set_pos(row, 0, 4 + idx * 42);        // manual y: list has no flex, scroll is pure offset
  lv_obj_set_style_pad_all(row, 8, 0);
  lv_obj_set_style_radius(row, 0, 0);          // square corners: no per-frame AA corner mask
  if (ct->fav) {                               // favourites: faint blue wash (no extra objects/glyphs -> zero scroll cost)
    lv_obj_set_style_bg_color(row, lv_color_hex(UI_BLUE), 0);
    lv_obj_set_style_bg_opa(row, 38, 0);
  }
  lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(row, contact_clicked, LV_EVENT_LONG_PRESSED, (void*)(intptr_t)idx);   // open on hold only (a drag can't long-press)

  lv_obj_t* pill = lv_ui_pill(row, type_tag(ct->type), type_color(ct->type));
  lv_obj_align(pill, LV_ALIGN_LEFT_MID, 0, 0);

  lv_obj_t* nm = lv_label_create(row);          // single line: just the name, centered
  lv_label_set_text(nm, ct->name);
  lv_obj_set_style_text_font(nm, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(nm, lv_color_hex(MD_ON), 0);
  lv_obj_align(nm, LV_ALIGN_LEFT_MID, 60, 0);
}

// outlined button (M3 OutlinedButton look) used for the Import action
// live-refresh state (valid only while the Contacts screen is active)
static lv_obj_t* s_list = NULL;
static int       s_last = -1;

static void contacts_fill(lv_obj_t* list);   // fwd

static void contacts_fill(lv_obj_t* list) {
  int n = lvd_contact_count();
  s_last = n;
  if (n <= 0) {
    const char* f = lvd_contact_filter();
    lv_obj_t* hint = lv_label_create(list);
    lv_label_set_text(hint, lvd_contact_fav_only() ? "No favourites yet" :
                            (f && f[0]) ? "No contacts match" : "No contacts yet");
    lv_obj_set_style_text_color(hint, lv_color_hex(MD_MUTED), 0);
    return;
  }
  lvd_contact_t c;
  for (int i = 0; i < n; i++) if (lvd_contact_get(i, &c)) contact_row(list, &c, i);
}

// ---- search keyboard screen ------------------------------------------------
static lv_obj_t* s_search_ta = NULL;
static void search_ready(lv_event_t* e) {
  (void)e;
  if (s_search_ta) lvd_contact_set_filter(lv_textarea_get_text(s_search_ta));
  if (lv_nav_cb) lv_nav_cb("back");
}
static void search_cancel(lv_event_t* e) { (void)e; if (lv_nav_cb) lv_nav_cb("back"); }

void lv_contact_search_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, "Search contacts");
  lv_obj_t* ta = lv_textarea_create(scr);
  lv_textarea_set_one_line(ta, true);
  lv_textarea_set_text(ta, lvd_contact_filter());
  lv_textarea_set_placeholder_text(ta, "Name");
  lv_obj_set_pos(ta, 8, 38); lv_obj_set_size(ta, 320 - 16, 34);
  lv_obj_set_style_bg_color(ta, lv_color_hex(0x18202c), 0);
  lv_obj_set_style_border_color(ta, lv_color_hex(MD_PRIMARY), 0);
  lv_obj_set_style_border_width(ta, 1, 0);
  lv_obj_set_style_text_color(ta, lv_color_hex(UI_TEXT), 0);
  s_search_ta = ta;
  lv_ui_kbd_focus(ta);   // route the physical keyboard into this field
  lv_obj_add_event_cb(ta, search_ready, LV_EVENT_READY, NULL);   // physical Enter submits
  if (lvd_osk_enabled()) {
    lv_obj_t* kb = lv_keyboard_create(scr);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_set_size(kb, 320, 150);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(kb, search_ready, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(kb, search_cancel, LV_EVENT_CANCEL, NULL);
  }
}

// ============================ Global directory view =========================
// Second tier: the region-tagged directory of ALL observed nodes. Long-press a
// row to push it into the radio contacts (chat nodes). Region dropdown + search
// narrow the (thousands-strong) list; only DIR_RENDER_MAX rows are drawn.
static int s_cview = 0;   // 0 = Radio contacts, 1 = Directory

static void dir_push_cb(void* p) {
  int i = (int)(intptr_t)p;
  lvd_dir_t d;
  if (!lvd_dir_get(i, &d)) return;
  lv_ui_toast(lvd_dir_push(i) ? "Added to contacts" : "Already a contact");
}
static void dir_row_pressed(lv_event_t* e) { lv_async_call(dir_push_cb, lv_event_get_user_data(e)); }

static void dir_row(lv_obj_t* list, const lvd_dir_t* d, int idx) {
  lv_obj_t* row = lv_ui_md_card(list);
  lv_obj_set_layout(row, LV_LAYOUT_NONE);
  lv_obj_set_size(row, lv_pct(100), 42);
  lv_obj_set_pos(row, 0, 4 + idx * 46);
  lv_obj_set_style_pad_all(row, 8, 0);
  lv_obj_set_style_radius(row, 0, 0);
  lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(row, dir_row_pressed, LV_EVENT_LONG_PRESSED, (void*)(intptr_t)idx);  // hold to add
  lv_obj_t* pill = lv_ui_pill(row, type_tag(d->type), type_color(d->type));
  lv_obj_align(pill, LV_ALIGN_LEFT_MID, 0, -8);
  lv_obj_t* nm = lv_label_create(row);
  lv_label_set_text(nm, d->name);
  lv_obj_set_style_text_font(nm, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(nm, lv_color_hex(MD_ON), 0);
  lv_obj_align(nm, LV_ALIGN_LEFT_MID, 60, -8);
  lv_obj_t* sub = lv_label_create(row);
  lv_label_set_text(sub, d->subtitle);
  lv_obj_set_style_text_font(sub, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(sub, lv_color_hex(MD_MUTED), 0);
  lv_obj_align(sub, LV_ALIGN_LEFT_MID, 60, 9);
  lv_obj_t* add = lv_label_create(row);      // affordance hint (long-press adds)
  lv_label_set_text(add, LV_SYMBOL_PLUS);
  lv_obj_set_style_text_color(add, lv_color_hex(UI_GREEN), 0);
  lv_obj_align(add, LV_ALIGN_RIGHT_MID, 0, 0);
}

static void dir_fill(lv_obj_t* list) {
  int n = lvd_dir_count();
  if (n <= 0) {
    lv_obj_t* h = lv_label_create(list);
    lv_label_set_text(h, lvd_dir_size() == 0 ? "No nodes seen yet" : "No nodes match");
    lv_obj_set_style_text_color(h, lv_color_hex(MD_MUTED), 0);
    return;
  }
  lvd_dir_t d;
  for (int i = 0; i < n; i++) if (lvd_dir_get(i, &d)) dir_row(list, &d, i);
  int match = lvd_dir_match();
  if (match > n) {   // more matches than we render: nudge to refine
    lv_obj_t* more = lv_ui_md_card(list);
    lv_obj_set_layout(more, LV_LAYOUT_NONE);
    lv_obj_set_size(more, lv_pct(100), 26); lv_obj_set_pos(more, 0, 4 + n * 46);
    lv_obj_set_style_bg_opa(more, 0, 0);
    lv_obj_t* l = lv_label_create(more);
    static char mb[48]; snprintf(mb, sizeof(mb), "%d of %d shown - search to narrow", n, match);
    lv_label_set_text(l, mb);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(MD_MUTED), 0);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 4, 0);
  }
}

// directory region filter: a dropdown of the distinct regions present
static void dir_region_changed(lv_event_t* e) {
  lv_obj_t* dd = lv_event_get_target(e);
  int sel = lv_dropdown_get_selected(dd);
  if (sel == 0) lvd_dir_set_region_filter("");
  else { char r[16]; if (lvd_dir_region_list_get(sel - 1, r, sizeof(r))) lvd_dir_set_region_filter(r); }
  if (s_list) { lv_obj_clean(s_list); dir_fill(s_list); }
}
static void dir_region_dropdown(lv_obj_t* scr) {
  static char opts[64 * 17];
  int k = snprintf(opts, sizeof(opts), "All regions");
  int nr = lvd_dir_region_list_count();
  for (int i = 0; i < nr && k < (int)sizeof(opts) - 18; i++) {
    char r[16]; if (lvd_dir_region_list_get(i, r, sizeof(r))) k += snprintf(opts + k, sizeof(opts) - k, "\n%s", r);
  }
  lv_obj_t* dd = lv_dropdown_create(scr);
  lv_dropdown_set_options(dd, opts);
  const char* cur = lvd_dir_region_filter();
  int selidx = 0;
  if (cur[0]) for (int i = 0; i < nr; i++) { char r[16]; if (lvd_dir_region_list_get(i, r, sizeof(r)) && !strcmp(r, cur)) { selidx = i + 1; break; } }
  lv_dropdown_set_selected(dd, selidx);
  lv_obj_set_pos(dd, 8, 70); lv_obj_set_size(dd, 150, 28);   // left half, below the toggle
  lv_obj_set_style_bg_color(dd, lv_color_hex(0x18202c), 0);
  lv_obj_set_style_text_color(dd, lv_color_hex(UI_TEXT), 0);
  lv_obj_set_style_text_font(dd, &lv_font_montserrat_14, 0);
  lv_obj_add_event_cb(dd, dir_region_changed, LV_EVENT_VALUE_CHANGED, NULL);
}

// directory type filter (right half): All / Chat / Repeater / Room / Sensor
static void dir_type_changed(lv_event_t* e) {
  lvd_dir_set_type(lv_dropdown_get_selected(lv_event_get_target(e)));   // 0 all, else ADV_TYPE_*
  if (s_list) { lv_obj_clean(s_list); dir_fill(s_list); }
}
static void dir_type_dropdown(lv_obj_t* scr) {
  lv_obj_t* dd = lv_dropdown_create(scr);
  lv_dropdown_set_options(dd, "All types\nChat\nRepeater\nRoom\nSensor");
  lv_dropdown_set_selected(dd, lvd_dir_type());
  lv_obj_set_pos(dd, 162, 70); lv_obj_set_size(dd, 320 - 162 - 8, 28);   // right half, below the toggle
  lv_obj_set_style_bg_color(dd, lv_color_hex(0x18202c), 0);
  lv_obj_set_style_text_color(dd, lv_color_hex(UI_TEXT), 0);
  lv_obj_set_style_text_font(dd, &lv_font_montserrat_14, 0);
  lv_obj_add_event_cb(dd, dir_type_changed, LV_EVENT_VALUE_CHANGED, NULL);
}

// ---- Radio | Directory segmented toggle ------------------------------------
void lv_contacts_create(lv_obj_t* scr);   // rebuilt in place on view switch
static void contacts_rebuild(void* p) { (void)p; lv_obj_t* s = lv_screen_active(); lv_obj_clean(s); lv_contacts_create(s); }
static void cview_seg_cb(lv_event_t* e) {
  int v = (int)(intptr_t)lv_event_get_user_data(e);
  if (v != s_cview) { s_cview = v; lv_async_call(contacts_rebuild, NULL); }
}

// long-press the Radio tab -> red "Delete all radio contacts" confirm overlay
static void del_close(lv_event_t* e) { lv_obj_del((lv_obj_t*)lv_event_get_user_data(e)); }
static void del_confirm(lv_event_t* e) {
  lvd_clear_radio_contacts();
  lv_obj_del((lv_obj_t*)lv_event_get_user_data(e));         // close overlay
  lv_ui_toast("All radio contacts deleted");
  if (s_list) { lv_obj_clean(s_list); contacts_fill(s_list); }   // we're on the Radio tab
}
static void show_delete_all_contacts(void) {
  lv_obj_t* m = lv_obj_create(lv_screen_active());           // dim overlay; tap = cancel
  lv_obj_set_size(m, 320, 240); lv_obj_set_pos(m, 0, 0);
  lv_obj_set_style_bg_color(m, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(m, 170, 0);
  lv_obj_set_style_border_width(m, 0, 0);
  lv_obj_add_flag(m, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(m, del_close, LV_EVENT_CLICKED, m);
  lv_obj_t* btn = lv_button_create(m);
  lv_obj_set_size(btn, 250, 48); lv_obj_center(btn);
  lv_obj_set_style_bg_color(btn, lv_color_hex(UI_RED), 0);
  lv_obj_set_style_radius(btn, 10, 0);
  lv_obj_add_event_cb(btn, del_confirm, LV_EVENT_CLICKED, m);
  lv_obj_t* l = lv_label_create(btn);
  lv_label_set_text(l, "Delete all radio contacts");
  lv_obj_set_style_text_color(l, lv_color_hex(0xffffff), 0);
  lv_obj_center(l);
}
static void radio_seg_longpress(lv_event_t* e) {
  (void)e;
  if (s_cview == 0) show_delete_all_contacts();   // only meaningful on the Radio tab
}
static void cview_toggle(lv_obj_t* scr) {
  lv_obj_t* seg = lv_ui_card(scr, 4, 38, 320 - 8, 28);
  lv_obj_set_style_pad_all(seg, 3, 0);
  lv_obj_set_flex_flow(seg, LV_FLEX_FLOW_ROW); lv_obj_set_style_pad_column(seg, 4, 0);
  static const char* names[2] = { "Radio", "Directory" };
  for (int i = 0; i < 2; i++) {
    lv_obj_t* b = lv_obj_create(seg);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_grow(b, 1); lv_obj_set_height(b, lv_pct(100));
    lv_obj_set_style_radius(b, 6, 0); lv_obj_set_style_pad_all(b, 0, 0); lv_obj_set_style_border_width(b, 0, 0);
    bool sel = (s_cview == i);
    lv_obj_set_style_bg_color(b, lv_color_hex(sel ? MD_PRIMARY : 0x1a2230), 0);
    lv_obj_set_style_bg_opa(b, sel ? 220 : 120, 0);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(b, cview_seg_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    if (i == 0) lv_obj_add_event_cb(b, radio_seg_longpress, LV_EVENT_LONG_PRESSED, NULL);   // hold Radio -> delete all
    lv_ui_press_fx(b);
    lv_obj_t* l = lv_label_create(b); lv_label_set_text(l, names[i]);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(sel ? 0xffffff : MD_MUTED), 0);
    lv_obj_center(l);
  }
}

static lv_obj_t* make_scroll_list(lv_obj_t* scr, int top) {
  lv_obj_t* list = lv_obj_create(scr);
  lv_obj_set_pos(list, 0, top);
  lv_obj_set_size(list, 320, 240 - top);
  lv_obj_set_style_bg_opa(list, 0, 0);
  lv_obj_set_style_border_width(list, 0, 0);
  lv_obj_set_style_pad_hor(list, 12, 0);
  lv_obj_set_style_pad_ver(list, 2, 0);
  lv_obj_set_layout(list, LV_LAYOUT_NONE);    // manual row positioning -> no flex relayout on scroll
  return list;
}

// The contact list is a SNAPSHOT taken on screen entry -- we deliberately do not
// re-read contacts or rebuild the list while the user is browsing (that re-ran the
// whole sort/build and reset the scroll position when discovery added a node). A
// newly-added contact shows up next time the screen is opened.

void lv_contacts_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, "Contacts");
  cview_toggle(scr);                            // Radio | Directory (y38)
  if (s_cview == 1) {                           // ---- global directory ----
    dir_region_dropdown(scr);                   // y70 left  (region, incl. "Radio")
    dir_type_dropdown(scr);                     // y70 right (type)
    s_list = make_scroll_list(scr, 104);
    dir_fill(s_list);
    return;
  }
  // ---- radio contacts: selected chat nodes, alphabetical, no search/filters ----
  lvd_contact_set_type(1);                      // chats only
  lvd_contact_set_fav_only(0);                  // no favourites filter here
  s_list = make_scroll_list(scr, 70);           // list right below the toggle
  contacts_fill(s_list);                        // drag to scroll
  // no refresh hook: the list is a snapshot, not re-read while browsing
}
