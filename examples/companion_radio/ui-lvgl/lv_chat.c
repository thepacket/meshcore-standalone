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
  lv_obj_set_style_text_font(nm, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(nm, lv_color_hex(UI_TEXT), 0);

  lv_obj_t* pv = lv_label_create(mid);
  lv_label_set_text(pv, r->preview);
  lv_label_set_long_mode(pv, LV_LABEL_LONG_DOT);
  lv_obj_set_width(pv, lv_pct(100));
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
      Row r = { c.name, lvd_chat_chan_preview(i), "", 0, c.is_public ? UI_BLUE : UI_PURPLE, true };
      add_row(list, &r, i, false);
    }
  } else {
    // DM threads: distinct peers we've messaged. Start a new one from a contact's Message button.
    int ndm = lvd_dm_count();
    lvd_dm_t d;
    for (int i = 0; i < ndm; i++) {
      if (!lvd_dm_get(i, &d)) continue;
      Row r = { d.name, d.preview, "", 0, UI_GREEN, false };
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
static void add_bubble(lv_obj_t* scroll, const char* sender, const char* text,
                       bool outgoing, const char* status) {
  lv_obj_t* row = lv_obj_create(scroll);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_width(row, lv_pct(100));
  lv_obj_set_height(row, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(row, 0, 0); lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);

  lv_obj_t* bub = lv_obj_create(row);
  lv_obj_remove_flag(bub, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_width(bub, LV_SIZE_CONTENT);
  lv_obj_set_style_max_width(bub, 224, 0);
  lv_obj_set_height(bub, LV_SIZE_CONTENT);
  lv_obj_set_style_radius(bub, 12, 0);
  lv_obj_set_style_pad_all(bub, 7, 0);
  lv_obj_set_style_border_width(bub, 0, 0);
  lv_obj_set_flex_flow(bub, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(bub, 1, 0);
  if (outgoing) {
    lv_obj_set_style_bg_color(bub, lv_color_hex(UI_BLUE), 0);
    lv_obj_set_style_bg_grad_color(bub, lv_color_hex(UI_INDIGO), 0);
    lv_obj_set_style_bg_grad_dir(bub, LV_GRAD_DIR_VER, 0);
    lv_obj_align(bub, LV_ALIGN_TOP_RIGHT, 0, 0);
  } else {
    lv_obj_set_style_bg_color(bub, lv_color_hex(UI_CARD), 0);
    lv_obj_set_style_bg_opa(bub, 22, 0);
    lv_obj_align(bub, LV_ALIGN_TOP_LEFT, 0, 0);
  }

  if (!outgoing) {
    // tap an incoming bubble to see everything known about the sender
    lv_obj_add_flag(bub, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(bub, peer_clicked, LV_EVENT_CLICKED, (void*)(sender ? sender : "Peer"));
    lv_ui_press_fx(bub);
  }
  if (!outgoing && sender) {
    lv_obj_t* s = lv_label_create(bub);
    lv_label_set_text(s, sender);
    lv_obj_set_style_text_font(s, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s, lv_color_hex(name_color(sender)), 0);
  }
  lv_obj_t* t = lv_label_create(bub);
  lv_label_set_text(t, text);
  lv_label_set_long_mode(t, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(t, LV_SIZE_CONTENT);
  lv_obj_set_style_max_width(t, 210, 0);
  lv_obj_set_style_text_font(t, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(t, lv_color_hex(outgoing ? 0xffffff : UI_TEXT), 0);

  if (outgoing && status) {
    lv_obj_t* st = lv_label_create(bub);
    lv_label_set_text(st, status);
    lv_obj_set_style_text_font(st, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(st, lv_color_hex(0xd6e2ff), 0);
    lv_obj_set_style_align(st, LV_ALIGN_RIGHT_MID, 0);
  }
}

// ---- live Public conversation ----------------------------------------------
static lv_obj_t* s_conv_scroll = NULL;
static unsigned  s_conv_last = 0;

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
    if (lvd_chat_get(i, &m))
      add_bubble(scroll, m.sender[0] ? m.sender : NULL, m.text, m.outgoing != 0, NULL);
  lv_obj_scroll_to_view(lv_obj_get_child(scroll, -1), LV_ANIM_OFF);   // pin to newest
}

static void conv_tick(void) {
  if (s_conv_scroll && lvd_chat_total() != s_conv_last) {
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
  lv_ui_md_topbar(scr, lvd_chat_title());

  int composeH = 32;
  s_conv_scroll = lv_obj_create(scr);
  lv_obj_set_pos(s_conv_scroll, 4, 36);
  lv_obj_set_size(s_conv_scroll, 320 - 8, 240 - 32 - composeH - 4);
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
