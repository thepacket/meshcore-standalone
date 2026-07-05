// LVGL chat screens: list (Channels/DMs tabs, Material rows) + conversation
// (gradient speech bubbles, coloured sender names, compose bar). The Public
// channel conversation is bound to the live chat store (lvd_chat_*); DMs and
// other channels remain prototype for now.
#include "lv_ui.h"
#include "lv_data.h"
#include <stdio.h>
#include <string.h>

// ---- small helpers ----------------------------------------------------------
static lv_obj_t* avatar(lv_obj_t* parent, const char* txt, uint32_t color) {
  lv_obj_t* a = lv_obj_create(parent);
  lv_obj_remove_flag(a, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(a, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(a, 38, 38);
  lv_obj_set_style_radius(a, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(a, lv_color_hex(color), 0);
  lv_obj_set_style_bg_grad_color(a, lv_color_darken(lv_color_hex(color), 90), 0);
  lv_obj_set_style_bg_grad_dir(a, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_border_width(a, 0, 0);
  lv_obj_set_style_pad_all(a, 0, 0);
  lv_obj_set_style_shadow_color(a, lv_color_hex(color), 0);
  lv_obj_set_style_shadow_opa(a, 110, 0);
  lv_obj_set_style_shadow_width(a, 10, 0);
  lv_obj_t* l = lv_label_create(a);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(0xffffff), 0);
  lv_obj_center(l);
  return a;
}

// the peer whose message was last tapped (shown by the peer-details screen)
static char g_peer[24] = "Repeater-7";
const char* lv_chat_active_peer(void) { return g_peer; }
void lv_chat_set_peer(const char* name) {
  if (name) { strncpy(g_peer, name, sizeof(g_peer) - 1); g_peer[sizeof(g_peer) - 1] = 0; }
}
static void peer_clicked(lv_event_t* e) {
  const char* s = (const char*)lv_event_get_user_data(e);
  if (s) { strncpy(g_peer, s, sizeof(g_peer) - 1); g_peer[sizeof(g_peer) - 1] = 0; }
  if (lv_nav_cb) lv_nav_cb("peer");
}

static const uint32_t NAME_COLORS[] = {UI_BLUE, UI_GREEN, UI_AMBER, UI_PINK, UI_CYAN, UI_PURPLE};
static uint32_t name_color(const char* s) {
  uint32_t h = 5381; for (const char* p = s; *p; p++) h = ((h << 5) + h) + (uint8_t)*p;
  return NAME_COLORS[h % 6];
}

// =============================================================================
// Chat list
// =============================================================================
typedef struct { const char* name; const char* preview; const char* time; int unread; uint32_t color; bool channel; } Row;

static int s_chat_tab = 0;   // 0 = Channels, 1 = DMs
void lv_chat_list_create(lv_obj_t* scr);   // fwd (rebuilt on tab switch)

static void open_chan_conv(lv_event_t* e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  lvd_chat_open_channel(i);
  if (lv_nav_cb) lv_nav_cb("conv");
}
static void open_dm_conv(lv_event_t* e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  lvd_dm_open(i);
  if (lv_nav_cb) lv_nav_cb("conv");
}

static void add_row(lv_obj_t* list, const Row* r, int idx, bool is_dm) {
  lv_obj_t* row = lv_ui_md_card(list);
  lv_obj_set_width(row, lv_pct(100));
  lv_obj_set_height(row, 54);
  lv_obj_set_style_min_height(row, 0, 0);
  lv_obj_set_style_pad_all(row, 8, 0);
  lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(row, is_dm ? open_dm_conv : open_chan_conv, LV_EVENT_CLICKED, (void*)(intptr_t)idx);
  lv_ui_press_fx(row);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  char ini[8];
  if (r->channel) snprintf(ini, sizeof(ini), "#");
  else { ini[0] = r->name[0]; ini[1] = 0; }
  lv_obj_t* av = avatar(row, ini, r->color);
  lv_obj_set_style_margin_right(av, 10, 0);

  lv_obj_t* mid = lv_obj_create(row);
  lv_obj_remove_flag(mid, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(mid, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(mid, 0, 0); lv_obj_set_style_border_width(mid, 0, 0);
  lv_obj_set_style_pad_all(mid, 0, 0);
  lv_obj_set_flex_grow(mid, 1);
  lv_obj_set_height(mid, 40);
  lv_obj_set_flex_flow(mid, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(mid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

  lv_obj_t* nm = lv_label_create(mid);
  lv_label_set_text(nm, r->name);
  lv_label_set_long_mode(nm, LV_LABEL_LONG_DOT);
  lv_obj_set_size(nm, lv_pct(100), 19);   // one line; DOT only ellipsizes with a FIXED size
  lv_obj_set_style_text_font(nm, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(nm, lv_color_hex(UI_TEXT), 0);

  lv_obj_t* pv = lv_label_create(mid);
  lv_label_set_text(pv, r->preview);
  lv_label_set_long_mode(pv, LV_LABEL_LONG_DOT);
  lv_obj_set_size(pv, lv_pct(100), 15);   // one line: with height=content a long preview WRAPS
                                          // instead of ellipsizing and overflows the 54px row
  lv_obj_set_style_text_font(pv, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(pv, lv_color_hex(UI_MUTED), 0);

  lv_obj_t* right = lv_obj_create(row);
  lv_obj_remove_flag(right, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(right, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(right, 0, 0); lv_obj_set_style_border_width(right, 0, 0);
  lv_obj_set_style_pad_all(right, 0, 0);
  lv_obj_set_size(right, 36, 40);
  lv_obj_t* tm = lv_label_create(right);
  lv_label_set_text(tm, r->time);
  lv_obj_set_style_text_font(tm, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(tm, lv_color_hex(UI_MUTED), 0);
  lv_obj_align(tm, LV_ALIGN_TOP_RIGHT, 0, 0);
  if (r->unread > 0) {
    lv_obj_t* b = lv_ui_pill(right, "", UI_BADGE);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_t* bl = lv_obj_get_child(b, 0);
    lv_label_set_text_fmt(bl, "%d", r->unread);
    lv_obj_set_size(b, 20, 18);
    lv_obj_align(b, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  }
}

static void chat_rebuild_cb(void* p) { (void)p; lv_obj_t* s = lv_screen_active(); lv_obj_clean(s); lv_chat_list_create(s); }
static void tab_clicked(lv_event_t* e) {
  int tab = (int)(intptr_t)lv_event_get_user_data(e);
  if (tab != s_chat_tab) { s_chat_tab = tab; lv_async_call(chat_rebuild_cb, NULL); }
}
static void seg_tab(lv_obj_t* parent, const char* txt, int tab) {
  bool active = (tab == s_chat_tab);
  lv_obj_t* t = lv_obj_create(parent);
  lv_obj_remove_flag(t, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_grow(t, 1);
  lv_obj_set_height(t, lv_pct(100));
  lv_obj_set_style_radius(t, 8, 0);
  lv_obj_set_style_bg_color(t, lv_color_hex(MD_PRIMARY), 0);
  lv_obj_set_style_bg_opa(t, active ? 200 : 0, 0);
  lv_obj_set_style_border_width(t, 0, 0);
  lv_obj_set_style_pad_all(t, 0, 0);
  lv_obj_add_flag(t, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(t, tab_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)tab);
  lv_ui_press_fx(t);
  lv_obj_t* l = lv_label_create(t);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(active ? 0xffffff : UI_MUTED), 0);
  lv_obj_center(l);
}

void lv_chat_list_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, "Chat");

  // segmented tabs
  lv_obj_t* seg = lv_obj_create(scr);
  lv_obj_remove_flag(seg, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(seg, 320 - 24, 28);
  lv_obj_set_pos(seg, 12, 42);
  lv_obj_set_style_radius(seg, 10, 0);
  lv_obj_set_style_bg_color(seg, lv_color_hex(MD_SURFACE), 0);
  lv_obj_set_style_bg_opa(seg, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(seg, 0, 0);
  lv_obj_set_style_pad_all(seg, 3, 0);
  lv_obj_set_flex_flow(seg, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(seg, 4, 0);
  seg_tab(seg, "Channels", 0);
  seg_tab(seg, "DMs", 1);

  // list
  lv_obj_t* list = lv_obj_create(scr);
  lv_obj_set_pos(list, 0, 78);
  lv_obj_set_size(list, 320, 240 - 78);
  lv_obj_set_style_bg_opa(list, 0, 0);
  lv_obj_set_style_border_width(list, 0, 0);
  lv_obj_set_style_pad_hor(list, 12, 0);
  lv_obj_set_style_pad_ver(list, 2, 0);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(list, 8, 0);

  if (s_chat_tab == 0) {
    // one row per channel (Public + named group channels), live last-message preview
    int nch = lvd_chat_chan_count();
    for (int i = 0; i < nch; i++) {
      lvd_chan_t c;
      if (!lvd_chat_chan_get(i, &c)) continue;
      Row r = { c.name, lvd_chat_chan_preview(i), lvd_chat_chan_time(i), lvd_chat_chan_unread(i),
                c.is_public ? UI_BLUE : UI_PURPLE, true };
      add_row(list, &r, i, false);
    }
  } else {
    // DM threads: distinct peers we've messaged. Start a new one from a contact's Message button.
    int ndm = lvd_dm_count();
    lvd_dm_t d;
    for (int i = 0; i < ndm; i++) {
      if (!lvd_dm_get(i, &d)) continue;
      // room-server threads get an orange avatar (posts from many authors)
      Row r = { d.name, d.preview, d.time, d.unread, d.is_room ? UI_ORANGE : UI_GREEN, false };
      add_row(list, &r, i, true);
    }
    if (ndm == 0) {
      lv_obj_t* hint = lv_label_create(list);
      lv_label_set_text(hint, "No direct messages yet.\nOpen a contact and tap Message to start one.");
      lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
      lv_obj_set_width(hint, lv_pct(100));
      lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
      lv_obj_set_style_text_color(hint, lv_color_hex(UI_MUTED), 0);
      lv_obj_set_style_pad_top(hint, 8, 0);
    }
  }
}

// =============================================================================
// Conversation
// =============================================================================
static void bubble_long_pressed(lv_event_t* e);   // defined below (message action sheet)

// one message, phone-client style: a cyan meta line ("17:30 - 4 hops"), then
// for incoming a small coloured avatar + the sender's name in its colour, then
// the bubble itself (white text, roomy padding). Outgoing keeps the blue
// gradient bubble, right-aligned, with the delivery glyph in the meta line.
static void add_bubble(lv_obj_t* scroll, const char* sender, const char* text,
                       bool outgoing, const char* meta, int idx) {
  lv_obj_t* row = lv_obj_create(scroll);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_width(row, lv_pct(100));
  lv_obj_set_height(row, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(row, 0, 0); lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(row, 2, 0);
  // the whole message block hugs its side (cross-axis per message row)
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START,
                        outgoing ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

  if (meta && meta[0]) {
    lv_obj_t* mt = lv_label_create(row);
    lv_label_set_text(mt, meta);
    lv_obj_set_style_text_font(mt, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(mt, lv_color_hex(MD_MUTED), 0);
  }

  uint32_t scol = sender ? name_color(sender) : UI_GREEN;
  if (!outgoing && sender) {
    lv_obj_t* sr = lv_obj_create(row);
    lv_obj_remove_flag(sr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(sr, LV_SIZE_CONTENT, 22);
    lv_obj_set_style_bg_opa(sr, 0, 0); lv_obj_set_style_border_width(sr, 0, 0);
    lv_obj_set_style_pad_all(sr, 0, 0);
    lv_obj_set_flex_flow(sr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(sr, 6, 0);
    lv_obj_add_flag(sr, LV_OBJ_FLAG_CLICKABLE);   // tap the sender line -> peer card
    lv_obj_add_event_cb(sr, peer_clicked, LV_EVENT_CLICKED, (void*)sender);
    lv_obj_t* av = lv_obj_create(sr);
    lv_obj_remove_flag(av, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(av, 20, 20);
    lv_obj_set_style_radius(av, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(av, lv_color_hex(scol), 0);
    lv_obj_set_style_bg_opa(av, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(av, 0, 0); lv_obj_set_style_pad_all(av, 0, 0);
    lv_obj_t* ai = lv_label_create(av);
    char ini[2] = { sender[0], 0 };
    if (ini[0] >= 'a' && ini[0] <= 'z') ini[0] -= 32;
    lv_label_set_text(ai, ini);
    lv_obj_set_style_text_font(ai, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ai, lv_color_hex(0x000000), 0);
    lv_obj_center(ai);
    lv_obj_t* s = lv_label_create(sr);
    lv_label_set_text(s, sender);
    lv_obj_set_style_text_font(s, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s, lv_color_hex(scol), 0);
  }

  lv_obj_t* bub = lv_obj_create(row);
  lv_obj_remove_flag(bub, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_width(bub, LV_SIZE_CONTENT);
  lv_obj_set_style_max_width(bub, 244, 0);
  lv_obj_set_height(bub, LV_SIZE_CONTENT);
  lv_obj_set_style_radius(bub, 14, 0);
  lv_obj_set_style_pad_all(bub, 10, 0);
  lv_obj_set_style_border_width(bub, 0, 0);
  if (outgoing) {
    lv_obj_set_style_bg_color(bub, lv_color_hex(UI_BLUE), 0);
    lv_obj_set_style_bg_grad_color(bub, lv_color_hex(UI_INDIGO), 0);
    lv_obj_set_style_bg_grad_dir(bub, LV_GRAD_DIR_VER, 0);
  } else {
    lv_obj_set_style_bg_color(bub, lv_color_hex(0x2a3547), 0);   // reference-style slate bubble
    lv_obj_set_style_bg_opa(bub, LV_OPA_COVER, 0);
    // tap an incoming bubble to see everything known about the sender
    lv_obj_add_flag(bub, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(bub, peer_clicked, LV_EVENT_CLICKED, (void*)(sender ? sender : "Peer"));
    lv_ui_press_fx(bub);
  }
  // long-press any bubble for message actions (Resend a failed DM / Delete)
  lv_obj_add_flag(bub, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(bub, bubble_long_pressed, LV_EVENT_LONG_PRESSED, (void*)(intptr_t)idx);

  lv_obj_t* t = lv_label_create(bub);
  lv_label_set_text(t, text);
  lv_label_set_long_mode(t, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(t, LV_SIZE_CONTENT);
  lv_obj_set_style_max_width(t, 224, 0);
  lv_obj_set_style_text_font(t, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(t, lv_color_hex(0xffffff), 0);
}

// ---- live Public conversation ----------------------------------------------
static lv_obj_t* s_conv_scroll = NULL;
static unsigned  s_conv_last = 0;
static void conv_fill(lv_obj_t* scroll);   // fwd

// ---- message action sheet (long-press a bubble): Resend / Delete ------------
static void conv_refresh(void) { if (s_conv_scroll) { lv_obj_clean(s_conv_scroll); conv_fill(s_conv_scroll); } }
static void msg_modal_close(lv_event_t* e) { lv_obj_del((lv_obj_t*)lv_event_get_user_data(e)); }
static void msg_resend(lv_event_t* e) {
  lvd_chat_resend((int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e)));
  lv_obj_del((lv_obj_t*)lv_obj_get_parent(lv_obj_get_parent(lv_event_get_target(e))));  // overlay
  conv_refresh();
}
static void msg_delete(lv_event_t* e) {
  lvd_chat_delete((int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e)));
  lv_obj_del((lv_obj_t*)lv_obj_get_parent(lv_obj_get_parent(lv_event_get_target(e))));  // overlay
  conv_refresh();
}
static void bubble_long_pressed(lv_event_t* e) {
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  lvd_msg_t m; if (!lvd_chat_get(idx, &m)) return;

  lv_obj_t* ov = lv_obj_create(lv_screen_active());
  lv_obj_set_size(ov, 320, 240); lv_obj_set_pos(ov, 0, 0);
  lv_obj_set_style_bg_color(ov, lv_color_hex(0x000000), 0); lv_obj_set_style_bg_opa(ov, 150, 0);
  lv_obj_set_style_border_width(ov, 0, 0);
  lv_obj_add_flag(ov, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(ov, msg_modal_close, LV_EVENT_CLICKED, ov);   // tap outside dismisses

  lv_obj_t* card = lv_obj_create(ov);
  int rows = 1 + (m.can_resend ? 1 : 0);
  lv_obj_set_size(card, 220, 24 + rows * 44); lv_obj_center(card);
  lv_obj_set_style_bg_color(card, lv_color_hex(0x18202c), 0);
  lv_obj_set_style_border_width(card, 0, 0); lv_obj_set_style_radius(card, 12, 0);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_all(card, 10, 0);
  lv_obj_set_style_pad_row(card, 8, 0);

  if (m.can_resend) {
    lv_obj_t* b = lv_button_create(card);
    lv_obj_set_width(b, lv_pct(100)); lv_obj_set_height(b, 36);
    lv_obj_set_style_bg_color(b, lv_color_hex(UI_BLUE), 0);
    lv_obj_set_user_data(b, (void*)(intptr_t)idx);
    lv_obj_add_event_cb(b, msg_resend, LV_EVENT_CLICKED, NULL);
    lv_obj_t* l = lv_label_create(b); lv_label_set_text(l, LV_SYMBOL_REFRESH "  Resend"); lv_obj_center(l);
  }
  lv_obj_t* d = lv_button_create(card);
  lv_obj_set_width(d, lv_pct(100)); lv_obj_set_height(d, 36);
  lv_obj_set_style_bg_color(d, lv_color_hex(UI_RED), 0);
  lv_obj_set_user_data(d, (void*)(intptr_t)idx);
  lv_obj_add_event_cb(d, msg_delete, LV_EVENT_CLICKED, NULL);
  lv_obj_t* dl = lv_label_create(d); lv_label_set_text(dl, LV_SYMBOL_TRASH "  Delete"); lv_obj_center(dl);
}

static void conv_fill(lv_obj_t* scroll) {
  s_conv_last = lvd_chat_total();
  int n = lvd_chat_count();
  if (n <= 0) {
    lv_obj_t* hint = lv_label_create(scroll);
    lv_label_set_text(hint, "No messages yet. Tap below to post to Public.");
    lv_obj_set_style_text_color(hint, lv_color_hex(UI_MUTED), 0);
    return;
  }
  lvd_msg_t m;
  for (int i = 0; i < n; i++)
    if (lvd_chat_get(i, &m)) {
      char foot[28];
      if (m.outgoing) {
        const char* g = m.status == 3 ? LV_SYMBOL_OK LV_SYMBOL_OK :   // delivered
                        m.status == 4 ? LV_SYMBOL_WARNING :           // failed / timed out
                        m.status == 2 ? LV_SYMBOL_REFRESH :           // pending
                                        LV_SYMBOL_OK;                 // sent
        if (m.time[0]) snprintf(foot, sizeof(foot), "%s  %s", m.time, g);
        else           snprintf(foot, sizeof(foot), "%s", g);
      } else {
        // incoming: time first, then route (e.g. "14:32 · 2 hops")
        if (m.route[0] && m.time[0]) snprintf(foot, sizeof(foot), "%s  -  %s", m.time, m.route);
        else if (m.route[0])         snprintf(foot, sizeof(foot), "%s", m.route);
        else                         snprintf(foot, sizeof(foot), "%s", m.time);
      }
      add_bubble(scroll, m.sender[0] ? m.sender : NULL, m.text, m.outgoing != 0, foot, i);
    }
  lv_obj_scroll_to_view(lv_obj_get_child(scroll, -1), LV_ANIM_OFF);   // pin to newest
}

static void conv_tick(void) {
  // redraw on new messages, and while an outbound DM is still awaiting its ack
  // (so pending flips to delivered/failed on screen)
  if (s_conv_scroll && (lvd_chat_total() != s_conv_last || lvd_chat_has_pending())) {
    lv_obj_clean(s_conv_scroll);
    conv_fill(s_conv_scroll);
  }
}

// ---- emergency position share (confirm first: broadcasts your location) ----
static void loc_cancel(lv_event_t* e) {
  lv_obj_del((lv_obj_t*)lv_event_get_user_data(e));   // the modal overlay
}
static void loc_confirm(lv_event_t* e) {
  lv_obj_del((lv_obj_t*)lv_event_get_user_data(e));
  lv_ui_toast(lvd_chat_send_location() ? "Position sent" : "No position (GPS off, no manual lat/lon)");
}
static void loc_clicked(lv_event_t* e) {
  (void)e;
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
  lv_label_set_text_fmt(q, "Send your position to\n\"%s\"?\nEmergency use - reveals your location.", lvd_chat_title());
  lv_obj_set_style_text_font(q, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(q, lv_color_hex(UI_TEXT), 0);
  lv_obj_align(q, LV_ALIGN_TOP_MID, 0, 0);

  lv_obj_t* cancel = lv_button_create(card);
  lv_obj_set_size(cancel, 110, 36); lv_obj_align(cancel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_style_bg_color(cancel, lv_color_hex(0x2a3343), 0);
  lv_obj_add_event_cb(cancel, loc_cancel, LV_EVENT_CLICKED, m);
  lv_obj_t* cl = lv_label_create(cancel); lv_label_set_text(cl, "Cancel"); lv_obj_center(cl);

  lv_obj_t* yes = lv_button_create(card);
  lv_obj_set_size(yes, 110, 36); lv_obj_align(yes, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_obj_set_style_bg_color(yes, lv_color_hex(UI_RED), 0);
  lv_obj_add_event_cb(yes, loc_confirm, LV_EVENT_CLICKED, m);
  lv_obj_t* yl = lv_label_create(yes); lv_label_set_text(yl, "Send"); lv_obj_center(yl);
}

void lv_chat_conv_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);

  // conversation header: back chevron + coloured avatar + title + kind subtitle
  const char* title = lvd_chat_title();
  int is_ch = lvd_chat_is_channel();
  uint32_t hcol = name_color(title);
  lv_obj_t* hbar = lv_obj_create(scr);
  lv_obj_remove_flag(hbar, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(hbar, 320, 40); lv_obj_set_pos(hbar, 0, 0);
  lv_obj_set_style_bg_color(hbar, lv_color_hex(UI_BLUE), 0);
  lv_obj_set_style_bg_opa(hbar, 70, 0);
  lv_obj_set_style_radius(hbar, 0, 0); lv_obj_set_style_pad_all(hbar, 0, 0);
  lv_obj_set_style_border_side(hbar, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_color(hbar, lv_color_hex(UI_BLUE), 0);
  lv_obj_set_style_border_opa(hbar, 235, 0); lv_obj_set_style_border_width(hbar, 2, 0);
  lv_obj_t* back = lv_obj_create(hbar);
  lv_obj_remove_flag(back, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(back, 34, 40); lv_obj_align(back, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_set_style_bg_opa(back, 0, 0); lv_obj_set_style_border_width(back, 0, 0);
  lv_obj_set_style_pad_all(back, 0, 0);
  lv_obj_t* bl = lv_label_create(back);
  lv_label_set_text(bl, LV_SYMBOL_LEFT);
  lv_obj_set_style_text_color(bl, lv_color_hex(0xffffff), 0);
  lv_obj_center(bl);
  lv_ui_clickable(back, "back");
  lv_obj_t* hav = lv_obj_create(hbar);
  lv_obj_remove_flag(hav, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(hav, 28, 28); lv_obj_set_pos(hav, 36, 6);
  lv_obj_set_style_radius(hav, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(hav, lv_color_hex(hcol), 0);
  lv_obj_set_style_bg_opa(hav, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(hav, 0, 0); lv_obj_set_style_pad_all(hav, 0, 0);
  lv_obj_t* hai = lv_label_create(hav);
  char hini[2] = { title[0] ? title[0] : '?', 0 };
  if (hini[0] >= 'a' && hini[0] <= 'z') hini[0] -= 32;
  lv_label_set_text(hai, hini);
  lv_obj_set_style_text_font(hai, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(hai, lv_color_hex(0x000000), 0);
  lv_obj_center(hai);
  lv_obj_t* ht = lv_label_create(hbar);
  lv_label_set_text(ht, title);
  lv_obj_set_style_text_font(ht, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(ht, lv_color_hex(0xffffff), 0);
  lv_obj_set_pos(ht, 72, 3);
  lv_obj_t* hs = lv_label_create(hbar);
  lv_label_set_text(hs, is_ch ? "Channel" : "Direct message");
  lv_obj_set_style_text_font(hs, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(hs, lv_color_hex(MD_MUTED), 0);
  lv_obj_set_pos(hs, 72, 22);

  int composeH = 32;
  s_conv_scroll = lv_obj_create(scr);
  lv_obj_set_pos(s_conv_scroll, 4, 44);
  // scroll must END above the compose bar (it starts at 240-composeH-2): a
  // too-tall scroll let the last bubble render underneath it (clipped)
  lv_obj_set_size(s_conv_scroll, 320 - 8, 240 - 44 - composeH - 6);
  lv_obj_set_style_bg_opa(s_conv_scroll, 0, 0);
  lv_obj_set_style_border_width(s_conv_scroll, 0, 0);
  lv_obj_set_style_pad_all(s_conv_scroll, 4, 0);
  lv_obj_set_flex_flow(s_conv_scroll, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(s_conv_scroll, 6, 0);
  conv_fill(s_conv_scroll);

  // compose bar -> opens the keyboard compose screen
  lv_obj_t* bar = lv_ui_card(scr, 4, 240 - composeH - 2, 320 - 8, composeH);
  lv_obj_set_style_pad_all(bar, 4, 0);
  lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_ui_clickable(bar, "compose");

  lv_obj_t* input = lv_label_create(bar);
  lv_label_set_text(input, "Message");
  lv_obj_set_flex_grow(input, 1);
  lv_obj_set_style_text_font(input, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(input, lv_color_hex(UI_MUTED), 0);
  lv_obj_set_style_pad_left(input, 6, 0);

  // emergency position share (red pin) -- confirm dialog before sending
  lv_obj_t* locb = lv_obj_create(bar);
  lv_obj_remove_flag(locb, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(locb, 24, 24);
  lv_obj_set_style_radius(locb, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(locb, lv_color_hex(UI_RED), 0);
  lv_obj_set_style_border_width(locb, 0, 0);
  lv_obj_set_style_pad_all(locb, 0, 0);
  lv_obj_set_style_margin_right(locb, 4, 0);
  lv_obj_add_flag(locb, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(locb, loc_clicked, LV_EVENT_CLICKED, NULL);
  lv_ui_press_fx(locb);
  lv_obj_t* li = lv_label_create(locb);
  lv_label_set_text(li, LV_SYMBOL_GPS);
  lv_obj_set_style_text_color(li, lv_color_hex(0xffffff), 0);
  lv_obj_center(li);

  lv_obj_t* send = lv_obj_create(bar);
  lv_obj_remove_flag(send, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(send, 24, 24);
  lv_obj_set_style_radius(send, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(send, lv_color_hex(UI_BLUE), 0);
  lv_obj_set_style_border_width(send, 0, 0);
  lv_obj_set_style_pad_all(send, 0, 0);
  lv_obj_t* si = lv_label_create(send);
  lv_label_set_text(si, LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_color(si, lv_color_hex(0xffffff), 0);
  lv_obj_center(si);

  lv_ui_set_refresh(conv_tick);
}

// ---- compose (keyboard) for a Public message -------------------------------
static lv_obj_t* s_compose_ta = NULL;

static void compose_ready(lv_event_t* e) {
  (void)e;
  if (s_compose_ta) {
    const char* txt = lv_textarea_get_text(s_compose_ta);
    if (txt && txt[0]) lvd_chat_send(txt);
  }
  if (lv_nav_cb) lv_nav_cb("back");
}
static void compose_cancel(lv_event_t* e) { (void)e; if (lv_nav_cb) lv_nav_cb("back"); }

void lv_chat_compose_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, lvd_chat_title());

  lv_obj_t* ta = lv_textarea_create(scr);
  lv_textarea_set_one_line(ta, true);
  lv_textarea_set_placeholder_text(ta, "Message");
  lv_obj_set_pos(ta, 8, 38);
  lv_obj_set_size(ta, 320 - 16, 34);
  lv_obj_set_style_bg_color(ta, lv_color_hex(0x18202c), 0);
  lv_obj_set_style_border_color(ta, lv_color_hex(MD_PRIMARY), 0);
  lv_obj_set_style_border_width(ta, 1, 0);
  lv_obj_set_style_text_color(ta, lv_color_hex(UI_TEXT), 0);
  s_compose_ta = ta;
  lv_ui_kbd_focus(ta);   // route the physical keyboard into this field
  lv_obj_add_event_cb(ta, compose_ready, LV_EVENT_READY, NULL);   // physical Enter sends

  if (lvd_osk_enabled()) {
    lv_obj_t* kb = lv_keyboard_create(scr);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_set_size(kb, 320, 150);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(kb, compose_ready, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(kb, compose_cancel, LV_EVENT_CANCEL, NULL);
  }
}
