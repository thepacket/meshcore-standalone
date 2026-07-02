// LVGL bring-up + screen manager + live-data bridge for the T-Deck companion UI.
#include "UITask.h"
#include <Arduino.h>
#include <lvgl.h>
#include <helpers/ui/LGFXDisplay.h>
#include "lv_ui.h"
#include "lv_data.h"
#include "../MyMesh.h"
#include "target.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <Preferences.h>

extern MyMesh the_mesh;   // global mesh instance (main.cpp)

// case-insensitive name compare for alphabetical sorting of lists
static int ci_strcmp(const char* a, const char* b) {
  while (*a && *b) {
    int d = tolower((unsigned char)*a) - tolower((unsigned char)*b);
    if (d) return d;
    a++; b++;
  }
  return (unsigned char)*a - (unsigned char)*b;
}

#ifndef BATT_MIN_MILLIVOLTS
  #define BATT_MIN_MILLIVOLTS 3300
#endif
#ifndef BATT_MAX_MILLIVOLTS
  #define BATT_MAX_MILLIVOLTS 4200
#endif

static UITask* g_ui = nullptr;   // set in begin(); used by the lvd_* bridge

// ---- ui-lvgl screen create functions (C) ----------------------------------
extern "C" {
  void lv_home_create(lv_obj_t* scr);
  void lv_chat_list_create(lv_obj_t* scr);
  void lv_chat_conv_create(lv_obj_t* scr);
  void lv_chat_compose_create(lv_obj_t* scr);
  void lv_settings_create(lv_obj_t* scr);
  void lv_settings_group_create(lv_obj_t* scr, int idx);
  void lv_settings_edit_create(lv_obj_t* scr);
  void lv_noise_create(lv_obj_t* scr);
  void lv_signal_create(lv_obj_t* scr);
  void lv_heard_create(lv_obj_t* scr);
  void lv_repeaters_create(lv_obj_t* scr);
  void lv_repeaters_set_tab(int t);
  void lv_repeater_detail_create(lv_obj_t* scr);
  void lv_rep_login_create(lv_obj_t* scr);
  void lv_rep_cli_create(lv_obj_t* scr);
  void lv_peer_create(lv_obj_t* scr);
  void lv_peer_export_create(lv_obj_t* scr);
  void lv_channels_create(lv_obj_t* scr);
  void lv_chan_detail_create(lv_obj_t* scr);
  void lv_chan_add_create(lv_obj_t* scr);
  void lv_rfscope_create(lv_obj_t* scr);
  void lv_trace_create(lv_obj_t* scr);
  void lv_trace_rep_search_create(lv_obj_t* scr);
  void lv_terminal_create(lv_obj_t* scr);
  void lv_terminal_set_tab(int t);
  void lv_pkt_detail_create(lv_obj_t* scr);
  void lv_pkt_search_create(lv_obj_t* scr);
  void lv_stats_create(lv_obj_t* scr);
  void lv_discover_create(lv_obj_t* scr);
  void lv_disc_create(lv_obj_t* scr);
  void lv_contacts_create(lv_obj_t* scr);
  void lv_contact_search_create(lv_obj_t* scr);
}

// ===========================================================================
// screen dispatch (mirrors sim-lvgl/main.c build())
// ===========================================================================
static void build_screen(const char* name) {
  lv_obj_t* s = lv_screen_active();
  lv_ui_set_refresh(NULL);   // drop the previous screen's live-refresh hook
  lv_obj_clean(s);
  if (!strcmp(name, "home")) lv_home_create(s);
  else if (!strcmp(name, "chat")) lv_chat_list_create(s);
  else if (!strcmp(name, "conv")) lv_chat_conv_create(s);
  else if (!strcmp(name, "compose")) lv_chat_compose_create(s);
  else if (!strcmp(name, "contacts")) lv_contacts_create(s);
  else if (!strcmp(name, "contact_search")) lv_contact_search_create(s);
  else if (!strcmp(name, "settings")) lv_settings_create(s);
  else if (!strcmp(name, "edit")) lv_settings_edit_create(s);
  else if (name[0] == 's' && name[1] == 'g') lv_settings_group_create(s, atoi(name + 2));
  else if (!strcmp(name, "noise")) lv_noise_create(s);
  else if (!strcmp(name, "signal")) lv_signal_create(s);
  else if (!strcmp(name, "heard")) lv_heard_create(s);
  else if (!strcmp(name, "repeaters")) { lv_repeaters_set_tab(0); lv_repeaters_create(s); }
  else if (!strcmp(name, "scan")) { lv_repeaters_set_tab(1); lv_repeaters_create(s); }
  else if (!strcmp(name, "repeater_detail")) lv_repeater_detail_create(s);
  else if (!strcmp(name, "rep_login")) lv_rep_login_create(s);
  else if (!strcmp(name, "rep_cli")) lv_rep_cli_create(s);
  else if (!strcmp(name, "peer")) lv_peer_create(s);
  else if (!strcmp(name, "peer_export")) lv_peer_export_create(s);
  else if (!strcmp(name, "channels")) lv_channels_create(s);
  else if (!strcmp(name, "chan_detail")) lv_chan_detail_create(s);
  else if (!strcmp(name, "chan_add")) lv_chan_add_create(s);
  else if (!strcmp(name, "rfscope")) lv_rfscope_create(s);
  else if (!strcmp(name, "trace")) lv_trace_create(s);
  else if (!strcmp(name, "tr_rep_search")) lv_trace_rep_search_create(s);
  else if (!strcmp(name, "terminal")) lv_terminal_create(s);
  else if (!strcmp(name, "packets")) lv_terminal_create(s);
  else if (!strcmp(name, "pktdetail")) lv_pkt_detail_create(s);
  else if (!strcmp(name, "pkt_search")) lv_pkt_search_create(s);
  else if (!strcmp(name, "stats")) lv_stats_create(s);
  else if (!strcmp(name, "discover")) lv_discover_create(s);
  else if (!strcmp(name, "disc")) lv_disc_create(s);
  else lv_home_create(s);
}

#define NAV_NAME_MAX 24
static char g_nav_stack[16][NAV_NAME_MAX];
static int  g_nav_sp = 0;

static void nav(const char* dest) {
  if (!strcmp(dest, "back")) {
    if (g_nav_sp > 1) g_nav_sp--;
  } else if (g_nav_sp < 16) {
    strncpy(g_nav_stack[g_nav_sp], dest, NAV_NAME_MAX - 1);
    g_nav_stack[g_nav_sp][NAV_NAME_MAX - 1] = 0;
    g_nav_sp++;
  }
  build_screen(g_nav_stack[g_nav_sp - 1]);
}

// ===========================================================================
// LVGL hardware glue
// ===========================================================================
static LGFX_Device*  g_gfx  = nullptr;   // panel, for flush
static DisplayDriver* g_disp = nullptr;  // for getTouch

static uint32_t tick_cb(void) { return millis(); }

// Defined in the T-Deck target: re-asserts the shared SPI output pins for the
// LoRa radio (SPI2) after the display (SPI3) has driven them during a flush.
// Safe now that the display is on its own host -- this only resets SPI2.
extern void radio_spi_claim();

// Set when a flush drove the display this loop, so we re-claim the shared SPI
// pins for the radio exactly once per frame (after lv_timer_handler) instead of
// on every partial tile -- spi.end()/begin() is expensive and the radio doesn't
// touch the bus until the_mesh.loop() runs, after the whole frame is flushed.
static volatile bool g_disp_drew = false;

static void flush_cb(lv_display_t* d, const lv_area_t* area, uint8_t* px_map) {
  if (g_gfx) {
    int w = area->x2 - area->x1 + 1;
    int h = area->y2 - area->y1 + 1;
    g_gfx->startWrite();
    g_gfx->pushImage(area->x1, area->y1, w, h, (lgfx::rgb565_t*)px_map);
    g_gfx->endWrite();
    g_disp_drew = true;
  }
  lv_display_flush_ready(d);
}

static void touch_read_cb(lv_indev_t* indev, lv_indev_data_t* data) {
  (void)indev;
  int x = 0, y = 0;
  if (g_disp && g_disp->getTouch(&x, &y)) {
    data->point.x = x;
    data->point.y = y;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

#if defined(UI_HAS_KEYBOARD)
// map the T-Deck keyboard's ASCII byte to an LVGL key (printables pass through)
static uint32_t kb_map(uint8_t c) {
  switch (c) {
    case 8: case 127: return LV_KEY_BACKSPACE;
    case 10: case 13: return LV_KEY_ENTER;
    case 27:          return LV_KEY_ESC;
    default:          return c;   // printable ASCII -> inserted into the focused field
  }
}
// LVGL keypad device: one press per key (the C3 keyboard yields each key once),
// reported as PRESSED then RELEASED so LVGL registers a single keystroke.
static void kb_read_cb(lv_indev_t* indev, lv_indev_data_t* data) {
  (void)indev;
  static uint32_t held = 0;
  char c = tdeck_keyboard.read();
  if (c) { held = kb_map((uint8_t)c); data->key = held; data->state = LV_INDEV_STATE_PRESSED; }
  else   {                            data->key = held; data->state = LV_INDEV_STATE_RELEASED; }
}
#endif

#if defined(UI_HAS_TRACKBALL)
// The active screen's main scrollable: the direct child with the most scrollable
// content. The trackball scrolls this up/down (left/right are intentionally unused).
static lv_obj_t* active_scrollable() {
  lv_obj_t* scr = lv_screen_active();
  if (!scr) return nullptr;
  lv_obj_t* best = nullptr; int best_range = 0;
  uint32_t n = lv_obj_get_child_count(scr);
  for (uint32_t i = 0; i < n; i++) {
    lv_obj_t* c = lv_obj_get_child(scr, i);
    if (!lv_obj_has_flag(c, LV_OBJ_FLAG_SCROLLABLE)) continue;
    int range = lv_obj_get_scroll_top(c) + lv_obj_get_scroll_bottom(c);
    if (range > best_range) { best_range = range; best = c; }
  }
  return best;
}
#define TRACKBALL_STEP_PX 12
// The trackball directions are noisy, quadrature-like pulses as the ball rolls
// (~3 transitions per roll, idle level wanders). FALLING-only misses most of
// them, so count *every* edge (CHANGE) and scroll a small step per edge for
// smooth, reliable up/down. Left/right are intentionally unused.
static volatile uint32_t s_tb_up = 0, s_tb_down = 0;
static void IRAM_ATTR tb_up_isr()   { s_tb_up++; }
static void IRAM_ATTR tb_down_isr() { s_tb_down++; }
#endif

// two partial draw buffers (internal RAM); ~1/6 screen each
#define LV_BUF_LINES 40
static uint8_t g_buf1[320 * LV_BUF_LINES * 2];
static uint8_t g_buf2[320 * LV_BUF_LINES * 2];

// LVGL timer: tick the active screen's registered live-refresh hook (if any).
static void refresh_timer_cb(lv_timer_t*) {
  lv_refresh_fn f = lv_ui_get_refresh();
  if (f) f();
}

void UITask::begin(DisplayDriver* display, SensorManager* sensors, NodePrefs* node_prefs) {
  _display = display;
  _sensors = sensors;
  _node_prefs = node_prefs;
  g_ui = this;

  g_disp = display;
  g_gfx  = static_cast<LGFXDisplay*>(display)->gfxDevice();

  lv_init();
  lv_tick_set_cb(tick_cb);

  lv_display_t* disp = lv_display_create(320, 240);
  lv_display_set_flush_cb(disp, flush_cb);
  lv_display_set_buffers(disp, g_buf1, g_buf2, sizeof(g_buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_indev_t* indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touch_read_cb);
  lv_indev_set_scroll_limit(indev, 4);   // small drag = scroll (not an accidental row tap)

#if defined(UI_HAS_KEYBOARD)
  // physical keyboard: a keypad device feeding the shared focus group, so the
  // T-Deck keys type into whichever text field a screen has focused.
  tdeck_keyboard.begin();
  lv_indev_t* kbd = lv_indev_create();
  lv_indev_set_type(kbd, LV_INDEV_TYPE_KEYPAD);
  lv_indev_set_read_cb(kbd, kb_read_cb);
  lv_indev_set_group(kbd, lv_ui_kbd_group());
#endif

#if defined(UI_HAS_TRACKBALL)
  // up/down only (scrolling); count every signal edge via CHANGE interrupts
  pinMode(PIN_TRACKBALL_UP, INPUT_PULLUP);
  pinMode(PIN_TRACKBALL_DOWN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_TRACKBALL_UP),   tb_up_isr,   CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_TRACKBALL_DOWN), tb_down_isr, CHANGE);
#endif

  lv_nav_cb = nav;       // screens route taps through here
  g_nav_sp = 0;
  nav("home");

  // drive live screens: every 1s, call the active screen's refresh hook (if any)
  lv_timer_create(refresh_timer_cb, 1000, nullptr);

  _lvgl_ready = true;
}

void UITask::loop() {
  if (!_lvgl_ready) return;
  // lv_timer_handler() has ~26us fixed overhead per call; the main loop spins it
  // tens of thousands of times a second, burning >50% CPU on nothing. Throttle to
  // ~5ms (LVGL's refresh period is 16ms) so the rest of the CPU is free to render.
  static uint32_t s_last_lv = 0;
  uint32_t lv_now = millis();
  if (lv_now - s_last_lv < 5) return;
  s_last_lv = lv_now;

#if defined(UI_HAS_TRACKBALL)
  // drain the edge counters and scroll the active list (left/right intentionally unused)
  uint32_t tbu = s_tb_up, tbd = s_tb_down;
  if (tbu) { s_tb_up   -= tbu; lv_obj_t* s = active_scrollable(); if (s) lv_obj_scroll_by(s, 0,  (int)(TRACKBALL_STEP_PX * tbu), LV_ANIM_OFF); }
  if (tbd) { s_tb_down -= tbd; lv_obj_t* s = active_scrollable(); if (s) lv_obj_scroll_by(s, 0, -(int)(TRACKBALL_STEP_PX * tbd), LV_ANIM_OFF); }
#endif

  lv_timer_handler();
  if (g_disp_drew) {       // the frame is fully flushed: give the bus back once
    g_disp_drew = false;
    radio_spi_claim();
  }
}

// ===========================================================================
// Live data bridge (lv_data.h) -- the C screens query these.
// ===========================================================================
extern "C" const char* lvd_node_name(void) {
  return (g_ui && g_ui->nodePrefs()) ? g_ui->nodePrefs()->node_name : "MeshCore";
}
extern "C" const char* lvd_device_label(void) {
#ifdef UI_DEVICE_LABEL
  return UI_DEVICE_LABEL;
#else
  return "device";
#endif
}
extern "C" void lvd_clock_hhmm(char* out, int len) {
  uint32_t e = rtc_clock.getCurrentTime();
  if (e < 1000000) { strncpy(out, "--:--", len); out[len - 1] = 0; return; }
  snprintf(out, len, "%02u:%02u", (unsigned)((e / 3600) % 24), (unsigned)((e / 60) % 60));
}
extern "C" int lvd_batt_pct(void) {
  if (!g_ui) return -1;
  int mv = g_ui->getBattMilliVolts();
  int pct = (mv - BATT_MIN_MILLIVOLTS) * 100 / (BATT_MAX_MILLIVOLTS - BATT_MIN_MILLIVOLTS);
  return pct < 0 ? 0 : (pct > 100 ? 100 : pct);
}
extern "C" int lvd_signal_bars(void) {
  int r = g_ui ? g_ui->lastRssi() : 0;
  if (r == 0) return 0;
  if (r >= -70) return 4;
  if (r >= -85) return 3;
  if (r >= -100) return 2;
  return 1;
}
extern "C" int lvd_unread_count(void) { return 0; }

// getRecentlyHeard() returns the WHOLE table including empty slots, so filter to
// real entries (a used slot has a name or a recv timestamp) and cache them; the
// screen calls lvd_heard_count() first, then lvd_heard_get(i) per row.
static AdvertPath g_heard[16];
static int        g_heard_n = 0;

extern "C" int lvd_heard_count(void) {
  AdvertPath rec[16];
  int n = the_mesh.getRecentlyHeard(rec, 16);
  g_heard_n = 0;
  for (int i = 0; i < n && g_heard_n < 16; i++) {
    if (rec[i].name[0] != 0 || rec[i].recv_timestamp != 0) g_heard[g_heard_n++] = rec[i];
  }
  return g_heard_n;
}
extern "C" bool lvd_heard_get(int i, lvd_heard_t* out) {
  if (i < 0 || i >= g_heard_n) return false;
  AdvertPath& a = g_heard[i];
  strncpy(out->name, a.name, sizeof(out->name) - 1); out->name[sizeof(out->name) - 1] = 0;

  char snrbuf[20] = "", rssibuf[16] = "";
  if (a.snr_q) snprintf(snrbuf, sizeof(snrbuf), "SNR %.1f dB", a.snr_q / 4.0f);
  if (a.rssi)  snprintf(rssibuf, sizeof(rssibuf), "RSSI %d", a.rssi);
  snprintf(out->meta, sizeof(out->meta), "%s%s%s",
           snrbuf, (snrbuf[0] && rssibuf[0]) ? "   " : "", rssibuf);

  uint32_t now = rtc_clock.getCurrentTime();
  uint32_t age = (now > a.recv_timestamp) ? now - a.recv_timestamp : 0;
  if (age < 60)        snprintf(out->age, sizeof(out->age), "%us ago", (unsigned)age);
  else if (age < 3600) snprintf(out->age, sizeof(out->age), "%um ago", (unsigned)(age / 60));
  else                 snprintf(out->age, sizeof(out->age), "%uh ago", (unsigned)(age / 3600));

  out->dist[0] = 0;   // distance binding added with GPS later
  int snr = a.snr_q / 4;
  out->bars = (a.snr_q == 0) ? 1 : (snr >= 5 ? 4 : (snr >= 0 ? 2 : 1));
  return true;
}

// ---- contacts (Contacts screen) --------------------------------------------
// Present contacts alphabetically by name. getContactByIdx() returns storage
// order, so keep a name-sorted index of contact indices, rebuilt when the count
// changes. The comparator fetches the two contacts by index on demand.
static int cmp_contact_idx(const void* a, const void* b) {
  ContactInfo ca, cb;
  the_mesh.getContactByIdx(*(const uint16_t*)a, ca);
  the_mesh.getContactByIdx(*(const uint16_t*)b, cb);
  return ci_strcmp(ca.name, cb.name);
}
static char     g_cfilter[24] = "";              // contacts name filter ("" = all)
static bool     g_fav_only = false;              // show only favourites when set
static uint16_t g_corder[MAX_CONTACTS];
static int      g_corder_n = -1;
static int      g_corder_total = -1;             // contact count the order was built for
static char     g_corder_filter[24] = "\x01";    // filter it was built for (impossible init)
static int      g_corder_fav = -1;               // fav-only mode the order was built for

// case-insensitive "needle is a substring of hay"
static bool ci_contains(const char* hay, const char* needle) {
  if (!needle[0]) return true;
  for (; *hay; hay++) {
    const char* h = hay; const char* n = needle;
    while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) { h++; n++; }
    if (!*n) return true;
  }
  return false;
}
// match `hay` against a search string of space-separated tokens, OR-combined
// (e.g. "sky hull" matches names containing "sky" OR "hull"). Empty => match all.
extern "C" bool lvd_name_match(const char* hay, const char* needle) {
  if (!needle || !needle[0]) return true;
  const char* p = needle;
  bool saw_token = false;
  while (*p) {
    while (*p == ' ') p++;
    if (!*p) break;
    const char* s = p;
    while (*p && *p != ' ') p++;
    int len = (int)(p - s); if (len > 31) len = 31;
    char tok[32]; memcpy(tok, s, len); tok[len] = 0;
    saw_token = true;
    if (ci_contains(hay, tok)) return true;
  }
  return !saw_token;   // an all-spaces query matches everything
}
static void build_contact_order(void) {
  int total = the_mesh.getNumContacts();
  if (total > MAX_CONTACTS) total = MAX_CONTACTS;
  if (total == g_corder_total && g_corder_fav == (int)g_fav_only &&
      strcmp(g_cfilter, g_corder_filter) == 0) return;   // cached
  int m = 0;
  for (int i = 0; i < total; i++) {
    ContactInfo c;
    if (!the_mesh.getContactByIdx((uint32_t)i, c)) continue;
    if (g_fav_only && !(c.flags & 0x01)) continue;       // favourites-only filter
    if (lvd_name_match(c.name, g_cfilter)) g_corder[m++] = (uint16_t)i;
  }
  qsort(g_corder, m, sizeof(g_corder[0]), cmp_contact_idx);
  g_corder_n = m;
  g_corder_total = total;
  g_corder_fav = (int)g_fav_only;
  strncpy(g_corder_filter, g_cfilter, sizeof(g_corder_filter) - 1); g_corder_filter[sizeof(g_corder_filter) - 1] = 0;
}
extern "C" void        lvd_contact_set_filter(const char* s) {
  strncpy(g_cfilter, s ? s : "", sizeof(g_cfilter) - 1); g_cfilter[sizeof(g_cfilter) - 1] = 0;
}
extern "C" const char* lvd_contact_filter(void) { return g_cfilter; }
extern "C" void        lvd_contact_set_fav_only(int on) { g_fav_only = (on != 0); }
extern "C" int         lvd_contact_fav_only(void) { return g_fav_only ? 1 : 0; }
extern "C" int         lvd_contact_total(void)  { return the_mesh.getNumContacts(); }

extern "C" int lvd_contact_count(void) {
  build_contact_order();
  return g_corder_n;
}
extern "C" bool lvd_contact_get(int i, lvd_contact_t* out) {
  build_contact_order();
  ContactInfo c;
  if (i < 0 || i >= g_corder_n || !the_mesh.getContactByIdx(g_corder[i], c)) return false;
  strncpy(out->name, c.name, sizeof(out->name) - 1); out->name[sizeof(out->name) - 1] = 0;
  out->type = c.type;
  out->fav = (c.flags & 0x01) ? 1 : 0;   // LSB of flags is the favourite bit

  const char* t = c.type == ADV_TYPE_CHAT     ? "Chat"     :
                  c.type == ADV_TYPE_REPEATER ? "Repeater" :
                  c.type == ADV_TYPE_ROOM     ? "Room"     :
                  c.type == ADV_TYPE_SENSOR   ? "Sensor"   : "Node";
  if (c.out_path_len == OUT_PATH_UNKNOWN)
    snprintf(out->subtitle, sizeof(out->subtitle), "%s / flood", t);
  else if (c.out_path_len == 0)
    snprintf(out->subtitle, sizeof(out->subtitle), "%s / direct", t);
  else
    snprintf(out->subtitle, sizeof(out->subtitle), "%s / %u hops", t, (unsigned)c.out_path_len);
  return true;
}

// ---- settings / device config ----------------------------------------------
// Only the fields bound here are live; the Settings screen keeps prototype
// values for everything else. Keyed by (group title, field label) so the C
// screen and this map stay decoupled from field ordering.
static bool eq(const char* a, const char* b) { return strcmp(a, b) == 0; }

// LoRa bandwidth options (kHz) matching the Radio > Bandwidth dropdown order
static const float BW_KHZ[] = {7.8f, 10.4f, 15.6f, 20.8f, 31.25f, 41.7f, 62.5f, 125, 250, 500};
#define BW_N ((int)(sizeof(BW_KHZ) / sizeof(BW_KHZ[0])))

// setRadioParams takes freq in kHz and bw in Hz; it validates all ranges and
// only applies (+persists) on success, so a bad value is a safe no-op.
static bool apply_radio(float freq_mhz, float bw_khz, uint8_t sf, uint8_t cr, uint8_t repeat) {
  return the_mesh.setRadioParams((uint32_t)(freq_mhz * 1000.0f + 0.5f),
                                 (uint32_t)(bw_khz * 1000.0f + 0.5f), sf, cr, repeat);
}

// On-screen keyboard preference (UI-only, persisted in NVS so it survives reboots
// and is independent of the device-prefs file). Default on.
static bool g_osk_loaded = false, g_osk_on = true;
extern "C" bool lvd_osk_enabled(void) {
  if (!g_osk_loaded) { Preferences p; p.begin("ui", true); g_osk_on = p.getBool("osk", true); p.end(); g_osk_loaded = true; }
  return g_osk_on;
}
extern "C" void lvd_osk_set(bool on) {
  g_osk_on = on; g_osk_loaded = true;
  Preferences p; p.begin("ui", false); p.putBool("osk", on); p.end();
}

// auto-add bitmask bit for a Contacts toggle label (0 if not an auto-add field)
static uint8_t autoadd_bit(const char* label) {
  if (eq(label, "Overwrite oldest"))   return 0x01;
  if (eq(label, "Auto-add users"))     return 0x02;
  if (eq(label, "Auto-add repeaters")) return 0x04;
  if (eq(label, "Auto-add rooms"))     return 0x08;
  if (eq(label, "Auto-add sensors"))   return 0x10;
  return 0;
}

extern "C" bool lvd_cfg_get(const char* group, const char* label, char* val, int len, int* sel) {
  NodePrefs* p = the_mesh.getNodePrefs();
  if (eq(group, "Public info")) {
    if (eq(label, "Node name"))       { strncpy(val, p->node_name, len - 1); val[len - 1] = 0; return true; }
    if (eq(label, "Location policy")) { *sel = p->advert_loc_policy ? 1 : 0; return true; }
  } else if (eq(group, "Radio")) {
    if (eq(label, "TX power"))         { snprintf(val, len, "%d", p->tx_power_dbm); return true; }
    if (eq(label, "Frequency"))        { snprintf(val, len, "%.3f", p->freq); return true; }
    if (eq(label, "Spreading factor")) { snprintf(val, len, "%u", p->sf); return true; }
    if (eq(label, "Coding rate"))      { snprintf(val, len, "%u", p->cr); return true; }
    if (eq(label, "Client repeat"))    { *sel = p->client_repeat ? 1 : 0; return true; }
    if (eq(label, "Channel activity detection")) { *sel = p->cad_enabled ? 1 : 0; return true; }
    if (eq(label, "Bandwidth")) {
      int best = 6; float bd = 1e9f;
      for (int i = 0; i < BW_N; i++) { float d = p->bw - BW_KHZ[i]; if (d < 0) d = -d; if (d < bd) { bd = d; best = i; } }
      *sel = best; return true;
    }
  } else if (eq(group, "Contacts")) {
    if (eq(label, "Manual add"))        { *sel = p->manual_add_contacts ? 1 : 0; return true; }
    if (eq(label, "Auto-add max hops")) { snprintf(val, len, "%u", p->autoadd_max_hops); return true; }
    uint8_t bit = autoadd_bit(label);
    if (bit) { *sel = (p->autoadd_config & bit) ? 1 : 0; return true; }
  } else if (eq(group, "Messaging")) {
    if (eq(label, "Multi-acks"))     { snprintf(val, len, "%u", p->multi_acks); return true; }
    if (eq(label, "Path hash mode")) { *sel = p->path_hash_mode <= 2 ? p->path_hash_mode : 2; return true; }
  } else if (eq(group, "Position")) {
    if (eq(label, "Latitude"))    { snprintf(val, len, "%.4f", sensors.node_lat); return true; }
    if (eq(label, "Longitude"))   { snprintf(val, len, "%.4f", sensors.node_lon); return true; }
    if (eq(label, "GPS enabled")) { *sel = p->gps_enabled ? 1 : 0; return true; }
    if (eq(label, "GPS interval")){ snprintf(val, len, "%u", (unsigned)p->gps_interval); return true; }
  } else if (eq(group, "Tuning")) {
    if (eq(label, "Airtime factor")) { snprintf(val, len, "%.2f", p->airtime_factor); return true; }
    if (eq(label, "RX delay base"))  { snprintf(val, len, "%.0f", p->rx_delay_base); return true; }
    if (eq(label, "Default scope"))  { snprintf(val, len, "%s", p->default_scope_name[0] ? p->default_scope_name : "(none)"); return true; }
  } else if (eq(group, "Security")) {
    if (eq(label, "BLE pin")) { snprintf(val, len, "%u", (unsigned)p->ble_pin); return true; }
  } else if (eq(group, "Device")) {
    if (eq(label, "Battery/storage")) {
      uint16_t mv; uint32_t used, total; the_mesh.getBattAndStorage(mv, used, total);
      snprintf(val, len, "%u mV  %u/%u KB", (unsigned)mv, (unsigned)used, (unsigned)total);
      return true;
    }
    if (eq(label, "Firmware")) { strncpy(val, FIRMWARE_VERSION, len - 1); val[len - 1] = 0; return true; }
    if (eq(label, "Device"))   { strncpy(val, board.getManufacturerName(), len - 1); val[len - 1] = 0; return true; }
    if (eq(label, "On-screen keyboard")) { *sel = lvd_osk_enabled() ? 1 : 0; return true; }
  }
  return false;
}

extern "C" void lvd_cfg_set(const char* group, const char* label, const char* val, int sel) {
  NodePrefs* p = the_mesh.getNodePrefs();
  if (eq(group, "Public info")) {
    if (eq(label, "Node name") && val)  { the_mesh.setAdvertName(val); return; }
    if (eq(label, "Location policy"))   { the_mesh.setAdvertLocPolicy((uint8_t)sel); return; }
  } else if (eq(group, "Radio")) {
    if (eq(label, "TX power") && val)          { the_mesh.setTxPower((int8_t)atoi(val)); return; }
    if (eq(label, "Frequency") && val)         { apply_radio((float)atof(val), p->bw, p->sf, p->cr, p->client_repeat); return; }
    if (eq(label, "Spreading factor") && val)  { apply_radio(p->freq, p->bw, (uint8_t)atoi(val), p->cr, p->client_repeat); return; }
    if (eq(label, "Coding rate") && val)       { apply_radio(p->freq, p->bw, p->sf, (uint8_t)atoi(val), p->client_repeat); return; }
    if (eq(label, "Client repeat"))            { apply_radio(p->freq, p->bw, p->sf, p->cr, (uint8_t)(sel != 0)); return; }
    if (eq(label, "Channel activity detection")) { the_mesh.setCADEnabled(sel != 0); return; }
    if (eq(label, "Bandwidth") && sel >= 0 && sel < BW_N) { apply_radio(p->freq, BW_KHZ[sel], p->sf, p->cr, p->client_repeat); return; }
  } else if (eq(group, "Contacts")) {
    if (eq(label, "Manual add"))                { the_mesh.setManualAdd(sel != 0); return; }
    if (eq(label, "Auto-add max hops") && val)  { the_mesh.setMaxHops((uint8_t)atoi(val)); return; }
    uint8_t bit = autoadd_bit(label);
    if (bit) { uint8_t m = p->autoadd_config; if (sel) m |= bit; else m &= ~bit; the_mesh.setAutoAddConfig(m); return; }
  } else if (eq(group, "Messaging")) {
    if (eq(label, "Multi-acks") && val)  { the_mesh.setMultiAcks((uint8_t)atoi(val)); return; }
    if (eq(label, "Path hash mode"))     { the_mesh.setPathHashMode((uint8_t)sel); return; }
  } else if (eq(group, "Position")) {
    if (eq(label, "Latitude") && val)  { the_mesh.setAdvertLatLon((int32_t)(atof(val) * 1e6), (int32_t)(sensors.node_lon * 1e6)); return; }
    if (eq(label, "Longitude") && val) { the_mesh.setAdvertLatLon((int32_t)(sensors.node_lat * 1e6), (int32_t)(atof(val) * 1e6)); return; }
#if ENV_INCLUDE_GPS == 1
    if (eq(label, "GPS enabled"))        { the_mesh.setGpsEnabled(sel != 0); return; }
    if (eq(label, "GPS interval") && val){ the_mesh.setGpsInterval((uint32_t)atoi(val)); return; }
#endif
  } else if (eq(group, "Tuning")) {
    if (eq(label, "Airtime factor") && val) { the_mesh.setTuningParams(p->rx_delay_base, (float)atof(val)); return; }
    if (eq(label, "RX delay base") && val)  { the_mesh.setTuningParams((float)atof(val), p->airtime_factor); return; }
  } else if (eq(group, "Security")) {
    if (eq(label, "BLE pin") && val) { the_mesh.setBlePin((uint32_t)strtoul(val, NULL, 10)); return; }
  } else if (eq(group, "Device")) {
    if (eq(label, "On-screen keyboard")) { lvd_osk_set(sel != 0); return; }
  }
}

extern "C" void lvd_cfg_action(const char* group, const char* label) {
  if (eq(group, "Public info")) {
    if (eq(label, "Send advert"))         { the_mesh.advert();      return; }
    if (eq(label, "Send advert (flood)")) { the_mesh.advertFlood(); return; }
  } else if (eq(group, "Tuning")) {
    if (eq(label, "Clear default scope")) { the_mesh.setDefaultFloodScope("", NULL); return; }
  } else if (eq(group, "Device")) {
    if (eq(label, "Reboot")) { the_mesh.rebootDevice(); return; }
    // Factory reset intentionally NOT wired yet -- needs a confirm dialog first.
  }
}

// ---- statistics ------------------------------------------------------------
#define STATS_HIST 40
static int s_noise_hist[STATS_HIST];
static int s_noise_head = 0;   // next write slot
static int s_noise_n    = 0;   // valid samples

extern "C" void lvd_stats_get(lvd_stats_t* out) {
  int nf = radio_driver.getNoiseFloor();
  s_noise_hist[s_noise_head] = nf;
  s_noise_head = (s_noise_head + 1) % STATS_HIST;
  if (s_noise_n < STATS_HIST) s_noise_n++;

  int mn = 0, mx = 0; bool first = true;
  for (int i = 0; i < s_noise_n; i++) {
    int v = s_noise_hist[i];
    if (v == 0) continue;            // skip "unknown" samples
    if (first || v < mn) mn = v;
    if (first || v > mx) mx = v;
    first = false;
  }
  out->noise_floor = nf;
  out->noise_min = mn;
  out->noise_max = mx;
  out->last_rssi = (int)radio_driver.getLastRSSI();
  out->last_snr_q = (int)(radio_driver.getLastSNR() * 10.0f);
  out->pkt_recv = radio_driver.getPacketsRecv();
  out->pkt_sent = radio_driver.getPacketsSent();
  out->pkt_recv_err = radio_driver.getPacketsRecvErrors();
  uint16_t mv; uint32_t used, total; the_mesh.getBattAndStorage(mv, used, total);
  out->batt_mv = mv;
  out->uptime_secs = millis() / 1000;
}

extern "C" int lvd_stats_noise_history(int* out, int max) {
  int n = s_noise_n < max ? s_noise_n : max;
  for (int i = 0; i < n; i++) {
    int idx = (s_noise_head - n + i + STATS_HIST * 2) % STATS_HIST;
    out[i] = s_noise_hist[idx];
  }
  return n;
}

extern "C" int      lvd_noise_floor(void) { return radio_driver.getNoiseFloor(); }
extern "C" int      lvd_rf_rssi(void)     { return (int)radio_driver.getCurrentRSSI(); }  // instantaneous channel RSSI
extern "C" unsigned lvd_pkt_recv(void)    { return radio_driver.getPacketsRecv(); }
extern "C" unsigned lvd_pkt_sent(void)    { return radio_driver.getPacketsSent(); }
extern "C" unsigned lvd_pkt_recv_err(void) { return radio_driver.getPacketsRecvErrors(); }
extern "C" int      lvd_last_rssi(void)    { return (int)radio_driver.getLastRSSI(); }
extern "C" int      lvd_last_snr_q(void)   { return (int)(radio_driver.getLastSNR() * 4.0f); }
extern "C" unsigned lvd_free_ram_kb(void)  { return (unsigned)(ESP.getFreeHeap() / 1024); }
extern "C" unsigned lvd_free_flash_kb(void) {
  uint16_t mv; uint32_t used, total; the_mesh.getBattAndStorage(mv, used, total);
  return (unsigned)(total > used ? total - used : 0);
}

// ---- packet monitor --------------------------------------------------------
#define PKT_LOG 32
#define PKT_RAW 192   // bytes of each packet kept for the hex/breakdown view
struct PktRec { uint8_t header; int rssi; int snr_q; int len; uint32_t t; uint8_t raw[PKT_RAW]; uint8_t rawlen; };
static PktRec s_pkt[PKT_LOG];
static int s_pkt_head = 0, s_pkt_n = 0;
static unsigned s_pkt_total = 0;   // monotonic, for change detection

void ui_log_packet(float snr, float rssi, const uint8_t* raw, int len) {
  PktRec& r = s_pkt[s_pkt_head];
  r.header = (len > 0) ? raw[0] : 0;
  r.rssi = (int)rssi;
  r.snr_q = (int)(snr * 4.0f);
  r.len = len;
  r.t = millis();
  r.rawlen = (uint8_t)(len > PKT_RAW ? PKT_RAW : (len < 0 ? 0 : len));
  if (raw && r.rawlen) memcpy(r.raw, raw, r.rawlen);
  s_pkt_head = (s_pkt_head + 1) % PKT_LOG;
  if (s_pkt_n < PKT_LOG) s_pkt_n++;
  s_pkt_total++;
}

// resolve a 1-byte path hash (== a node's pub_key[0]) to a saved contact name
static const char* contact_name_by_hash(uint8_t h) {
  static char nm[32];
  int n = the_mesh.getNumContacts();
  for (int i = 0; i < n; i++) {
    ContactInfo c;
    if (the_mesh.getContactByIdx((uint32_t)i, c) && c.id.pub_key[0] == h) {
      strncpy(nm, c.name, sizeof(nm) - 1); nm[sizeof(nm) - 1] = 0; return nm;
    }
  }
  return NULL;
}
// friendly path string for a packet ("direct", or "A > B > 7F"; "" if no path field)
static void packet_path_str(const PktRec& p, char* v, int vlen) {
  const uint8_t* b = p.raw; int rl = p.rawlen;
  if (rl < 1) { v[0] = 0; return; }
  int route = b[0] & 0x03; bool transport = (route == 0 || route == 3);
  int off = 1;
  if (transport && rl >= 5) off = 5;
  uint8_t plf = (off < rl) ? b[off] : 0; off++;
  int hcount = plf & 63, hsize = (plf >> 6) + 1;
  if (hcount == 0) { snprintf(v, vlen, "direct"); return; }
  int k = 0;
  for (int j = 0; j < hcount && off + j * hsize < rl; j++) {
    uint8_t hb = b[off + j * hsize];
    const char* nm = contact_name_by_hash(hb);
    const char* sep = (j == 0) ? "" : " > ";
    if (nm) k += snprintf(v + k, vlen - k, "%s%s", sep, nm);
    else    k += snprintf(v + k, vlen - k, "%s%02X", sep, hb);
    if (k >= vlen - 8) { snprintf(v + k, vlen - k, " .."); break; }
  }
}

// path text filter for the monitor list
static char g_pkt_filter[24] = "";
static int  g_pktidx[PKT_LOG];   // ring indices passing the filter, newest first
static int  g_pktidx_n = 0;

static void pkt_build_filter(void) {
  g_pktidx_n = 0;
  for (int i = 0; i < s_pkt_n; i++) {
    int idx = (s_pkt_head - 1 - i + PKT_LOG * 2) % PKT_LOG;   // newest first
    if (g_pkt_filter[0]) {
      char path[96]; packet_path_str(s_pkt[idx], path, sizeof(path));
      if (!lvd_name_match(path, g_pkt_filter)) continue;
    }
    g_pktidx[g_pktidx_n++] = idx;
  }
}
extern "C" void        lvd_packet_set_path_filter(const char* s) {
  strncpy(g_pkt_filter, s ? s : "", sizeof(g_pkt_filter) - 1); g_pkt_filter[sizeof(g_pkt_filter) - 1] = 0;
}
extern "C" const char* lvd_packet_path_filter(void) { return g_pkt_filter; }

extern "C" int      lvd_packet_count(void) { pkt_build_filter(); return g_pktidx_n; }
extern "C" unsigned lvd_packet_total(void) { return s_pkt_total; }

extern "C" bool lvd_packet_get(int i, lvd_packet_t* out) {
  if (i < 0 || i >= g_pktidx_n) return false;
  int idx = g_pktidx[i];
  const PktRec& r = s_pkt[idx];

  uint8_t pt = (r.header >> 2) & 0x0F;   // PH_TYPE_SHIFT / PH_TYPE_MASK
  const char* ty; uint32_t col;
  switch (pt) {
    case 0x02: ty = "TXT"; col = UI_BLUE;   break;   // PAYLOAD_TYPE_TXT_MSG
    case 0x03: ty = "ACK"; col = UI_GREEN;  break;   // ACK
    case 0x04: ty = "ADV"; col = UI_PURPLE; break;   // ADVERT
    case 0x05: case 0x06: ty = "GRP"; col = UI_CYAN; break;  // GRP_TXT / GRP_DATA
    case 0x08: ty = "PTH"; col = UI_INDIGO; break;   // PATH
    case 0x09: ty = "TRC"; col = UI_AMBER;  break;   // TRACE
    case 0x00: ty = "REQ"; col = UI_MUTED;  break;
    case 0x01: ty = "RSP"; col = UI_MUTED;  break;
    case 0x07: ty = "ANR"; col = UI_MUTED;  break;
    case 0x0B: ty = "CTL"; col = UI_TEAL;   break;   // CONTROL
    default:   ty = "?";   col = UI_MUTED;  break;
  }
  strncpy(out->type, ty, sizeof(out->type) - 1); out->type[sizeof(out->type) - 1] = 0;
  out->color = col;

  static const char* RT[4] = { "TFL", "FLD", "DIR", "TDR" };  // route type (header & 0x03)
  const char* rt = RT[r.header & 0x03];
  int snr10 = r.snr_q * 10 / 4;   // snr*10 for "%d.%d"
  int sa = snr10 < 0 ? -snr10 : snr10;
  uint32_t age = (millis() - r.t) / 1000;
  char agebuf[10];
  if (age < 60) snprintf(agebuf, sizeof(agebuf), "%us", (unsigned)age);
  else          snprintf(agebuf, sizeof(agebuf), "%um", (unsigned)(age / 60));
  snprintf(out->meta, sizeof(out->meta), "%s  len%d  %ddBm  %s%d.%d  %s",
           rt, r.len, r.rssi, snr10 < 0 ? "-" : "", sa / 10, sa % 10, agebuf);
  return true;
}

// ---- packet detail (tap a row in the monitor) ------------------------------
static PktRec s_psel;
static bool   s_psel_valid = false;

extern "C" void lvd_packet_select(int i) {
  if (i < 0 || i >= g_pktidx_n) { s_psel_valid = false; return; }
  s_psel = s_pkt[g_pktidx[i]];
  s_psel_valid = true;
}

static const char* route_name(int r) {
  return r == 0 ? "Transport-flood" : r == 1 ? "Flood" : r == 2 ? "Direct" : "Transport-direct";
}
static const char* ptype_name(int t) {
  switch (t) {
    case 0x00: return "Request";    case 0x01: return "Response";  case 0x02: return "Text";
    case 0x03: return "Ack";        case 0x04: return "Advert";    case 0x05: return "Group text";
    case 0x06: return "Group data"; case 0x07: return "Anon req";  case 0x08: return "Path";
    case 0x09: return "Trace";      case 0x0A: return "Multipart"; case 0x0B: return "Control";
    case 0x0F: return "Raw custom"; default:   return "Unknown";
  }
}
static const char* contact_name_by_pubkey6(const uint8_t* pk6) {
  static char nm[32];
  int n = the_mesh.getNumContacts();
  for (int i = 0; i < n; i++) {
    ContactInfo c;
    if (the_mesh.getContactByIdx((uint32_t)i, c) && memcmp(c.id.pub_key, pk6, 6) == 0) {
      strncpy(nm, c.name, sizeof(nm) - 1); nm[sizeof(nm) - 1] = 0; return nm;
    }
  }
  return NULL;
}
static void kv_add(lvd_kv_t* out, int* n, int max, const char* label, const char* value) {
  if (*n >= max) return;
  strncpy(out[*n].label, label, sizeof(out[*n].label) - 1); out[*n].label[sizeof(out[*n].label) - 1] = 0;
  strncpy(out[*n].value, value, sizeof(out[*n].value) - 1); out[*n].value[sizeof(out[*n].value) - 1] = 0;
  (*n)++;
}

extern "C" int lvd_packet_detail(lvd_kv_t* out, int max) {
  if (!s_psel_valid) return 0;
  const PktRec& p = s_psel;
  const uint8_t* b = p.raw;
  int rl = p.rawlen;
  uint8_t hdr = rl > 0 ? b[0] : 0;
  int route = hdr & 0x03, ptype = (hdr >> 2) & 0x0F, ver = (hdr >> 6) & 0x03;
  bool transport = (route == 0 || route == 3);
  char v[88];
  int n = 0;

  kv_add(out, &n, max, "Type", ptype_name(ptype));
  snprintf(v, sizeof(v), "%s (v%d)", route_name(route), ver);       kv_add(out, &n, max, "Route", v);

  int off = 1;   // header [+4 transport] path_len path... payload
  if (transport && rl >= 5) {
    uint16_t t0 = b[1] | (b[2] << 8), t1 = b[3] | (b[4] << 8);
    snprintf(v, sizeof(v), "0x%04X, 0x%04X", t0, t1);              kv_add(out, &n, max, "Transport", v);
    off = 5;
  }
  uint8_t plf = (off < rl) ? b[off] : 0;
  off++;
  int hcount = plf & 63, hsize = (plf >> 6) + 1, pbytes = hcount * hsize;
  if (hcount == 0) snprintf(v, sizeof(v), "direct (0 hops)");
  else {
    int k = 0;   // chain of friendly names (hex byte where unknown), e.g. "GW > Hilltop > 7F"
    for (int j = 0; j < hcount && off + j * hsize < rl; j++) {
      uint8_t hb = b[off + j * hsize];
      const char* nm = contact_name_by_hash(hb);
      const char* sep = (j == 0) ? "" : " > ";
      if (nm) k += snprintf(v + k, sizeof(v) - k, "%s%s", sep, nm);
      else    k += snprintf(v + k, sizeof(v) - k, "%s%02X", sep, hb);
      if (k >= (int)sizeof(v) - 8) { snprintf(v + k, sizeof(v) - k, " .."); break; }
    }
  }
  kv_add(out, &n, max, "Path", v);
  int payoff = off + pbytes;
  int paylen = p.len - payoff; if (paylen < 0) paylen = 0;
  const uint8_t* pl = b + payoff;
  int plav = rl - payoff; if (plav < 0) plav = 0;

  switch (ptype) {
    case 0x00: case 0x01: case 0x02: case 0x08:   // REQ/RESP/TXT/PATH: dest+src hash
      if (plav >= 1) { snprintf(v, sizeof(v), "0x%02X", pl[0]); kv_add(out, &n, max, "Dest", v); }
      if (plav >= 2) { snprintf(v, sizeof(v), "0x%02X", pl[1]); kv_add(out, &n, max, "Source", v); }
      break;
    case 0x03:                                    // ACK
      if (plav >= 1) { int k = snprintf(v, sizeof(v), "0x"); for (int j = 0; j < plav && j < 8 && k < (int)sizeof(v) - 2; j++) k += snprintf(v + k, sizeof(v) - k, "%02X", pl[j]); kv_add(out, &n, max, "ACK code", v); }
      break;
    case 0x05: case 0x06:                          // GRP_TXT/GRP_DATA: channel hash
      if (plav >= 1) { snprintf(v, sizeof(v), "0x%02X", pl[0]); kv_add(out, &n, max, "Channel", v); }
      break;
    case 0x04:                                     // ADVERT: advertiser pubkey
      if (plav >= 6) {
        const char* nm = contact_name_by_pubkey6(pl);
        if (nm) snprintf(v, sizeof(v), "%s (%02X%02X%02X..)", nm, pl[0], pl[1], pl[2]);
        else    snprintf(v, sizeof(v), "%02X%02X%02X%02X%02X%02X..", pl[0], pl[1], pl[2], pl[3], pl[4], pl[5]);
        kv_add(out, &n, max, "Advertiser", v);
      }
      break;
    case 0x09:                                     // TRACE: tag
      if (plav >= 4) { uint32_t tag = pl[0] | (pl[1] << 8) | (pl[2] << 16) | ((uint32_t)pl[3] << 24); snprintf(v, sizeof(v), "0x%08X", (unsigned)tag); kv_add(out, &n, max, "Trace tag", v); }
      break;
    default: break;
  }

  { int v10 = p.snr_q * 10 / 4, a = v10 < 0 ? -v10 : v10;
    snprintf(v, sizeof(v), "%s%d.%d dB / %d dBm", v10 < 0 ? "-" : "", a / 10, a % 10, p.rssi);
    kv_add(out, &n, max, "SNR / RSSI", v); }
  snprintf(v, sizeof(v), "%d B (payload %d B)", p.len, paylen);     kv_add(out, &n, max, "Length", v);
  snprintf(v, sizeof(v), "0x%02X", hdr);                            kv_add(out, &n, max, "Header", v);
  { uint32_t age = (millis() - p.t) / 1000;
    if (age < 60) snprintf(v, sizeof(v), "%us ago", (unsigned)age);
    else          snprintf(v, sizeof(v), "%um ago", (unsigned)(age / 60));
    kv_add(out, &n, max, "Received", v); }
  return n;
}

extern "C" const char* lvd_packet_hex(void) {
  static char hex[PKT_RAW * 3 + 8];
  if (!s_psel_valid) { hex[0] = 0; return hex; }
  int k = 0;
  for (int i = 0; i < s_psel.rawlen && k < (int)sizeof(hex) - 4; i++)
    k += snprintf(hex + k, sizeof(hex) - k, "%02X ", s_psel.raw[i]);
  if (s_psel.len > s_psel.rawlen && k < (int)sizeof(hex) - 4) snprintf(hex + k, sizeof(hex) - k, "...");
  return hex;
}

// ---- discover (heard-but-unsaved nodes) ------------------------------------
static const char* adv_type_name(int t) {
  return t == ADV_TYPE_CHAT     ? "Companion" :
         t == ADV_TYPE_REPEATER ? "Repeater"  :
         t == ADV_TYPE_ROOM     ? "Room"      :
         t == ADV_TYPE_SENSOR   ? "Sensor"    : "Node";
}

// Discover = responders to our active NODE_DISCOVER_REQ (not passive adverts).
static MyMesh::DiscNode g_disc[16];
static int              g_disc_n = 0;

static int cmp_disc_snr(const void* a, const void* b) {   // strongest first
  return ((const MyMesh::DiscNode*)b)->snr_q - ((const MyMesh::DiscNode*)a)->snr_q;
}
// best-effort label: the saved contact's name for this key, else its hex prefix
static void disc_label(const uint8_t* pk, char* out, int len) {
  int n = the_mesh.getNumContacts();
  for (int k = 0; k < n; k++) {
    ContactInfo c;
    if (the_mesh.getContactByIdx((uint32_t)k, c) &&
        memcmp(c.id.pub_key, pk, PUB_KEY_SIZE) == 0 && c.name[0]) {
      strncpy(out, c.name, len - 1); out[len - 1] = 0; return;
    }
  }
  snprintf(out, len, "%02X%02X%02X%02X", pk[0], pk[1], pk[2], pk[3]);
}

extern "C" int lvd_disc_count(void) {
  g_disc_n = the_mesh.getDiscoveredNodes(g_disc, 16);
  qsort(g_disc, g_disc_n, sizeof(g_disc[0]), cmp_disc_snr);
  return g_disc_n;
}
extern "C" bool lvd_disc_get(int i, lvd_disc_t* out) {
  if (i < 0 || i >= g_disc_n) return false;
  MyMesh::DiscNode& d = g_disc[i];
  disc_label(d.pub_key, out->name, sizeof(out->name));
  out->type = d.type;
  int v10 = d.snr_q * 10 / 4, a = v10 < 0 ? -v10 : v10;
  snprintf(out->subtitle, sizeof(out->subtitle), "%s - SNR %s%d.%d dB",
           adv_type_name(d.type), v10 < 0 ? "-" : "", a / 10, a % 10);
  return true;
}
// Paced active discovery: repeaters rate-limit and will ignore us if we ask too
// often, so enforce a single global 60s minimum between requests regardless of
// caller (screen entry, the 60s auto-poll, or the manual button). Returns 0 when
// a request was actually sent, else the seconds remaining until the next is allowed.
#define DISC_REQ_MIN_MS 60000UL
extern "C" int lvd_disc_request(void) {
  static uint32_t last_ms = 0;
  static bool     ever    = false;
  uint32_t now = millis();
  if (ever && (now - last_ms) < DISC_REQ_MIN_MS)
    return (int)((DISC_REQ_MIN_MS - (now - last_ms) + 999) / 1000);
  the_mesh.sendNodeDiscoverReq();
  last_ms = now; ever = true;
  return 0;
}
extern "C" void lvd_disc_clear(void)   { the_mesh.clearDiscoveredNodes(); }
extern "C" void lvd_disc_announce(void) { the_mesh.advert(); }            // zero-hop self-advert
extern "C" void lvd_disc_announce_flood(void) { the_mesh.advertFlood(); } // flood-routed self-advert

// ---- channels (channel management) -----------------------------------------
// Standard base64 encode (the base64 lib defines symbols in its header, so it
// can't be included here too; we only need encode for displaying a PSK).
static int b64_encode(const uint8_t* in, int n, char* out) {
  static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  int o = 0;
  for (int i = 0; i < n; i += 3) {
    int b0 = in[i], b1 = (i + 1 < n) ? in[i + 1] : 0, b2 = (i + 2 < n) ? in[i + 2] : 0;
    out[o++] = T[b0 >> 2];
    out[o++] = T[((b0 & 3) << 4) | (b1 >> 4)];
    out[o++] = (i + 1 < n) ? T[((b1 & 15) << 2) | (b2 >> 6)] : '=';
    out[o++] = (i + 2 < n) ? T[b2 & 63] : '=';
  }
  out[o] = 0;
  return o;
}
// A compacted index of occupied channel slots (idx 0 is the Public channel).
static int g_chan_idx[MAX_GROUP_CHANNELS];
static int g_chan_n = 0;
static bool chan_occupied(const ChannelDetails& c) {
  if (c.name[0]) return true;
  for (size_t i = 0; i < sizeof(c.channel.secret); i++) if (c.channel.secret[i]) return true;
  return false;
}
static bool chan_is128(const ChannelDetails& c) {   // 128-bit key = upper 16 bytes zero
  for (int k = 16; k < 32; k++) if (c.channel.secret[k]) return false;
  return true;
}
static void chan_build(void) {
  g_chan_n = 0;
  for (int i = 0; i < MAX_GROUP_CHANNELS && g_chan_n < MAX_GROUP_CHANNELS; i++) {
    ChannelDetails c;
    if (the_mesh.getChannel(i, c) && chan_occupied(c)) g_chan_idx[g_chan_n++] = i;
  }
}
extern "C" int lvd_chan_count(void) { chan_build(); return g_chan_n; }
extern "C" bool lvd_chan_get(int i, lvd_chan_t* out) {
  if (i < 0 || i >= g_chan_n) return false;
  ChannelDetails c; the_mesh.getChannel(g_chan_idx[i], c);
  const char* nm = c.name[0] ? c.name : "(unnamed)";
  strncpy(out->name, nm, sizeof(out->name) - 1); out->name[sizeof(out->name) - 1] = 0;
  snprintf(out->info, sizeof(out->info), "%s%s", g_chan_idx[i] == 0 ? "Public - " : "",
           chan_is128(c) ? "128-bit key" : "256-bit key");
  out->is_public = (g_chan_idx[i] == 0) ? 1 : 0;
  return true;
}
extern "C" bool lvd_chan_add(const char* name, const char* psk_b64) {
  if (!name || !*name || !psk_b64) return false;
  return the_mesh.uiAddChannel(name, psk_b64);
}
extern "C" bool lvd_chan_remove(int i) {
  if (i < 0 || i >= g_chan_n || g_chan_idx[i] == 0) return false;   // never remove Public
  return the_mesh.uiRemoveChannel(g_chan_idx[i]);
}
extern "C" const char* lvd_chan_psk(int i) {     // the channel's PSK as base64 (for sharing/QR)
  static char b64[50]; b64[0] = 0;
  if (i < 0 || i >= g_chan_n) return b64;
  ChannelDetails c; the_mesh.getChannel(g_chan_idx[i], c);
  b64_encode(c.channel.secret, chan_is128(c) ? 16 : 32, b64);
  return b64;
}
extern "C" const char* lvd_chan_new_psk(void) {  // a fresh random 128-bit key (base64), for new channels
  static char b64[50];
  uint8_t key[16];
  for (int i = 0; i < 16; i++) key[i] = (uint8_t)esp_random();
  b64_encode(key, 16, b64);
  return b64;
}
extern "C" const char* lvd_chan_hashtag_psk(const char* name) {  // hashtag channel key: first 16 bytes of sha256(name)
  static char b64[50];
  uint8_t key[16];
  mesh::Utils::sha256(key, sizeof(key), (const uint8_t*)name, strlen(name));
  b64_encode(key, 16, b64);
  return b64;
}

// ---- chat store (Public channel + DMs) -------------------------------------
#define MSG_LOG 32
struct MsgRec { bool is_ch; int ch; uint8_t peer6[6]; char who[24]; char text[124]; bool out; };
static MsgRec s_msg[MSG_LOG];
static int s_msg_head = 0, s_msg_n = 0;
static unsigned s_msg_total = 0;

// active conversation: a channel (s_conv_ch >= 0 = channel slot, 0 = Public) or a DM
static int         s_conv_ch = 0;
static uint8_t     s_conv_peer[6];
static ContactInfo s_conv_contact;
static bool        s_conv_has_contact = false;
static char        s_conv_chname[32] = "Public";

void ui_store_message(bool is_ch, int ch, const uint8_t* peer6, const char* who, const char* text, bool out) {
  MsgRec& m = s_msg[s_msg_head];
  m.is_ch = is_ch; m.ch = ch; m.out = out;
  if (peer6) memcpy(m.peer6, peer6, 6); else memset(m.peer6, 0, 6);
  strncpy(m.who,  who  ? who  : "", sizeof(m.who)  - 1); m.who[sizeof(m.who)   - 1] = 0;
  strncpy(m.text, text ? text : "", sizeof(m.text) - 1); m.text[sizeof(m.text) - 1] = 0;
  s_msg_head = (s_msg_head + 1) % MSG_LOG;
  if (s_msg_n < MSG_LOG) s_msg_n++;
  s_msg_total++;
}

// does message at ring index idx belong to the active conversation?
static bool in_active_conv(int idx) {
  const MsgRec& m = s_msg[idx];
  if (s_conv_ch >= 0) return m.is_ch && m.ch == s_conv_ch;
  return !m.is_ch && memcmp(m.peer6, s_conv_peer, 6) == 0;
}
// also: does a message belong to the Public channel (for the list preview)?
static bool is_public_msg(int idx) { return s_msg[idx].is_ch && s_msg[idx].ch == 0; }

static int nth_idx(int j, bool (*match)(int)) {
  int seen = 0;
  for (int k = 0; k < s_msg_n; k++) {
    int idx = (s_msg_head - s_msg_n + k + MSG_LOG * 2) % MSG_LOG;
    if (match(idx)) { if (seen == j) return idx; seen++; }
  }
  return -1;
}
static int count_idx(bool (*match)(int)) {
  int n = 0;
  for (int k = 0; k < s_msg_n; k++) {
    int idx = (s_msg_head - s_msg_n + k + MSG_LOG * 2) % MSG_LOG;
    if (match(idx)) n++;
  }
  return n;
}

// find a saved contact by display name (for opening / sending a DM)
static bool find_contact_by_name(const char* name, ContactInfo& out) {
  int n = the_mesh.getNumContacts();
  for (int i = 0; i < n; i++) {
    ContactInfo c;
    if (the_mesh.getContactByIdx((uint32_t)i, c) && strcmp(c.name, name) == 0) { out = c; return true; }
  }
  return false;
}

extern "C" void lvd_chat_open_public(void) { s_conv_ch = 0; strncpy(s_conv_chname, "Public", sizeof(s_conv_chname)); }
extern "C" void lvd_chat_open_channel(int i) {     // i = display index from the chat list
  chan_build();
  if (i < 0 || i >= g_chan_n) { lvd_chat_open_public(); return; }
  s_conv_ch = g_chan_idx[i];
  ChannelDetails c; the_mesh.getChannel(s_conv_ch, c);
  strncpy(s_conv_chname, c.name[0] ? c.name : "(unnamed)", sizeof(s_conv_chname) - 1);
  s_conv_chname[sizeof(s_conv_chname) - 1] = 0;
}
extern "C" void lvd_chat_open_dm(const char* contact_name) {
  s_conv_ch = -1;
  s_conv_has_contact = find_contact_by_name(contact_name, s_conv_contact);
  if (s_conv_has_contact) memcpy(s_conv_peer, s_conv_contact.id.pub_key, 6);
  else memset(s_conv_peer, 0, 6);
}
extern "C" const char* lvd_chat_title(void) {
  if (s_conv_ch >= 0) return s_conv_chname;
  return s_conv_has_contact ? s_conv_contact.name : "Direct";
}
// chat list: enumerate channels (reuses the channel index map) + per-channel last preview
extern "C" int  lvd_chat_chan_count(void) { return lvd_chan_count(); }
extern "C" bool lvd_chat_chan_get(int i, lvd_chan_t* out) { return lvd_chan_get(i, out); }
extern "C" const char* lvd_chat_chan_preview(int i) {
  static char buf[124];
  chan_build();
  if (i < 0 || i >= g_chan_n) return "";
  int slot = g_chan_idx[i];
  for (int k = s_msg_n - 1; k >= 0; k--) {
    int idx = (s_msg_head - s_msg_n + k + MSG_LOG * 2) % MSG_LOG;
    if (s_msg[idx].is_ch && s_msg[idx].ch == slot) {
      strncpy(buf, s_msg[idx].text, sizeof(buf) - 1); buf[sizeof(buf) - 1] = 0; return buf;
    }
  }
  return "No messages yet";
}

extern "C" int  lvd_chat_count(void) { return count_idx(in_active_conv); }
extern "C" bool lvd_chat_get(int i, lvd_msg_t* out) {
  int idx = nth_idx(i, in_active_conv);
  if (idx < 0) return false;
  strncpy(out->sender, s_msg[idx].who,  sizeof(out->sender) - 1); out->sender[sizeof(out->sender) - 1] = 0;
  strncpy(out->text,   s_msg[idx].text, sizeof(out->text)  - 1); out->text[sizeof(out->text)   - 1] = 0;
  out->outgoing = s_msg[idx].out ? 1 : 0;
  return true;
}
extern "C" unsigned lvd_chat_total(void) { return s_msg_total; }
extern "C" const char* lvd_chat_last_preview(void) {
  int n = count_idx(is_public_msg);
  if (n <= 0) return "No messages yet";
  int idx = nth_idx(n - 1, is_public_msg);
  return idx >= 0 ? s_msg[idx].text : "";
}
extern "C" void lvd_chat_send(const char* text) {
  if (!text || !text[0]) return;
  if (s_conv_ch >= 0) {
    if (the_mesh.sendChannelText(s_conv_ch, text)) ui_store_message(true, s_conv_ch, NULL, "You", text, true);
  } else if (s_conv_has_contact) {
    uint32_t ack, timeout;
    if (the_mesh.sendTextTo(s_conv_contact, text, ack, timeout))
      ui_store_message(false, -1, s_conv_peer, "You", text, true);
  }
}

// ---- DM threads (distinct peers we have message history with) ---------------
static uint8_t g_dm_peer[16][6];
static int     g_dm_n = 0;
static bool find_contact_by_pubkey6(const uint8_t* p6, ContactInfo& out) {
  int n = the_mesh.getNumContacts();
  for (int i = 0; i < n; i++) {
    ContactInfo c;
    if (the_mesh.getContactByIdx((uint32_t)i, c) && memcmp(c.id.pub_key, p6, 6) == 0) { out = c; return true; }
  }
  return false;
}
static void dm_build(void) {       // distinct DM peers, most-recent first
  g_dm_n = 0;
  for (int k = s_msg_n - 1; k >= 0 && g_dm_n < 16; k--) {
    int idx = (s_msg_head - s_msg_n + k + MSG_LOG * 2) % MSG_LOG;
    const MsgRec& m = s_msg[idx];
    if (m.is_ch) continue;
    bool seen = false;
    for (int j = 0; j < g_dm_n; j++) if (memcmp(g_dm_peer[j], m.peer6, 6) == 0) { seen = true; break; }
    if (!seen) memcpy(g_dm_peer[g_dm_n++], m.peer6, 6);
  }
}
extern "C" int lvd_dm_count(void) { dm_build(); return g_dm_n; }
extern "C" bool lvd_dm_get(int i, lvd_dm_t* out) {
  if (i < 0 || i >= g_dm_n) return false;
  ContactInfo c;
  if (find_contact_by_pubkey6(g_dm_peer[i], c)) {
    strncpy(out->name, c.name, sizeof(out->name) - 1); out->name[sizeof(out->name) - 1] = 0;
  } else {
    snprintf(out->name, sizeof(out->name), "%02X%02X%02X%02X",
             g_dm_peer[i][0], g_dm_peer[i][1], g_dm_peer[i][2], g_dm_peer[i][3]);
  }
  out->preview[0] = 0;             // last message in this thread
  for (int k = s_msg_n - 1; k >= 0; k--) {
    int idx = (s_msg_head - s_msg_n + k + MSG_LOG * 2) % MSG_LOG;
    const MsgRec& m = s_msg[idx];
    if (!m.is_ch && memcmp(m.peer6, g_dm_peer[i], 6) == 0) {
      strncpy(out->preview, m.text, sizeof(out->preview) - 1); out->preview[sizeof(out->preview) - 1] = 0; break;
    }
  }
  return true;
}
extern "C" void lvd_dm_open(int i) {
  if (i < 0 || i >= g_dm_n) return;
  s_conv_ch = -1;
  memcpy(s_conv_peer, g_dm_peer[i], 6);
  s_conv_has_contact = find_contact_by_pubkey6(g_dm_peer[i], s_conv_contact);
}

// ---- trace route -----------------------------------------------------------
static bool rep_nth(int scan, int i, ContactInfo& c);   // defined below (repeaters)
#define TRACE_MAX 16
static int      s_tr_state = 0;          // 0 idle, 1 tracing, 2 done, 3 no-path
static char     s_tr_target[64] = "";
static uint32_t s_tr_tag = 0;
static int      s_tr_hops = 0;           // intermediate hops
static uint8_t  s_tr_hash[TRACE_MAX];
static int8_t   s_tr_snr[TRACE_MAX];
static int8_t   s_tr_final = 0;
static unsigned s_tr_seq = 0;
// trace path being built (chain of node hashes = each repeater's pub_key[0])
static uint8_t  s_tpath[TRACE_MAX];
static int      s_tpath_n = 0;
static uint32_t s_tr_sent_ms = 0;        // when the in-flight trace was sent
#define TRACE_TIMEOUT_MS 10000

void ui_store_trace(uint32_t tag, const uint8_t* hashes, const uint8_t* snrs,
                    uint8_t path_len, uint8_t path_sz, int8_t final_snr_q) {
  if (tag != s_tr_tag) return;   // only our own trace (also accepts a late result after timeout)
  int n = path_len >> path_sz;                      // SNR entries (== hops)
  if (n > TRACE_MAX) n = TRACE_MAX;
  s_tr_hops = n;
  for (int i = 0; i < n; i++) { s_tr_hash[i] = hashes[i]; s_tr_snr[i] = snrs[i]; }
  s_tr_final = final_snr_q;
  s_tr_state = 2;
  s_tr_seq++;
}

// ---- trace path builder ----
extern "C" void lvd_trace_path_clear(void) { s_tpath_n = 0; s_tr_state = 0; s_tr_seq++; }
extern "C" void lvd_trace_path_add(int i) {   // i = index in the saved repeater/room list
  if (s_tpath_n >= TRACE_MAX) return;
  ContactInfo c;
  if (rep_nth(0, i, c)) { s_tpath[s_tpath_n++] = c.id.pub_key[0]; }
}
extern "C" void lvd_trace_path_add_name(const char* name) {   // add a contact by name
  if (s_tpath_n >= TRACE_MAX) return;
  ContactInfo c;
  if (find_contact_by_name(name, c)) { s_tpath[s_tpath_n++] = c.id.pub_key[0]; }
}
extern "C" int  lvd_trace_path_len(void) { return s_tpath_n; }
extern "C" const char* lvd_trace_path_str(void) {
  static char b[80]; int k = 0; b[0] = 0;
  for (int j = 0; j < s_tpath_n; j++) {
    const char* nm = contact_name_by_hash(s_tpath[j]);
    const char* sep = (j == 0) ? "" : " > ";
    if (nm) k += snprintf(b + k, sizeof(b) - k, "%s%s", sep, nm);
    else    k += snprintf(b + k, sizeof(b) - k, "%s%02X", sep, s_tpath[j]);
    if (k >= (int)sizeof(b) - 8) { snprintf(b + k, sizeof(b) - k, " .."); break; }
  }
  return b;
}
extern "C" void lvd_trace_go(void) {
  if (s_tpath_n == 0) return;
  strncpy(s_tr_target, lvd_trace_path_str(), sizeof(s_tr_target) - 1); s_tr_target[sizeof(s_tr_target) - 1] = 0;
  s_tr_hops = 0;
  uint32_t tag;
  if (the_mesh.sendTracePath(s_tpath, (uint8_t)s_tpath_n, tag)) { s_tr_tag = tag; s_tr_state = 1; s_tr_sent_ms = millis(); }
  else s_tr_state = 3;
  s_tr_seq++;
}
// called periodically by the trace screen; flips a stuck trace to "timed out"
extern "C" void lvd_trace_poll(void) {
  if (s_tr_state == 1 && millis() - s_tr_sent_ms > TRACE_TIMEOUT_MS) { s_tr_state = 4; s_tr_seq++; }
}
extern "C" int         lvd_trace_state(void)  { return s_tr_state; }
extern "C" const char* lvd_trace_target(void) { return s_tr_target; }
extern "C" unsigned    lvd_trace_seq(void)    { return s_tr_seq; }
extern "C" int         lvd_trace_count(void)  { return s_tr_state == 2 ? s_tr_hops + 1 : 0; }
extern "C" bool        lvd_trace_get(int i, lvd_hop_t* out) {
  if (s_tr_state != 2 || i < 0 || i > s_tr_hops) return false;
  int8_t q = (i < s_tr_hops) ? s_tr_snr[i] : s_tr_final;
  if (i < s_tr_hops) {
    const char* nm = contact_name_by_hash(s_tr_hash[i]);
    if (nm) snprintf(out->left, sizeof(out->left), "%d.  %s", i + 1, nm);
    else    snprintf(out->left, sizeof(out->left), "%d.  id %02X", i + 1, s_tr_hash[i]);
  } else {
    snprintf(out->left, sizeof(out->left), "%s  to you", LV_SYMBOL_DOWN);
  }
  int v10 = q * 10 / 4, a = v10 < 0 ? -v10 : v10;
  snprintf(out->snr, sizeof(out->snr), "%s%d.%d dB", v10 < 0 ? "-" : "+", a / 10, a % 10);
  out->quality = q >= 20 ? 2 : (q >= 0 ? 1 : 0);
  return true;
}

// ---- repeater / room admin -------------------------------------------------
static bool     s_rep_active = false;
static uint8_t  s_rep_peer[6];
static char     s_rep_name[32] = "";
static int      s_rep_login = 0;            // 0 none, 1 pending, 2 in, 3 failed
static bool     s_rep_have_status = false;
static uint8_t  s_rep_blob[80];
static int      s_rep_blob_len = 0;
static unsigned s_rep_seq = 0;
#define CLI_LINES 10
static char     s_cli[CLI_LINES][84];
static int      s_cli_n = 0;

static bool is_rep_type(int t) { return t == ADV_TYPE_REPEATER || t == ADV_TYPE_ROOM; }
static const char* rep_type_tag(int t) { return t == ADV_TYPE_ROOM ? "ROOM" : "RPT"; }

// Name-sorted snapshot of the current repeater/room list (saved or heard). Built
// when the list is requested; lvd_rep_get/open then read it by display index.
#define REPLIST_MAX 64
static ContactInfo g_replist[REPLIST_MAX];
static int         g_replist_n = 0;
static int         g_replist_scan = -1;

static int cmp_ci_name(const void* a, const void* b) {
  return ci_strcmp(((const ContactInfo*)a)->name, ((const ContactInfo*)b)->name);
}
static void rep_build(int scan) {
  g_replist_n = 0;
  g_replist_scan = scan;
  if (scan == 0) {
    int n = the_mesh.getNumContacts();
    for (int k = 0; k < n && g_replist_n < REPLIST_MAX; k++) {
      ContactInfo t;
      if (the_mesh.getContactByIdx((uint32_t)k, t) && is_rep_type(t.type)) g_replist[g_replist_n++] = t;
    }
  } else {
    ContactInfo cand[16];
    int n = the_mesh.getHeardCandidates(cand, 16);
    for (int k = 0; k < n && g_replist_n < REPLIST_MAX; k++)
      if (is_rep_type(cand[k].type)) g_replist[g_replist_n++] = cand[k];
  }
  qsort(g_replist, g_replist_n, sizeof(ContactInfo), cmp_ci_name);
}
// fetch the i-th saved (scan=0) or heard (scan=1) repeater/room into c
static bool rep_nth(int scan, int i, ContactInfo& c) {
  if (g_replist_scan != scan) rep_build(scan);
  if (i < 0 || i >= g_replist_n) return false;
  c = g_replist[i];
  return true;
}

extern "C" int lvd_rep_count(int scan) { rep_build(scan); return g_replist_n; }
extern "C" bool lvd_rep_get(int scan, int i, lvd_replist_t* out) {
  ContactInfo c;
  if (!rep_nth(scan, i, c)) return false;
  strncpy(out->name, c.name, sizeof(out->name) - 1); out->name[sizeof(out->name) - 1] = 0;
  strncpy(out->type, rep_type_tag(c.type), sizeof(out->type) - 1); out->type[sizeof(out->type) - 1] = 0;
  out->fav = 0;
  return true;
}
extern "C" void lvd_rep_open(int scan, int i) {
  ContactInfo c;
  if (!rep_nth(scan, i, c)) return;
  s_rep_active = true;
  memcpy(s_rep_peer, c.id.pub_key, 6);
  strncpy(s_rep_name, c.name, sizeof(s_rep_name) - 1); s_rep_name[sizeof(s_rep_name) - 1] = 0;
  s_rep_login = 0; s_rep_have_status = false; s_cli_n = 0;
  s_rep_seq++;
}

extern "C" const char* lvd_rep_name(void)        { return s_rep_name; }
extern "C" int         lvd_rep_login_state(void) { return s_rep_login; }
extern "C" void        lvd_rep_login(const char* password) {
  if (!s_rep_active) return;
  uint32_t timeout;
  if (the_mesh.uiLogin(s_rep_peer, password ? password : "", timeout)) s_rep_login = 1;  // pending
  else s_rep_login = 3;
  s_rep_seq++;
}
extern "C" void lvd_rep_request_status(void) {
  if (!s_rep_active) return;
  uint32_t timeout;
  the_mesh.uiRequestStatus(s_rep_peer, timeout);
  s_rep_seq++;
}

static uint32_t rd_u32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t rd_u16(const uint8_t* p) { return p[0] | (p[1] << 8); }

extern "C" void lvd_rep_status_get(lvd_repstat_t* out) {
  out->have = 0;
  if (!s_rep_have_status || s_rep_blob_len < 24) { return; }
  const uint8_t* b = s_rep_blob;
  out->have = 1;
  uint16_t mv = rd_u16(b + 0);
  snprintf(out->batt, sizeof(out->batt), "%u.%02uV", mv / 1000, (mv % 1000) / 10);
  uint32_t up = (s_rep_blob_len >= 24) ? rd_u32(b + 20) : 0;
  if (up >= 86400) snprintf(out->uptime, sizeof(out->uptime), "%ud %uh", up / 86400, (up % 86400) / 3600);
  else             snprintf(out->uptime, sizeof(out->uptime), "%uh %um", up / 3600, (up % 3600) / 60);
  snprintf(out->recv, sizeof(out->recv), "%u", (unsigned)rd_u32(b + 8));
  snprintf(out->sent, sizeof(out->sent), "%u", (unsigned)rd_u32(b + 12));
  uint32_t air = (s_rep_blob_len >= 20) ? rd_u32(b + 16) : 0;
  snprintf(out->airtime, sizeof(out->airtime), "%um", air / 60);
  if (s_rep_blob_len >= 44) {
    int16_t snr = (int16_t)rd_u16(b + 42); int v10 = snr * 10 / 4, a = v10 < 0 ? -v10 : v10;
    snprintf(out->snr, sizeof(out->snr), "%s%d.%d dB", v10 < 0 ? "-" : "", a / 10, a % 10);
  } else snprintf(out->snr, sizeof(out->snr), "--");
}

extern "C" void lvd_rep_send_cmd(const char* cmd) {
  if (!s_rep_active || !cmd || !cmd[0]) return;
  uint32_t timeout;
  if (the_mesh.uiSendCommand(s_rep_peer, cmd, timeout)) {
    // echo the command locally
    if (s_cli_n < CLI_LINES) { snprintf(s_cli[s_cli_n], sizeof(s_cli[0]), "> %s", cmd); s_cli_n++; }
    else { for (int i = 1; i < CLI_LINES; i++) memcpy(s_cli[i - 1], s_cli[i], sizeof(s_cli[0])); snprintf(s_cli[CLI_LINES - 1], sizeof(s_cli[0]), "> %s", cmd); }
    s_rep_seq++;
  }
}
extern "C" int         lvd_rep_cli_count(void) { return s_cli_n; }
extern "C" const char* lvd_rep_cli_line(int i)  { return (i >= 0 && i < s_cli_n) ? s_cli[i] : ""; }
extern "C" unsigned    lvd_rep_seq(void)        { return s_rep_seq; }

void ui_rep_on_login(const uint8_t* pk6, bool ok, uint8_t perms) {
  if (s_rep_active && memcmp(pk6, s_rep_peer, 6) == 0) { s_rep_login = ok ? 2 : 3; s_rep_seq++; }
}
void ui_rep_on_status(const uint8_t* pk6, const uint8_t* data, uint8_t len) {
  if (!s_rep_active || memcmp(pk6, s_rep_peer, 6) != 0) return;
  s_rep_blob_len = len > (int)sizeof(s_rep_blob) ? (int)sizeof(s_rep_blob) : len;
  memcpy(s_rep_blob, data, s_rep_blob_len);
  s_rep_have_status = true;
  s_rep_seq++;
}
void ui_rep_on_cmdreply(const uint8_t* pk6, const char* text) {
  if (!s_rep_active || memcmp(pk6, s_rep_peer, 6) != 0) return;
  char line[84]; snprintf(line, sizeof(line), "%s", text ? text : "");
  if (s_cli_n < CLI_LINES) { memcpy(s_cli[s_cli_n], line, sizeof(line)); s_cli_n++; }
  else { for (int i = 1; i < CLI_LINES; i++) memcpy(s_cli[i - 1], s_cli[i], sizeof(s_cli[0])); memcpy(s_cli[CLI_LINES - 1], line, sizeof(line)); }
  s_rep_seq++;
}

// ---- peer / contact details ------------------------------------------------
static bool find_heard_by_pubkey(const uint8_t* pub, AdvertPath& out) {
  AdvertPath rec[16];
  int n = the_mesh.getRecentlyHeard(rec, 16);
  for (int i = 0; i < n; i++)
    if (memcmp(rec[i].pubkey_prefix, pub, sizeof(rec[i].pubkey_prefix)) == 0) { out = rec[i]; return true; }
  return false;
}

extern "C" bool lvd_peer_get(const char* name, lvd_peer_t* out) {
  ContactInfo c;
  if (!find_contact_by_name(name, c)) return false;

  const char* t = c.type == ADV_TYPE_REPEATER ? "Repeater" :
                  c.type == ADV_TYPE_ROOM     ? "Room server" :
                  c.type == ADV_TYPE_SENSOR   ? "Sensor" : "Chat contact";
  strncpy(out->type, t, sizeof(out->type) - 1); out->type[sizeof(out->type) - 1] = 0;

  if (c.out_path_len == OUT_PATH_UNKNOWN) { snprintf(out->path, sizeof(out->path), "flood");  snprintf(out->hops, sizeof(out->hops), "--"); }
  else if (c.out_path_len == 0)           { snprintf(out->path, sizeof(out->path), "direct"); snprintf(out->hops, sizeof(out->hops), "0"); }
  else                                    { snprintf(out->path, sizeof(out->path), "routed"); snprintf(out->hops, sizeof(out->hops), "%u", (unsigned)c.out_path_len); }

  for (int i = 0; i < 32; i++) snprintf(out->pubkey + i * 2, 3, "%02x", c.id.pub_key[i]);

  int32_t lat = c.gps_lat, lon = c.gps_lon;
  AdvertPath h; bool hh = find_heard_by_pubkey(c.id.pub_key, h);
  if ((lat == 0 && lon == 0) && hh) { lat = h.gps_lat; lon = h.gps_lon; }
  if (lat == 0 && lon == 0) { snprintf(out->lat, sizeof(out->lat), "--"); snprintf(out->lon, sizeof(out->lon), "--"); }
  else { snprintf(out->lat, sizeof(out->lat), "%.4f", lat / 1e6); snprintf(out->lon, sizeof(out->lon), "%.4f", lon / 1e6); }

  if (hh && h.rssi)  snprintf(out->rssi, sizeof(out->rssi), "%d dBm", h.rssi); else snprintf(out->rssi, sizeof(out->rssi), "--");
  if (hh && h.snr_q) { int v10 = h.snr_q * 10 / 4, a = v10 < 0 ? -v10 : v10; snprintf(out->snr, sizeof(out->snr), "%s%d.%d dB", v10 < 0 ? "-" : "", a / 10, a % 10); }
  else snprintf(out->snr, sizeof(out->snr), "--");
  // Distance = great-circle (haversine) between our position and the peer's.
  // Needs both a peer fix (lat/lon above) and our own position (sensors.node_*,
  // from GPS or the manual lat/lon in Settings); otherwise "--".
  bool peer_loc = !(lat == 0 && lon == 0);
  bool self_loc = !(sensors.node_lat == 0.0 && sensors.node_lon == 0.0);
  if (peer_loc && self_loc) {
    const double R = 6371000.0, D2R = M_PI / 180.0;
    double la1 = sensors.node_lat * D2R, la2 = (lat / 1e6) * D2R;
    double dla = la2 - la1;
    double dlo = ((lon / 1e6) - sensors.node_lon) * D2R;
    double a = sin(dla / 2) * sin(dla / 2) + cos(la1) * cos(la2) * sin(dlo / 2) * sin(dlo / 2);
    double m = R * 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    if (m < 1000.0)       snprintf(out->dist, sizeof(out->dist), "%d m", (int)(m + 0.5));
    else if (m < 10000.0) snprintf(out->dist, sizeof(out->dist), "%.1f km", m / 1000.0);
    else                  snprintf(out->dist, sizeof(out->dist), "%d km", (int)(m / 1000.0 + 0.5));
  } else snprintf(out->dist, sizeof(out->dist), "--");

  // Prefer the live heard-ring timestamp (this session), else fall back to the
  // contact's persisted lastmod -- updated on every advert, even multi-hop, and
  // survives reboots -- so a saved repeater still shows when we last heard it.
  uint32_t hts = (hh && h.recv_timestamp) ? h.recv_timestamp : c.lastmod;
  if (hts) {
    uint32_t now = rtc_clock.getCurrentTime();
    uint32_t age = now > hts ? now - hts : 0;
    if (age < 60) snprintf(out->lastheard, sizeof(out->lastheard), "%us", (unsigned)age);
    else if (age < 3600) snprintf(out->lastheard, sizeof(out->lastheard), "%um", (unsigned)(age / 60));
    else if (age < 86400) snprintf(out->lastheard, sizeof(out->lastheard), "%uh", (unsigned)(age / 3600));
    else snprintf(out->lastheard, sizeof(out->lastheard), "%ud", (unsigned)(age / 86400));
  } else snprintf(out->lastheard, sizeof(out->lastheard), "--");

  out->fav = (c.flags & 0x01) ? 1 : 0;   // LSB of flags is the favourite bit
  return true;
}

// ---- contact ops (peer details) --------------------------------------------
extern "C" bool lvd_peer_share(const char* name) {
  ContactInfo c; return find_contact_by_name(name, c) && the_mesh.uiShareContact(c.id.pub_key);
}
extern "C" bool lvd_peer_reset_path(const char* name) {
  ContactInfo c; return find_contact_by_name(name, c) && the_mesh.uiResetPath(c.id.pub_key);
}
extern "C" bool lvd_peer_set_fav(const char* name, int on) {
  ContactInfo c; return find_contact_by_name(name, c) && the_mesh.setContactFavourite(c.id.pub_key, on != 0);
}
extern "C" bool lvd_peer_remove(const char* name) {
  ContactInfo c; return find_contact_by_name(name, c) && the_mesh.uiRemoveContact(c.id.pub_key);
}
extern "C" const char* lvd_peer_export_hex(const char* name) {
  static char hex[540];
  hex[0] = 0;
  ContactInfo c;
  if (!find_contact_by_name(name, c)) return hex;
  uint8_t blob[256];
  int n = the_mesh.uiExportContact(c.id.pub_key, blob, sizeof(blob));
  int k = 0;
  for (int i = 0; i < n && k < (int)sizeof(hex) - 3; i++) k += snprintf(hex + k, sizeof(hex) - k, "%02X", blob[i]);
  return hex;
}

// ---- signal coverage (saved repeaters/rooms we've actually heard) ----------
// Only include repeaters with a recent heard advert (skip stale ones). Keep a
// filtered index into the sorted g_replist, rebuilt on each count.
struct SigEnt { uint16_t idx; int rssi; };
static SigEnt g_sig[REPLIST_MAX];
static int    g_sig_n = 0;

static int cmp_sig_rssi(const void* a, const void* b) {
  return ((const SigEnt*)b)->rssi - ((const SigEnt*)a)->rssi;   // descending (strongest first)
}
static void sig_build(void) {
  rep_build(0);
  g_sig_n = 0;
  for (int i = 0; i < g_replist_n; i++) {
    AdvertPath h;
    if (find_heard_by_pubkey(g_replist[i].id.pub_key, h) && h.rssi) {
      g_sig[g_sig_n].idx = (uint16_t)i; g_sig[g_sig_n].rssi = h.rssi; g_sig_n++;
    }
  }
  qsort(g_sig, g_sig_n, sizeof(g_sig[0]), cmp_sig_rssi);
}
extern "C" int lvd_signal_count(void) { sig_build(); return g_sig_n; }
extern "C" bool lvd_signal_get(int i, lvd_sig_t* out) {
  if (i < 0 || i >= g_sig_n) return false;
  ContactInfo& c = g_replist[g_sig[i].idx];
  strncpy(out->name, c.name, sizeof(out->name) - 1); out->name[sizeof(out->name) - 1] = 0;

  AdvertPath h;
  find_heard_by_pubkey(c.id.pub_key, h);   // present (filtered in sig_build)
  out->rssi = h.rssi; out->heard = 1;
  int v10 = h.snr_q * 10 / 4, a = v10 < 0 ? -v10 : v10;
  snprintf(out->info, sizeof(out->info), "%d dBm   SNR %s%d.%d dB", h.rssi, v10 < 0 ? "-" : "", a / 10, a % 10);

  uint32_t now = rtc_clock.getCurrentTime();
  uint32_t age = now > h.recv_timestamp ? now - h.recv_timestamp : 0;
  if (age < 60)        snprintf(out->age, sizeof(out->age), "%us", (unsigned)age);
  else if (age < 3600) snprintf(out->age, sizeof(out->age), "%um", (unsigned)(age / 60));
  else                 snprintf(out->age, sizeof(out->age), "%uh", (unsigned)(age / 3600));
  return true;
}
