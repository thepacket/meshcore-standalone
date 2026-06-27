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

static void open_search(lv_event_t* e) { (void)e; if (lv_nav_cb) lv_nav_cb("contact_search"); }
static void do_clear_cb(void* p) { (void)p; lvd_contact_set_filter(""); if (s_list) { lv_obj_clean(s_list); contacts_fill(s_list); } }
static void clear_search(lv_event_t* e) { (void)e; lv_async_call(do_clear_cb, NULL); }

// a tappable search field row (magnifier + filter text / placeholder + clear),
// fixed on the screen above the list so it does not scroll away
static void search_field(lv_obj_t* scr) {
  const char* f = lvd_contact_filter();
  bool active = f && f[0];
  lv_obj_t* sf = lv_ui_md_card(scr);
  lv_obj_set_size(sf, 320 - 16, 34);
  lv_obj_set_pos(sf, 8, 38);                    // fixed, just below the topbar
  lv_obj_set_flex_flow(sf, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(sf, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_add_flag(sf, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(sf, open_search, LV_EVENT_CLICKED, NULL);
  lv_ui_press_fx(sf);

  lv_obj_t* ic = lv_label_create(sf);
  lv_label_set_text(ic, ICON_FINDER);
  lv_obj_set_style_text_font(ic, &icons_fa, 0);
  lv_obj_set_style_text_color(ic, lv_color_hex(MD_MUTED), 0);
  lv_obj_set_style_margin_right(ic, 12, 0);

  lv_obj_t* st = lv_label_create(sf);
  lv_label_set_text(st, active ? f : "Search contacts");
  lv_obj_set_style_text_font(st, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(st, lv_color_hex(active ? MD_ON : MD_MUTED), 0);
  lv_obj_set_flex_grow(st, 1);

  if (active) {   // a tappable clear (×) -- its own click target, so it won't open search
    lv_obj_t* cl = lv_label_create(sf);
    lv_label_set_text(cl, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(cl, lv_color_hex(MD_MUTED), 0);
    lv_obj_add_flag(cl, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(cl, 10);
    lv_obj_add_event_cb(cl, clear_search, LV_EVENT_CLICKED, NULL);
    lv_ui_press_fx(cl);
  }
}

static void contacts_fill(lv_obj_t* list) {
  int n = lvd_contact_count();
  s_last = n;
  if (n <= 0) {
    const char* f = lvd_contact_filter();
    lv_obj_t* hint = lv_label_create(list);
    lv_label_set_text(hint, (f && f[0]) ? "No contacts match" : "No contacts yet");
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

// The contact list is a SNAPSHOT taken on screen entry -- we deliberately do not
// re-read contacts or rebuild the list while the user is browsing (that re-ran the
// whole sort/build and reset the scroll position when discovery added a node). A
// newly-added contact shows up next time the screen is opened.

void lv_contacts_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, "Contacts");
  search_field(scr);                            // fixed above the list (does not scroll)

  s_list = lv_obj_create(scr);                  // scrollable list below the search field
  lv_obj_set_pos(s_list, 0, 76);
  lv_obj_set_size(s_list, 320, 240 - 76);
  lv_obj_set_style_bg_opa(s_list, 0, 0);
  lv_obj_set_style_border_width(s_list, 0, 0);
  lv_obj_set_style_pad_hor(s_list, 12, 0);
  lv_obj_set_style_pad_ver(s_list, 2, 0);
  lv_obj_set_layout(s_list, LV_LAYOUT_NONE);    // manual row positioning -> no flex relayout on scroll
  contacts_fill(s_list);                        // drag to scroll
  // no refresh hook: the list is a snapshot, not re-read while browsing
}
