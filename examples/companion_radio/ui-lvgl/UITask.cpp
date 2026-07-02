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
  void lv_files_create(lv_obj_t* scr);
  void lv_contacts_create(lv_obj_t* scr);
  void lv_contact_search_create(lv_obj_t* scr);
}

// incoming messages since the chat list / a conversation was last viewed
// (drives the home Chat tile badge)
static unsigned s_unread = 0;

// ===========================================================================
// screen dispatch
// ===========================================================================
static void build_screen(const char* name) {
  lv_obj_t* s = lv_screen_active();
  lv_ui_set_refresh(NULL);   // drop the previous screen's live-refresh hook
  lv_obj_clean(s);
  if (!strcmp(name, "chat") || !strcmp(name, "conv")) s_unread = 0;   // user is looking at chats
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
  // "repeaters"/"scan" list section removed: repeaters are managed from their
  // contact peer card (Manage -> repeater_detail). Detail/login/CLI screens kept.
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
  else if (!strcmp(name, "files")) lv_files_create(s);
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
extern "C" int lvd_unread_count(void) { return (int)s_unread; }

// getRecentlyHeard() returns the WHOLE table including empty slots, so filter to
// real entries (a used slot has a name or a recv timestamp) and cache them; the
// screen calls lvd_heard_count() first, then lvd_heard_get(i) per row.
static AdvertPath g_heard[16];
static int        g_heard_n = 0;

// Great-circle (haversine) distance from our position (sensors.node_*, from GPS
// or the manual lat/lon in Settings) to a peer fix (1e-6 deg), formatted as
// "850 m" / "4.2 km". False if either side has no position.
static bool fmt_distance(int32_t lat_e6, int32_t lon_e6, char* out, int len) {
  if (lat_e6 == 0 && lon_e6 == 0) return false;
  if (sensors.node_lat == 0.0 && sensors.node_lon == 0.0) return false;
  const double R = 6371000.0, D2R = M_PI / 180.0;
  double la1 = sensors.node_lat * D2R, la2 = (lat_e6 / 1e6) * D2R;
  double dla = la2 - la1;
  double dlo = ((lon_e6 / 1e6) - sensors.node_lon) * D2R;
  double a = sin(dla / 2) * sin(dla / 2) + cos(la1) * cos(la2) * sin(dlo / 2) * sin(dlo / 2);
  double m = R * 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
  if (m < 1000.0)       snprintf(out, len, "%d m", (int)(m + 0.5));
  else if (m < 10000.0) snprintf(out, len, "%.1f km", m / 1000.0);
  else                  snprintf(out, len, "%d km", (int)(m / 1000.0 + 0.5));
  return true;
}

// look up a heard node (by its 6-byte pubkey prefix) among saved contacts;
// returns 1 if saved and fills *type with its ADV_TYPE_*, else 0.
static int heard_contact_lookup(const uint8_t* pk6, int* type) {
  // real contacts live in slots [MAX_ANON_CONTACTS, total); getContactByIdx is not offset
  for (int i = MAX_ANON_CONTACTS, n = the_mesh.getTotalContactSlots(); i < n; i++) {
    ContactInfo c;
    if (the_mesh.getContactByIdx((uint32_t)i, c) && memcmp(c.id.pub_key, pk6, 6) == 0) {
      if (type) *type = c.type;
      return 1;
    }
  }
  return 0;
}
// 8-point compass bearing from our position to a peer fix (1e-6 deg); "" if unknown
static const char* heard_bearing(int32_t lat_e6, int32_t lon_e6) {
  if ((lat_e6 == 0 && lon_e6 == 0) || (sensors.node_lat == 0.0 && sensors.node_lon == 0.0)) return "";
  const double D2R = M_PI / 180.0;
  double la1 = sensors.node_lat * D2R, la2 = (lat_e6 / 1e6) * D2R;
  double dlo = ((lon_e6 / 1e6) - sensors.node_lon) * D2R;
  double y = sin(dlo) * cos(la2);
  double x = cos(la1) * sin(la2) - sin(la1) * cos(la2) * cos(dlo);
  double brg = atan2(y, x) / D2R;
  if (brg < 0) brg += 360.0;
  static const char* PTS[] = {"N","NE","E","SE","S","SW","W","NW"};
  return PTS[((int)((brg + 22.5) / 45.0)) & 7];
}

static int g_heard_sort = 0;   // 0 = recent, 1 = strongest signal
extern "C" void lvd_heard_set_sort(int m) { g_heard_sort = m ? 1 : 0; }
extern "C" int  lvd_heard_sort(void) { return g_heard_sort; }

static int cmp_heard_signal(const void* a, const void* b) {
  return ((const AdvertPath*)b)->snr_q - ((const AdvertPath*)a)->snr_q;   // strongest first
}

extern "C" int lvd_heard_count(void) {
  AdvertPath rec[16];
  int n = the_mesh.getRecentlyHeard(rec, 16);   // already recency-sorted
  g_heard_n = 0;
  for (int i = 0; i < n && g_heard_n < 16; i++) {
    if (rec[i].name[0] != 0 || rec[i].recv_timestamp != 0) g_heard[g_heard_n++] = rec[i];
  }
  if (g_heard_sort == 1) qsort(g_heard, g_heard_n, sizeof(g_heard[0]), cmp_heard_signal);
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

  // saved-contact status + node type (by pubkey prefix)
  out->type = 0;
  out->saved = heard_contact_lookup(a.pubkey_prefix, &out->type);

  // route: type name + how we heard it (direct vs relayed)
  const char* tn = out->type == ADV_TYPE_CHAT     ? "Companion" :
                   out->type == ADV_TYPE_REPEATER ? "Repeater"  :
                   out->type == ADV_TYPE_ROOM     ? "Room"      :
                   out->type == ADV_TYPE_SENSOR   ? "Sensor"    : "Node";
  int hops = a.path_len & 63;   // path_len is encoded: low 6 bits = hop count, top 2 = hash size
  if (hops == 0)      snprintf(out->route, sizeof(out->route), "%s \xC2\xB7 direct", tn);
  else if (hops == 1) snprintf(out->route, sizeof(out->route), "%s \xC2\xB7 1 hop", tn);
  else                snprintf(out->route, sizeof(out->route), "%s \xC2\xB7 %d hops", tn, hops);

  uint32_t now = rtc_clock.getCurrentTime();
  uint32_t age = (now > a.recv_timestamp) ? now - a.recv_timestamp : 0;
  if (age < 60)        snprintf(out->age, sizeof(out->age), "%us ago", (unsigned)age);
  else if (age < 3600) snprintf(out->age, sizeof(out->age), "%um ago", (unsigned)(age / 60));
  else                 snprintf(out->age, sizeof(out->age), "%uh ago", (unsigned)(age / 3600));

  char db[12];
  if (fmt_distance(a.gps_lat, a.gps_lon, db, sizeof(db))) {
    const char* brg = heard_bearing(a.gps_lat, a.gps_lon);
    snprintf(out->dist, sizeof(out->dist), "%s%s%s", db, brg[0] ? " " : "", brg);
  } else out->dist[0] = 0;
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
static int      g_type_filter = 0;               // 0 all, 1 chats, 2 repeaters, 3 rooms
static uint16_t g_corder[MAX_CONTACTS];
static int      g_corder_n = -1;
static int      g_corder_total = -1;             // contact count the order was built for
static char     g_corder_filter[24] = "\x01";    // filter it was built for (impossible init)
static int      g_corder_fav = -1;               // fav-only mode the order was built for
static int      g_corder_type = -1;              // type filter the order was built for

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
  if (total == g_corder_total && g_corder_fav == (int)g_fav_only && g_corder_type == g_type_filter &&
      strcmp(g_cfilter, g_corder_filter) == 0) return;   // cached
  int m = 0;
  // real contacts occupy absolute slots [MAX_ANON_CONTACTS, total); getContactByIdx
  // is not offset, so iterate absolute slots and skip the reserved anon/empty ones.
  for (int i = MAX_ANON_CONTACTS, tot = the_mesh.getTotalContactSlots(); i < tot && m < MAX_CONTACTS; i++) {
    ContactInfo c;
    if (!the_mesh.getContactByIdx((uint32_t)i, c)) continue;
    if (c.type == ADV_TYPE_NONE || c.name[0] == 0) continue;   // skip empty/anon slots
    if (g_fav_only && !(c.flags & 0x01)) continue;       // favourites-only filter
    if (g_type_filter == 1 && c.type != ADV_TYPE_CHAT)     continue;   // Chats
    if (g_type_filter == 2 && c.type != ADV_TYPE_REPEATER) continue;   // Repeaters
    if (g_type_filter == 3 && c.type != ADV_TYPE_ROOM)     continue;   // Rooms
    if (lvd_name_match(c.name, g_cfilter)) g_corder[m++] = (uint16_t)i;
  }
  qsort(g_corder, m, sizeof(g_corder[0]), cmp_contact_idx);
  g_corder_n = m;
  g_corder_total = total;
  g_corder_fav = (int)g_fav_only;
  g_corder_type = g_type_filter;
  strncpy(g_corder_filter, g_cfilter, sizeof(g_corder_filter) - 1); g_corder_filter[sizeof(g_corder_filter) - 1] = 0;
}
extern "C" void        lvd_contact_set_filter(const char* s) {
  strncpy(g_cfilter, s ? s : "", sizeof(g_cfilter) - 1); g_cfilter[sizeof(g_cfilter) - 1] = 0;
}
extern "C" const char* lvd_contact_filter(void) { return g_cfilter; }
extern "C" void        lvd_contact_set_fav_only(int on) { g_fav_only = (on != 0); }
extern "C" int         lvd_contact_fav_only(void) { return g_fav_only ? 1 : 0; }
extern "C" void        lvd_contact_set_type(int t) { g_type_filter = (t < 0 || t > 3) ? 0 : t; }  // 0 all,1 chats,2 repeaters,3 rooms
extern "C" int         lvd_contact_type(void) { return g_type_filter; }
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
    snprintf(out->subtitle, sizeof(out->subtitle), "%s / %d hops", t, c.out_path_len & 63);   // low 6 bits = hop count
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

// Region frequency plans; order matches the settings "Preset" enum, with
// "Custom" (= no match) as the last option. Same table as ui-new SettingsModel.
struct RadioPlan { float freq, bw; uint8_t sf, cr; };
static const RadioPlan RADIO_PLANS[] = {
  {910.525f, 62.5f,  7, 5},   // USA/Canada (902-928 ISM, narrow-band)
  {869.525f, 250.0f, 10, 5},  // EU standard default
  {869.618f, 62.5f,  8, 8},   // UK/CH regional
  {915.800f, 250.0f, 10, 5},  // AU/NZ
};
#define RADIO_PLAN_N       ((int)(sizeof(RADIO_PLANS) / sizeof(RADIO_PLANS[0])))
#define RADIO_PLAN_CUSTOM  RADIO_PLAN_N
static int radio_plan_match(const NodePrefs* p) {
  for (int i = 0; i < RADIO_PLAN_N; i++) {
    const RadioPlan& r = RADIO_PLANS[i];
    if (fabsf(p->freq - r.freq) < 0.01f && fabsf(p->bw - r.bw) < 0.01f && p->sf == r.sf && p->cr == r.cr)
      return i;
  }
  return RADIO_PLAN_CUSTOM;
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

// ---- microSD browser bridge (wraps the target's shared-bus SD helpers) ------
extern "C" int lvd_sd_available(void) { return tdeck_sd_ok() ? 1 : 0; }
extern "C" int lvd_sd_list(const char* path, lvd_sd_t* out, int max) {
  static SdEntry ent[64];
  if (max > 64) max = 64;
  int n = tdeck_sd_list(path, ent, max);
  for (int i = 0; i < n; i++) {
    strncpy(out[i].name, ent[i].name, sizeof(out[i].name) - 1); out[i].name[sizeof(out[i].name) - 1] = 0;
    out[i].is_dir = ent[i].is_dir ? 1 : 0;
    out[i].size = ent[i].size;
  }
  return n;
}
extern "C" void lvd_sd_usage(char* out, int len) {
  uint64_t used = 0, total = tdeck_sd_bytes(&used);
  if (total == 0) { strncpy(out, "no card", len - 1); out[len - 1] = 0; return; }
  double gt = total / 1073741824.0, gu = used / 1073741824.0;
  if (gt >= 1.0) snprintf(out, len, "%.1f / %.1f GB used", gu, gt);
  else           snprintf(out, len, "%.0f / %.0f MB used", used / 1048576.0, total / 1048576.0);
}
extern "C" int lvd_sd_remove(const char* path) { return tdeck_sd_remove(path) ? 1 : 0; }
extern "C" int lvd_sd_format(void) { return tdeck_sd_format() ? 1 : 0; }   // reformat FAT32 (destructive)

// ---- SD backup / restore (config + contacts/channels) ----------------------
#define SD_CONFIG_PATH  "/meshcore/config.txt"
#define SD_APPDATA_PATH "/meshcore/appdata.txt"

static bool data_backup_config(void) {
  if (!tdeck_sd_ok()) return false;
  tdeck_sd_mkdir("/meshcore");
  NodePrefs* p = the_mesh.getNodePrefs();
  char b[1400]; int k = 0;
  k += snprintf(b + k, sizeof(b) - k, "# meshcore config backup v1\nname=%s\n", p->node_name);
  k += snprintf(b + k, sizeof(b) - k, "freq=%.3f\nbw=%.2f\nsf=%u\ncr=%u\ntx_power=%d\n", p->freq, p->bw, p->sf, p->cr, p->tx_power_dbm);
  k += snprintf(b + k, sizeof(b) - k, "client_repeat=%u\nrx_boosted_gain=%u\ncad=%u\n", p->client_repeat, p->rx_boosted_gain, p->cad_enabled);
  k += snprintf(b + k, sizeof(b) - k, "airtime_factor=%.2f\nrx_delay_base=%.0f\n", p->airtime_factor, p->rx_delay_base);
  k += snprintf(b + k, sizeof(b) - k, "multi_acks=%u\npath_hash_mode=%u\n", p->multi_acks, p->path_hash_mode);
  k += snprintf(b + k, sizeof(b) - k, "manual_add=%u\nautoadd_config=%u\nautoadd_max_hops=%u\n", p->manual_add_contacts, p->autoadd_config, p->autoadd_max_hops);
  k += snprintf(b + k, sizeof(b) - k, "telemetry_base=%u\ntelemetry_loc=%u\ntelemetry_env=%u\n", p->telemetry_mode_base, p->telemetry_mode_loc, p->telemetry_mode_env);
  k += snprintf(b + k, sizeof(b) - k, "loc_policy=%u\nlat=%.6f\nlon=%.6f\n", p->advert_loc_policy, sensors.node_lat, sensors.node_lon);
  k += snprintf(b + k, sizeof(b) - k, "gps=%u\ngps_interval=%u\ntime_sync_gps=%u\n", p->gps_enabled, (unsigned)p->gps_interval, p->time_sync_gps);
  k += snprintf(b + k, sizeof(b) - k, "buzzer_quiet=%u\ndisc_autoadd=%u\nble_pin=%u\n", p->buzzer_quiet, p->disc_autoadd, (unsigned)p->ble_pin);
  return tdeck_sd_write(SD_CONFIG_PATH, (const uint8_t*)b, k, false);
}
static bool data_restore_config(void) {
  if (!tdeck_sd_ok()) return false;
  char* buf = (char*)malloc(8192);
  if (!buf) return false;
  int n = tdeck_sd_read(SD_CONFIG_PATH, (uint8_t*)buf, 8191);
  if (n <= 0) { free(buf); return false; }
  buf[n] = 0;
  NodePrefs* p = the_mesh.getNodePrefs();
  float freq = p->freq, bw = p->bw; int sf = p->sf, cr = p->cr, repeat = p->client_repeat;
  int tb = p->telemetry_mode_base, tl = p->telemetry_mode_loc, te = p->telemetry_mode_env;
  double lat = sensors.node_lat, lon = sensors.node_lon; bool have_loc = false;
  for (char* line = strtok(buf, "\r\n"); line; line = strtok(NULL, "\r\n")) {
    char* eq = strchr(line, '='); if (!eq || line[0] == '#') continue;
    *eq = 0; const char* key = line; const char* v = eq + 1;
    if      (!strcmp(key, "name"))            the_mesh.setAdvertName(v);
    else if (!strcmp(key, "freq"))            freq = atof(v);
    else if (!strcmp(key, "bw"))              bw = atof(v);
    else if (!strcmp(key, "sf"))              sf = atoi(v);
    else if (!strcmp(key, "cr"))              cr = atoi(v);
    else if (!strcmp(key, "client_repeat"))   repeat = atoi(v);
    else if (!strcmp(key, "tx_power"))        the_mesh.setTxPower(atoi(v));
    else if (!strcmp(key, "rx_boosted_gain")) the_mesh.setRxBoostedGain(atoi(v));
    else if (!strcmp(key, "cad"))             the_mesh.setCADEnabled(atoi(v));
    else if (!strcmp(key, "airtime_factor"))  the_mesh.setTuningParams(p->rx_delay_base, atof(v));
    else if (!strcmp(key, "rx_delay_base"))   the_mesh.setTuningParams(atof(v), p->airtime_factor);
    else if (!strcmp(key, "multi_acks"))      the_mesh.setMultiAcks(atoi(v));
    else if (!strcmp(key, "path_hash_mode"))  the_mesh.setPathHashMode(atoi(v));
    else if (!strcmp(key, "manual_add"))      the_mesh.setManualAdd(atoi(v));
    else if (!strcmp(key, "autoadd_config"))  the_mesh.setAutoAddConfig(atoi(v));
    else if (!strcmp(key, "autoadd_max_hops")) the_mesh.setMaxHops(atoi(v));
    else if (!strcmp(key, "telemetry_base"))  tb = atoi(v);
    else if (!strcmp(key, "telemetry_loc"))   tl = atoi(v);
    else if (!strcmp(key, "telemetry_env"))   te = atoi(v);
    else if (!strcmp(key, "loc_policy"))      the_mesh.setAdvertLocPolicy(atoi(v));
    else if (!strcmp(key, "lat"))             { lat = atof(v); have_loc = true; }
    else if (!strcmp(key, "lon"))             { lon = atof(v); have_loc = true; }
    else if (!strcmp(key, "buzzer_quiet"))    the_mesh.setBuzzerQuiet(atoi(v));
    else if (!strcmp(key, "disc_autoadd"))    the_mesh.setDiscAutoAdd(atoi(v));
    else if (!strcmp(key, "ble_pin"))         the_mesh.setBlePin((uint32_t)strtoul(v, NULL, 10));
#if ENV_INCLUDE_GPS == 1
    else if (!strcmp(key, "gps"))             the_mesh.setGpsEnabled(atoi(v));
    else if (!strcmp(key, "gps_interval"))    the_mesh.setGpsInterval((uint32_t)atoi(v));
    else if (!strcmp(key, "time_sync_gps"))   the_mesh.setTimeSyncFromGps(atoi(v));
#endif
  }
  apply_radio(freq, bw, sf, cr, repeat);
  the_mesh.setTelemetryModes(tb, tl, te);
  if (have_loc) the_mesh.setAdvertLatLon((int32_t)(lat * 1e6), (int32_t)(lon * 1e6));
  free(buf);
  return true;
}
static int hex_to_bytes(const char* s, uint8_t* out, int max) {
  int n = 0;
  while (s[0] && s[1] && n < max) {
    auto hv = [](char c) -> int { return (c >= '0' && c <= '9') ? c - '0' :
                                         (c >= 'a' && c <= 'f') ? c - 'a' + 10 :
                                         (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1; };
    int hi = hv(s[0]), lo = hv(s[1]); if (hi < 0 || lo < 0) break;
    out[n++] = (uint8_t)((hi << 4) | lo); s += 2;
  }
  return n;
}
static bool data_backup_appdata(void) {
  if (!tdeck_sd_ok()) return false;
  tdeck_sd_mkdir("/meshcore");
  const char* hdr = "# meshcore appdata backup v1\n[contacts]\n";
  if (!tdeck_sd_write(SD_APPDATA_PATH, (const uint8_t*)hdr, strlen(hdr), false)) return false;
  for (int i = MAX_ANON_CONTACTS, tot = the_mesh.getTotalContactSlots(); i < tot; i++) {
    ContactInfo c;
    if (!the_mesh.getContactByIdx((uint32_t)i, c) || c.type == ADV_TYPE_NONE || !c.name[0]) continue;
    uint8_t blob[256]; int bn = the_mesh.uiExportContact(c.id.pub_key, blob, sizeof(blob));
    if (bn <= 0) continue;
    char line[560]; int k = 0;
    for (int j = 0; j < bn && k < (int)sizeof(line) - 3; j++) k += snprintf(line + k, sizeof(line) - k, "%02X", blob[j]);
    k += snprintf(line + k, sizeof(line) - k, "\n");
    tdeck_sd_write(SD_APPDATA_PATH, (const uint8_t*)line, k, true);
  }
  tdeck_sd_write(SD_APPDATA_PATH, (const uint8_t*)"[channels]\n", 11, true);
  for (int i = 0, cn = lvd_chan_count(); i < cn; i++) {
    lvd_chan_t ch;
    if (!lvd_chan_get(i, &ch)) continue;
    char line[128]; int k = snprintf(line, sizeof(line), "%s\t%s\n", ch.name, lvd_chan_psk(i));
    tdeck_sd_write(SD_APPDATA_PATH, (const uint8_t*)line, k, true);
  }
  return true;
}
static bool data_restore_appdata(void) {
  if (!tdeck_sd_ok()) return false;
  char* buf = (char*)malloc(32768);
  if (!buf) return false;
  int n = tdeck_sd_read(SD_APPDATA_PATH, (uint8_t*)buf, 32767);
  if (n <= 0) { free(buf); return false; }
  buf[n] = 0;
  int section = 0;   // 1 contacts, 2 channels
  for (char* line = strtok(buf, "\r\n"); line; line = strtok(NULL, "\r\n")) {
    if      (!strcmp(line, "[contacts]")) section = 1;
    else if (!strcmp(line, "[channels]")) section = 2;
    else if (line[0] == '#' || !line[0])  continue;
    else if (section == 1) { uint8_t blob[256]; int bn = hex_to_bytes(line, blob, sizeof(blob)); if (bn > 0) the_mesh.importContact(blob, (uint8_t)bn); }
    else if (section == 2) { char* tab = strchr(line, '\t'); if (tab) { *tab = 0; the_mesh.uiAddChannel(line, tab + 1); } }
  }
  free(buf);
  return true;
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
    if (eq(label, "Preset"))           { *sel = radio_plan_match(p); return true; }
    if (eq(label, "TX power"))         { snprintf(val, len, "%d", p->tx_power_dbm); return true; }
    if (eq(label, "Frequency"))        { snprintf(val, len, "%.3f", p->freq); return true; }
    if (eq(label, "Spreading factor")) { snprintf(val, len, "%u", p->sf); return true; }
    if (eq(label, "Coding rate"))      { snprintf(val, len, "%u", p->cr); return true; }
    if (eq(label, "Client repeat"))    { *sel = p->client_repeat ? 1 : 0; return true; }
    if (eq(label, "Channel activity detection")) { *sel = p->cad_enabled ? 1 : 0; return true; }
    if (eq(label, "RX boosted gain")) { *sel = p->rx_boosted_gain ? 1 : 0; return true; }
    if (eq(label, "Bandwidth")) {
      int best = 6; float bd = 1e9f;
      for (int i = 0; i < BW_N; i++) { float d = p->bw - BW_KHZ[i]; if (d < 0) d = -d; if (d < bd) { bd = d; best = i; } }
      *sel = best; return true;
    }
  } else if (eq(group, "Contacts")) {
    if (eq(label, "Manual add"))        { *sel = p->manual_add_contacts ? 1 : 0; return true; }
    if (eq(label, "Auto-add discovered")) { *sel = p->disc_autoadd ? 1 : 0; return true; }
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
  } else if (eq(group, "Telemetry")) {
    if (eq(label, "Base"))        { *sel = p->telemetry_mode_base <= 2 ? p->telemetry_mode_base : 2; return true; }
    if (eq(label, "Location"))    { *sel = p->telemetry_mode_loc  <= 2 ? p->telemetry_mode_loc  : 2; return true; }
    if (eq(label, "Environment")) { *sel = p->telemetry_mode_env  <= 2 ? p->telemetry_mode_env  : 2; return true; }
  } else if (eq(group, "Time")) {
    if (eq(label, "Set time (UTC)")) {
      uint32_t e = rtc_clock.getCurrentTime();
      DateTime dt(e);
      snprintf(val, len, "%04u-%02u-%02u %02u:%02u", dt.year(), dt.month(), dt.day(), dt.hour(), dt.minute());
      return true;
    }
    if (eq(label, "Sync from GPS")) { *sel = p->time_sync_gps ? 1 : 0; return true; }
    if (strncmp(label, "Time source ", 12) == 0) {
      int i = atoi(label + 12) - 1;
      if (i < 0 || i > 2) return false;
      snprintf(val, len, "%s", p->time_source[i][0] ? p->time_source[i] : "(none)");
      return true;
    }
  } else if (eq(group, "Data")) {
    if (eq(label, "Debug logs")) {
#ifdef MESH_DEBUG
      strncpy(val, "USB serial", len - 1);
#else
      strncpy(val, "off (MESH_DEBUG flag)", len - 1);
#endif
      val[len - 1] = 0; return true;
    }
  } else if (eq(group, "Tuning")) {
    if (eq(label, "Airtime factor")) { snprintf(val, len, "%.2f", p->airtime_factor); return true; }
    if (eq(label, "RX delay base"))  { snprintf(val, len, "%.0f", p->rx_delay_base); return true; }
    if (eq(label, "Default scope"))  { snprintf(val, len, "%s", p->default_scope_name[0] ? p->default_scope_name : "(none)"); return true; }
  } else if (eq(group, "Security")) {
    if (eq(label, "BLE pin")) { snprintf(val, len, "%u", (unsigned)p->ble_pin); return true; }
    if (eq(label, "Active BLE pin")) {
      uint32_t a = the_mesh.getBLEPin();
      if (a) snprintf(val, len, "%u", (unsigned)a);
      else { strncpy(val, "(BLE off)", len - 1); val[len - 1] = 0; }
      return true;
    }
  } else if (eq(group, "Device")) {
    if (eq(label, "Contacts")) { snprintf(val, len, "%d / %d used", the_mesh.getNumContacts(), MAX_CONTACTS); return true; }
    if (eq(label, "Battery/storage")) {
      uint16_t mv; uint32_t used, total; the_mesh.getBattAndStorage(mv, used, total);
      snprintf(val, len, "%u mV  %u/%u KB", (unsigned)mv, (unsigned)used, (unsigned)total);
      return true;
    }
    if (eq(label, "Firmware")) { strncpy(val, FIRMWARE_VERSION, len - 1); val[len - 1] = 0; return true; }
    if (eq(label, "Device"))   { strncpy(val, board.getManufacturerName(), len - 1); val[len - 1] = 0; return true; }
    if (eq(label, "On-screen keyboard")) { *sel = lvd_osk_enabled() ? 1 : 0; return true; }
    if (eq(label, "Buzzer quiet"))       { *sel = p->buzzer_quiet ? 1 : 0; return true; }
  }
  return false;
}

extern "C" void lvd_cfg_set(const char* group, const char* label, const char* val, int sel) {
  NodePrefs* p = the_mesh.getNodePrefs();
  if (eq(group, "Public info")) {
    if (eq(label, "Node name") && val)  { the_mesh.setAdvertName(val); return; }
    if (eq(label, "Location policy"))   { the_mesh.setAdvertLocPolicy((uint8_t)sel); return; }
  } else if (eq(group, "Radio")) {
    if (eq(label, "Preset")) {   // last option is "Custom": informational, no change
      if (sel >= 0 && sel < RADIO_PLAN_N) {
        const RadioPlan& r = RADIO_PLANS[sel];
        apply_radio(r.freq, r.bw, r.sf, r.cr, p->client_repeat);
      }
      return;
    }
    if (eq(label, "TX power") && val)          { the_mesh.setTxPower((int8_t)atoi(val)); return; }
    if (eq(label, "Frequency") && val)         { apply_radio((float)atof(val), p->bw, p->sf, p->cr, p->client_repeat); return; }
    if (eq(label, "Spreading factor") && val)  { apply_radio(p->freq, p->bw, (uint8_t)atoi(val), p->cr, p->client_repeat); return; }
    if (eq(label, "Coding rate") && val)       { apply_radio(p->freq, p->bw, p->sf, (uint8_t)atoi(val), p->client_repeat); return; }
    if (eq(label, "Client repeat"))            { apply_radio(p->freq, p->bw, p->sf, p->cr, (uint8_t)(sel != 0)); return; }
    if (eq(label, "Channel activity detection")) { the_mesh.setCADEnabled(sel != 0); return; }
    if (eq(label, "RX boosted gain")) { the_mesh.setRxBoostedGain(sel != 0); return; }
    if (eq(label, "Bandwidth") && sel >= 0 && sel < BW_N) { apply_radio(p->freq, BW_KHZ[sel], p->sf, p->cr, p->client_repeat); return; }
  } else if (eq(group, "Contacts")) {
    if (eq(label, "Manual add"))                { the_mesh.setManualAdd(sel != 0); return; }
    if (eq(label, "Auto-add discovered"))       { the_mesh.setDiscAutoAdd(sel != 0); return; }
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
  } else if (eq(group, "Telemetry")) {
    if (eq(label, "Base"))        { the_mesh.setTelemetryModes((uint8_t)sel, p->telemetry_mode_loc, p->telemetry_mode_env); return; }
    if (eq(label, "Location"))    { the_mesh.setTelemetryModes(p->telemetry_mode_base, (uint8_t)sel, p->telemetry_mode_env); return; }
    if (eq(label, "Environment")) { the_mesh.setTelemetryModes(p->telemetry_mode_base, p->telemetry_mode_loc, (uint8_t)sel); return; }
  } else if (eq(group, "Time")) {
    if (eq(label, "Set time (UTC)") && val) {
      unsigned y, mo, d, h, mi;
      if (sscanf(val, "%u-%u-%u %u:%u", &y, &mo, &d, &h, &mi) == 5) {
        DateTime dt(y, mo, d, h, mi, 0);
        the_mesh.setDeviceTime(dt.unixtime());        // refuses to go backwards (mesh timestamps)
      } else if (val[0] >= '0' && val[0] <= '9' && !strchr(val, '-')) {
        the_mesh.setDeviceTime((uint32_t)strtoul(val, NULL, 10));   // plain epoch seconds
      }
      return;
    }
#if ENV_INCLUDE_GPS == 1
    if (eq(label, "Sync from GPS")) { the_mesh.setTimeSyncFromGps(sel != 0); return; }
#endif
    if (strncmp(label, "Time source ", 12) == 0 && val) { the_mesh.setTimeSource(atoi(label + 12) - 1, val); return; }
  } else if (eq(group, "Tuning")) {
    if (eq(label, "Airtime factor") && val) { the_mesh.setTuningParams(p->rx_delay_base, (float)atof(val)); return; }
    if (eq(label, "RX delay base") && val)  { the_mesh.setTuningParams((float)atof(val), p->airtime_factor); return; }
  } else if (eq(group, "Security")) {
    if (eq(label, "BLE pin") && val) { the_mesh.setBlePin((uint32_t)strtoul(val, NULL, 10)); return; }
  } else if (eq(group, "Device")) {
    if (eq(label, "On-screen keyboard")) { lvd_osk_set(sel != 0); return; }
    if (eq(label, "Buzzer quiet"))       { the_mesh.setBuzzerQuiet(sel != 0); return; }
  }
}

static void ui_dump_packet_ring(void);   // defined after the packet ring below

extern "C" void lvd_cfg_action(const char* group, const char* label) {
  if (eq(group, "Public info")) {
    if (eq(label, "Send advert"))         { the_mesh.advert();      return; }
    if (eq(label, "Send advert (flood)")) { the_mesh.advertFlood(); return; }
  } else if (eq(group, "Tuning")) {
    if (eq(label, "Clear default scope")) { the_mesh.setDefaultFloodScope("", NULL); return; }
  } else if (eq(group, "Device")) {
    if (eq(label, "Reboot")) { the_mesh.rebootDevice(); return; }
    // Factory reset goes through lvd_factory_reset() after the confirm dialog (lv_settings.c).
  } else if (eq(group, "Data")) {
    // One-way text dumps to USB serial. Safe alongside the companion frame
    // protocol (we only WRITE; reading Serial here would steal frame bytes).
    // SD backup / restore (paths under /meshcore on the card)
    if (eq(label, "Backup config"))    { lv_ui_toast(data_backup_config()  ? "Config saved to SD"  : "Backup failed (no card?)");  return; }
    if (eq(label, "Restore config"))   { lv_ui_toast(data_restore_config() ? "Config restored"      : "Restore failed (no file?)"); return; }
    if (eq(label, "Backup contacts"))  { lv_ui_toast(data_backup_appdata()  ? "Contacts + channels saved"    : "Backup failed (no card?)");  return; }
    if (eq(label, "Restore contacts")) { lv_ui_toast(data_restore_appdata() ? "Contacts + channels restored" : "Restore failed (no file?)"); return; }
    if (eq(label, "Export packets")) { ui_dump_packet_ring(); return; }   // serial (defined after the ring)
  }
}

extern "C" void lvd_factory_reset(void) {   // called only after the UI confirm dialog
  the_mesh.factoryReset();
}

// ---- statistics ------------------------------------------------------------
#define STATS_HIST 40
static int s_noise_hist[STATS_HIST];
static int s_noise_head = 0;   // next write slot
static int s_noise_n    = 0;   // valid samples
static int s_batt_hist[STATS_HIST];   // battery mV, same rolling window
static int s_batt_head = 0;
static int s_batt_n     = 0;

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
  out->free_ram_kb = (unsigned)(ESP.getFreeHeap() / 1024);
  out->flash_used_kb = used; out->flash_total_kb = total;
  out->err_flags = the_mesh.getErrFlags();
  out->num_contacts = the_mesh.getNumContacts(); out->max_contacts = MAX_CONTACTS;
  out->num_channels = lvd_chan_count();
  // roll the battery history alongside the noise history
  s_batt_hist[s_batt_head] = mv;
  s_batt_head = (s_batt_head + 1) % STATS_HIST;
  if (s_batt_n < STATS_HIST) s_batt_n++;
}

extern "C" int lvd_stats_noise_history(int* out, int max) {
  int n = s_noise_n < max ? s_noise_n : max;
  for (int i = 0; i < n; i++) {
    int idx = (s_noise_head - n + i + STATS_HIST * 2) % STATS_HIST;
    out[i] = s_noise_hist[idx];
  }
  return n;
}
extern "C" const char* lvd_stats_err_str(void) {
  static char b[48];
  uint16_t f = the_mesh.getErrFlags();
  if (f == 0) { strcpy(b, "None"); return b; }
  int k = 0; b[0] = 0;
  if (f & ERR_EVENT_FULL)            k += snprintf(b + k, sizeof(b) - k, "%sQueue full", k ? ", " : "");
  if (f & ERR_EVENT_CAD_TIMEOUT)     k += snprintf(b + k, sizeof(b) - k, "%sCAD timeout", k ? ", " : "");
  if (f & ERR_EVENT_STARTRX_TIMEOUT) k += snprintf(b + k, sizeof(b) - k, "%sRX-start timeout", k ? ", " : "");
  if (!k) snprintf(b, sizeof(b), "0x%04X", f);
  return b;
}
extern "C" void lvd_stats_reset(void) {
  radio_driver.resetStats();   // recv / sent / recv-errors
  the_mesh.resetStats();       // flood/direct counters + err flags (Dispatcher)
}
extern "C" int lvd_stats_batt_history(int* out, int max) {
  int n = s_batt_n < max ? s_batt_n : max;
  for (int i = 0; i < n; i++) {
    int idx = (s_batt_head - n + i + STATS_HIST * 2) % STATS_HIST;
    out[i] = s_batt_hist[idx];
  }
  return n;
}

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
// recent expected-ack CRCs of our outbound DMs, so the analyzer can flag "ACK for our message"
#define OUR_ACKS 8
static uint32_t s_our_acks[OUR_ACKS];
static int      s_our_ack_head = 0;
struct PktRec { uint8_t header; int rssi; int snr_q; int len; uint32_t t;
                uint32_t epoch;    // RTC time at RX (absolute timestamp)
                uint32_t payhash;  // FNV-1a of type+payload (path excluded), for rebroadcast counting
                int16_t  freq_err; // carrier offset of last RX, Hz/100 (INT16_MIN = unknown)
                uint8_t raw[PKT_RAW]; uint8_t rawlen; };
static PktRec s_pkt[PKT_LOG];
static int s_pkt_head = 0, s_pkt_n = 0;
static unsigned s_pkt_total = 0;   // monotonic, for change detection

// offset of the payload within the raw frame: header [+4 transport] pathlen path...
// (-1 if the frame is too short to parse)
static int pkt_payload_off(const uint8_t* b, int rl) {
  if (rl < 2) return -1;
  int route = b[0] & 0x03;
  int off = (route == 0 || route == 3) ? 5 : 1;   // TRANSPORT_* routes carry two 16-bit codes
  if (off >= rl) return -1;
  uint8_t plf = b[off++];
  off += (plf & 63) * ((plf >> 6) + 1);
  return (off <= rl) ? off : -1;
}

void ui_log_packet(float snr, float rssi, const uint8_t* raw, int len) {
  PktRec& r = s_pkt[s_pkt_head];
  r.header = (len > 0) ? raw[0] : 0;
  r.rssi = (int)rssi;
  r.snr_q = (int)(snr * 4.0f);
  r.len = len;
  r.t = millis();
  r.epoch = rtc_clock.getCurrentTime();
  { float fe = radio_driver.getLastFreqError();   // valid now (still the last-RX state)
    float scaled = fe / 100.0f;
    r.freq_err = (scaled > 32000.0f || scaled < -32000.0f) ? INT16_MIN : (int16_t)scaled; }
  r.rawlen = (uint8_t)(len > PKT_RAW ? PKT_RAW : (len < 0 ? 0 : len));
  if (raw && r.rawlen) memcpy(r.raw, raw, r.rawlen);
  // FNV-1a over payload type + payload bytes; the path is excluded so flood
  // rebroadcasts of the same packet hash identically (payhash + len match)
  uint32_t h = 2166136261u;
  h = (h ^ ((r.header >> 2) & 0x0F)) * 16777619u;
  int po = pkt_payload_off(r.raw, r.rawlen);
  if (po >= 0) for (int i = po; i < r.rawlen; i++) h = (h ^ r.raw[i]) * 16777619u;
  r.payhash = h;
  s_pkt_head = (s_pkt_head + 1) % PKT_LOG;
  if (s_pkt_n < PKT_LOG) s_pkt_n++;
  s_pkt_total++;
}

// dump the capture ring (newest-first) to USB serial as CSV+hex, for offline
// analysis. Write-only, so it's safe alongside the companion frame protocol.
static void ui_dump_packet_ring(void) {
  Serial.println("\n=== meshcore packet capture v1 ===");
  Serial.printf("# %d packets  (age_ms, len, snr_q, rssi, freq_err_hz, hex)\n", s_pkt_n);
  for (int k = 0; k < s_pkt_n; k++) {
    int idx = (s_pkt_head - 1 - k + PKT_LOG * 2) % PKT_LOG;
    const PktRec& q = s_pkt[idx];
    long fe = (q.freq_err == INT16_MIN) ? 0 : (long)q.freq_err * 100;
    Serial.printf("%lu,%d,%d,%d,%ld,", (unsigned long)(millis() - q.t), q.len, q.snr_q, q.rssi, fe);
    for (int i = 0; i < q.rawlen; i++) Serial.printf("%02X", q.raw[i]);
    Serial.println();
  }
  Serial.println("=== end packet capture ===");
}

// resolve a 1-byte path hash (== a node's pub_key[0]) to a saved contact name
static const char* contact_name_by_hash(uint8_t h) {
  static char nm[32];
  for (int i = MAX_ANON_CONTACTS, n = the_mesh.getTotalContactSlots(); i < n; i++) {
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
  for (int i = MAX_ANON_CONTACTS, n = the_mesh.getTotalContactSlots(); i < n; i++) {
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
  if (ptype == 0x09) {   // TRACE: the path field holds accumulated per-hop SNRs, not hashes
    if (hcount == 0) snprintf(v, sizeof(v), "(no hops yet)");
    else {
      int k = 0;
      for (int j = 0; j < hcount && off + j * hsize < rl; j++) {
        int8_t sq = (int8_t)b[off + j * hsize];
        int v10 = sq * 10 / 4, a = v10 < 0 ? -v10 : v10;
        k += snprintf(v + k, sizeof(v) - k, "%s%s%d.%d", j ? ", " : "", v10 < 0 ? "-" : "", a / 10, a % 10);
        if (k >= (int)sizeof(v) - 10) break;
      }
      snprintf(v + strlen(v), sizeof(v) - strlen(v), " dB");
    }
    kv_add(out, &n, max, "Hop SNRs", v);
  } else {
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
  }
  int payoff = off + pbytes;
  int paylen = p.len - payoff; if (paylen < 0) paylen = 0;
  const uint8_t* pl = b + payoff;
  int plav = rl - payoff; if (plav < 0) plav = 0;

  switch (ptype) {
    case 0x00: case 0x01: case 0x02: case 0x08: {  // REQ/RESP/TXT/PATH: dest+src hash
      for (int f = 0; f < 2 && plav >= f + 1; f++) {
        const char* nm = contact_name_by_hash(pl[f]);
        if (nm) snprintf(v, sizeof(v), "0x%02X (%s)", pl[f], nm);
        else    snprintf(v, sizeof(v), "0x%02X", pl[f]);
        kv_add(out, &n, max, f == 0 ? "Dest" : "Source", v);
      }
      break;
    }
    case 0x03:                                    // ACK
      if (plav >= 1) { int k = snprintf(v, sizeof(v), "0x"); for (int j = 0; j < plav && j < 8 && k < (int)sizeof(v) - 2; j++) k += snprintf(v + k, sizeof(v) - k, "%02X", pl[j]); kv_add(out, &n, max, "ACK code", v); }
      if (plav >= 4) {   // does the 4-byte ACK CRC match one of our pending outbound DMs?
        uint32_t code; memcpy(&code, pl, 4);
        for (int j = 0; j < OUR_ACKS; j++) if (s_our_acks[j] == code) { kv_add(out, &n, max, "Matches", "ACK for our message"); break; }
      }
      break;
    case 0x05: case 0x06:                          // GRP_TXT/GRP_DATA: channel hash + encrypted (MAC + data)
      if (plav >= 1) {
        // find our channel(s) whose hash matches, and try to decrypt (MAC-checked)
        ChannelDetails match; bool have_ch = false; uint8_t dec[MAX_PACKET_PAYLOAD]; int dlen = -1;
        for (int ci = 0; ci < MAX_GROUP_CHANNELS; ci++) {
          ChannelDetails cd;
          if (!the_mesh.getChannel(ci, cd) || !cd.name[0] || cd.channel.hash[0] != pl[0]) continue;
          match = cd; have_ch = true;
          if (plav > 3) dlen = mesh::Utils::MACThenDecrypt(cd.channel.secret, dec, pl + 1, plav - 1);
          if (dlen > 0) break;   // this channel's key worked
        }
        if (have_ch) snprintf(v, sizeof(v), "0x%02X (%s)", pl[0], match.name);
        else         snprintf(v, sizeof(v), "0x%02X (not our channel)", pl[0]);
        kv_add(out, &n, max, "Channel", v);

        if (dlen > 0) {
          if (ptype == 0x05 && dlen >= 5) {          // GRP_TXT: timestamp(4) type(1) text
            dec[dlen] = 0;
            kv_add(out, &n, max, "Message", (const char*)&dec[5]);
          } else if (ptype == 0x06 && dlen >= 3) {   // GRP_DATA: data_type(2) len(1) blob
            uint16_t dt = dec[0] | (dec[1] << 8);
            snprintf(v, sizeof(v), "0x%04X (%u B)", dt, dec[2]);
            kv_add(out, &n, max, "Data type", v);
          }
        } else if (have_ch) {
          kv_add(out, &n, max, "Message", "(decrypt failed)");
        } else {
          kv_add(out, &n, max, "Message", "(encrypted - no key)");
        }
      }
      break;
    case 0x04: {                                   // ADVERT: pubkey(32) ts(4) sig(64) appdata
      if (plav >= 6) {
        const char* nm = contact_name_by_pubkey6(pl);
        if (nm) snprintf(v, sizeof(v), "%s (%02X%02X%02X..)", nm, pl[0], pl[1], pl[2]);
        else    snprintf(v, sizeof(v), "%02X%02X%02X%02X%02X%02X..", pl[0], pl[1], pl[2], pl[3], pl[4], pl[5]);
        kv_add(out, &n, max, "Advertiser", v);
      }
      if (plav >= 36) {                            // advert creation time vs our clock = sender clock skew
        uint32_t ts; memcpy(&ts, pl + 32, 4);
        int32_t d = (int32_t)(ts - p.epoch);       // relative to when WE received it
        if (d > -3 && d < 3) snprintf(v, sizeof(v), "in sync with ours");
        else                 snprintf(v, sizeof(v), "%+d s vs our clock", (int)d);
        kv_add(out, &n, max, "Sender clock", v);
      }
      if (plav > 100) {                            // appdata: flags [latlon] [feats] name
        AdvertDataParser ap(pl + 100, (uint8_t)(plav - 100));
        if (ap.isValid()) {
          const char* tn = ap.getType() == ADV_TYPE_CHAT     ? "Companion" :
                           ap.getType() == ADV_TYPE_REPEATER ? "Repeater"  :
                           ap.getType() == ADV_TYPE_ROOM     ? "Room"      :
                           ap.getType() == ADV_TYPE_SENSOR   ? "Sensor"    : "Node";
          snprintf(v, sizeof(v), "%s (%s)", ap.hasName() ? ap.getName() : "(unnamed)", tn);
          kv_add(out, &n, max, "Node", v);
          if (ap.hasLatLon()) {
            char db[14] = ""; bool hd = fmt_distance(ap.getIntLat(), ap.getIntLon(), db, sizeof(db));
            snprintf(v, sizeof(v), "%.4f, %.4f%s%s%s", ap.getLat(), ap.getLon(),
                     hd ? " (" : "", db, hd ? ")" : "");
            kv_add(out, &n, max, "Position", v);
          }
        }
      }
      break;
    }
    case 0x09: {                                   // TRACE: tag(4) auth(4) flags(1) route...
      if (plav >= 4) { uint32_t tag; memcpy(&tag, pl, 4); snprintf(v, sizeof(v), "0x%08X", (unsigned)tag); kv_add(out, &n, max, "Trace tag", v); }
      if (plav >= 9) {                             // remaining payload = target route (hashes)
        uint8_t tflags = pl[8];
        int hsz = 1 << (tflags & 0x03);
        int k = 0; v[0] = 0;
        for (int j = 0; 9 + j * hsz < plav && k < (int)sizeof(v) - 8; j++) {
          uint8_t hb = pl[9 + j * hsz];
          const char* nm2 = contact_name_by_hash(hb);
          const char* sep = (j == 0) ? "" : " > ";
          if (nm2) k += snprintf(v + k, sizeof(v) - k, "%s%s", sep, nm2);
          else     k += snprintf(v + k, sizeof(v) - k, "%s%02X", sep, hb);
        }
        if (v[0]) kv_add(out, &n, max, "Route", v);
      }
      break;
    }
    default: break;
  }

  { int v10 = p.snr_q * 10 / 4, a = v10 < 0 ? -v10 : v10;
    snprintf(v, sizeof(v), "%s%d.%d dB / %d dBm", v10 < 0 ? "-" : "", a / 10, a % 10, p.rssi);
    kv_add(out, &n, max, "SNR / RSSI", v); }
  // link margin: how far this SNR sits above the SF's demod floor (Semtech, x10 dB)
  { int sf = the_mesh.getNodePrefs()->sf;
    static const int LIMIT_X10[] = { -75, -100, -125, -150, -175, -200 };   // SF7..SF12
    if (sf >= 7 && sf <= 12) {
      int margin10 = (p.snr_q * 10 / 4) - LIMIT_X10[sf - 7];
      int a = margin10 < 0 ? -margin10 : margin10;
      snprintf(v, sizeof(v), "%s%d.%d dB above SF%d floor", margin10 < 0 ? "-" : "", a / 10, a % 10, sf);
      kv_add(out, &n, max, "Link margin", v);
    } }
  if (p.freq_err != INT16_MIN) {
    long hz = (long)p.freq_err * 100;
    snprintf(v, sizeof(v), "%+ld Hz", hz);
    kv_add(out, &n, max, "Freq error", v);
  }
  snprintf(v, sizeof(v), "%d B (payload %d B)", p.len, paylen);     kv_add(out, &n, max, "Length", v);
  snprintf(v, sizeof(v), "%u ms", (unsigned)radio_driver.getEstAirtimeFor(p.len));
  kv_add(out, &n, max, "Airtime", v);
  snprintf(v, sizeof(v), "0x%02X", hdr);                            kv_add(out, &n, max, "Header", v);
  { uint32_t age = (millis() - p.t) / 1000;
    char ab[12] = "";
    if (p.epoch > 1000000)   // absolute wall-clock time when the RTC is set
      snprintf(ab, sizeof(ab), "%02u:%02u:%02u  ", (unsigned)((p.epoch / 3600) % 24),
               (unsigned)((p.epoch / 60) % 60), (unsigned)(p.epoch % 60));
    if (age < 60) snprintf(v, sizeof(v), "%s%us ago", ab, (unsigned)age);
    else          snprintf(v, sizeof(v), "%s%um ago", ab, (unsigned)(age / 60));
    kv_add(out, &n, max, "Received", v); }
  // rebroadcast detection: the payhash covers type+payload but NOT the path,
  // so flood copies (whose paths grow per hop) hash identically
  { int seen = 0;
    for (int j = 0; j < s_pkt_n; j++) if (s_pkt[j].payhash == p.payhash) seen++;
    if (seen > 1) {
      snprintf(v, sizeof(v), "%d copies in log (flood rebroadcasts)", seen);
      kv_add(out, &n, max, "Seen", v);
    } }
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
  for (int k = MAX_ANON_CONTACTS, n = the_mesh.getTotalContactSlots(); k < n; k++) {
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
static uint32_t s_disc_scan_epoch = 0;   // RTC time of the current scan (for "fresh" flagging)

extern "C" bool lvd_disc_get(int i, lvd_disc_t* out) {
  if (i < 0 || i >= g_disc_n) return false;
  MyMesh::DiscNode& d = g_disc[i];
  disc_label(d.pub_key, out->name, sizeof(out->name));
  out->type = d.type;
  int v10 = d.snr_q * 10 / 4, a = v10 < 0 ? -v10 : v10;
  if (d.rssi) snprintf(out->subtitle, sizeof(out->subtitle), "%s - SNR %s%d.%d dB - RSSI %d",
                       adv_type_name(d.type), v10 < 0 ? "-" : "", a / 10, a % 10, d.rssi);
  else        snprintf(out->subtitle, sizeof(out->subtitle), "%s - SNR %s%d.%d dB",
                       adv_type_name(d.type), v10 < 0 ? "-" : "", a / 10, a % 10);
  uint32_t now = rtc_clock.getCurrentTime();
  uint32_t age = (now > d.ts) ? now - d.ts : 0;
  if (age < 60)        snprintf(out->age, sizeof(out->age), "%us ago", (unsigned)age);
  else if (age < 3600) snprintf(out->age, sizeof(out->age), "%um ago", (unsigned)(age / 60));
  else                 snprintf(out->age, sizeof(out->age), "%uh ago", (unsigned)(age / 3600));
  int snr = d.snr_q / 4;
  out->bars = (d.snr_q == 0) ? 1 : (snr >= 5 ? 4 : (snr >= 0 ? 2 : 1));
  out->fresh = (s_disc_scan_epoch && d.ts >= s_disc_scan_epoch) ? 1 : 0;
  // distance + bearing from the responder's saved contact position, if any
  out->dist[0] = 0;
  ContactInfo c;
  for (int k = MAX_ANON_CONTACTS, cn = the_mesh.getTotalContactSlots(); k < cn; k++) {
    if (the_mesh.getContactByIdx((uint32_t)k, c) && memcmp(c.id.pub_key, d.pub_key, PUB_KEY_SIZE) == 0) {
      char db[12];
      if (fmt_distance(c.gps_lat, c.gps_lon, db, sizeof(db))) {
        const char* brg = heard_bearing(c.gps_lat, c.gps_lon);
        snprintf(out->dist, sizeof(out->dist), "%s%s%s", db, brg[0] ? " " : "", brg);
      }
      break;
    }
  }
  return true;
}
extern "C" const char* lvd_disc_summary(void) {
  static char b[56];
  int rep = 0, comp = 0, other = 0;
  for (int i = 0; i < g_disc_n; i++) {
    if (g_disc[i].type == ADV_TYPE_REPEATER) rep++;
    else if (g_disc[i].type == ADV_TYPE_CHAT) comp++;
    else other++;
  }
  if (g_disc_n == 0) { strncpy(b, "No neighbours yet", sizeof(b)); return b; }
  int k = snprintf(b, sizeof(b), "%d direct neighbour%s", g_disc_n, g_disc_n == 1 ? "" : "s");
  if (rep || comp) {
    k += snprintf(b + k, sizeof(b) - k, " \xC2\xB7 ");
    bool first = true;
    if (rep)  { k += snprintf(b + k, sizeof(b) - k, "%d repeater%s", rep, rep == 1 ? "" : "s"); first = false; }
    if (comp) { k += snprintf(b + k, sizeof(b) - k, "%s%d companion%s", first ? "" : ", ", comp, comp == 1 ? "" : "s"); }
  }
  return b;
}
// Paced active discovery: repeaters rate-limit and will ignore us if we ask too
// often, so enforce a single global 60s minimum between requests regardless of
// caller (screen entry, the 60s auto-poll, or the manual button).
#define DISC_REQ_MIN_MS 60000UL
static uint32_t s_disc_last_ms = 0;
static bool     s_disc_ever    = false;
static uint32_t disc_remaining_ms(void) {
  if (!s_disc_ever) return 0;
  uint32_t el = millis() - s_disc_last_ms;
  return el < DISC_REQ_MIN_MS ? DISC_REQ_MIN_MS - el : 0;
}
// Returns 0 when a request was actually sent, else the seconds until allowed.
extern "C" int lvd_disc_request(void) {
  uint32_t rem = disc_remaining_ms();
  if (rem > 0) return (int)((rem + 999) / 1000);
  the_mesh.sendNodeDiscoverReq();
  s_disc_last_ms = millis(); s_disc_ever = true;
  s_disc_scan_epoch = rtc_clock.getCurrentTime();   // responders after this are "fresh"
  return 0;
}
extern "C" int lvd_disc_next_secs(void) { return (int)((disc_remaining_ms() + 999) / 1000); }
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
#define MSG_LOG 64
// state: 0 incoming, 1 sent (channel / no ack), 2 pending (DM awaiting ack), 3 delivered
struct MsgRec { bool is_ch; int ch; uint8_t peer6[6]; char who[24]; char text[124]; bool out;
                uint32_t ts; uint32_t ack; uint32_t sent_ms; uint8_t state;
                uint8_t path_len;   // inbound route (encoded; 0xFF = direct/unknown)
                bool deleted; };
static MsgRec s_msg[MSG_LOG];
static int s_msg_head = 0, s_msg_n = 0;
static unsigned s_msg_total = 0;
#define ACK_TIMEOUT_MS 30000

// per-thread unread counters (channels by slot; DMs by pubkey prefix)
static int     s_ch_unread[MAX_GROUP_CHANNELS];
static struct { uint8_t pk6[6]; int n; } s_dm_unread[16];
static int     s_dm_unread_n = 0;
static int*    dm_unread_slot(const uint8_t* pk6, bool create) {
  for (int i = 0; i < s_dm_unread_n; i++) if (memcmp(s_dm_unread[i].pk6, pk6, 6) == 0) return &s_dm_unread[i].n;
  if (!create || s_dm_unread_n >= 16) return NULL;
  memcpy(s_dm_unread[s_dm_unread_n].pk6, pk6, 6); s_dm_unread[s_dm_unread_n].n = 0;
  return &s_dm_unread[s_dm_unread_n++].n;
}

// active conversation: a channel (s_conv_ch >= 0 = channel slot, 0 = Public) or a DM
static int         s_conv_ch = 0;
static uint8_t     s_conv_peer[6];
static ContactInfo s_conv_contact;
static bool        s_conv_has_contact = false;
static char        s_conv_chname[32] = "Public";

// is the conversation screen currently open on this exact thread?
static bool viewing_thread(bool is_ch, int ch, const uint8_t* peer6) {
  const char* cur = g_nav_sp > 0 ? g_nav_stack[g_nav_sp - 1] : "";
  if (strcmp(cur, "conv") != 0) return false;
  if (s_conv_ch >= 0) return is_ch && ch == s_conv_ch;
  return !is_ch && peer6 && memcmp(peer6, s_conv_peer, 6) == 0;
}

void ui_store_message(bool is_ch, int ch, const uint8_t* peer6, const char* who, const char* text, bool out, uint8_t path_len) {
  if (!out && !viewing_thread(is_ch, ch, peer6)) {   // unread unless already viewing this thread
    const char* cur = g_nav_sp > 0 ? g_nav_stack[g_nav_sp - 1] : "";
    if (strcmp(cur, "conv") != 0 && strcmp(cur, "chat") != 0) s_unread++;   // home-tile badge
    if (is_ch) { if (ch >= 0 && ch < MAX_GROUP_CHANNELS) s_ch_unread[ch]++; }
    else if (peer6) { int* u = dm_unread_slot(peer6, true); if (u) (*u)++; }   // per-thread badge
  }
  MsgRec& m = s_msg[s_msg_head];
  m.is_ch = is_ch; m.ch = ch; m.out = out;
  if (peer6) memcpy(m.peer6, peer6, 6); else memset(m.peer6, 0, 6);
  strncpy(m.who,  who  ? who  : "", sizeof(m.who)  - 1); m.who[sizeof(m.who)   - 1] = 0;
  strncpy(m.text, text ? text : "", sizeof(m.text) - 1); m.text[sizeof(m.text) - 1] = 0;
  m.ts = rtc_clock.getCurrentTime();
  m.ack = 0; m.sent_ms = millis();
  m.state = out ? 1 : 0;   // outgoing defaults to "sent"; DM upgraded to pending by lvd_chat_send
  m.path_len = path_len; m.deleted = false;
  s_msg_head = (s_msg_head + 1) % MSG_LOG;
  if (s_msg_n < MSG_LOG) s_msg_n++;
  s_msg_total++;
}
static int last_msg_idx(void) { return (s_msg_head - 1 + MSG_LOG) % MSG_LOG; }
static void fmt_hhmm(uint32_t e, char* out, int len);   // defined below

// does message at ring index idx belong to the active conversation?
static bool in_active_conv(int idx) {
  const MsgRec& m = s_msg[idx];
  if (m.deleted) return false;
  if (s_conv_ch >= 0) return m.is_ch && m.ch == s_conv_ch;
  return !m.is_ch && memcmp(m.peer6, s_conv_peer, 6) == 0;
}
// also: does a message belong to the Public channel (for the list preview)?
static bool is_public_msg(int idx) { return !s_msg[idx].deleted && s_msg[idx].is_ch && s_msg[idx].ch == 0; }

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
  for (int i = MAX_ANON_CONTACTS, n = the_mesh.getTotalContactSlots(); i < n; i++) {
    ContactInfo c;
    if (the_mesh.getContactByIdx((uint32_t)i, c) && strcmp(c.name, name) == 0) { out = c; return true; }
  }
  return false;
}

extern "C" void lvd_chat_open_public(void) { s_conv_ch = 0; strncpy(s_conv_chname, "Public", sizeof(s_conv_chname)); s_ch_unread[0] = 0; }
extern "C" void lvd_chat_open_channel(int i) {     // i = display index from the chat list
  chan_build();
  if (i < 0 || i >= g_chan_n) { lvd_chat_open_public(); return; }
  s_conv_ch = g_chan_idx[i];
  if (s_conv_ch >= 0 && s_conv_ch < MAX_GROUP_CHANNELS) s_ch_unread[s_conv_ch] = 0;   // mark read
  ChannelDetails c; the_mesh.getChannel(s_conv_ch, c);
  strncpy(s_conv_chname, c.name[0] ? c.name : "(unnamed)", sizeof(s_conv_chname) - 1);
  s_conv_chname[sizeof(s_conv_chname) - 1] = 0;
}
extern "C" void lvd_chat_open_dm(const char* contact_name) {
  s_conv_ch = -1;
  s_conv_has_contact = find_contact_by_name(contact_name, s_conv_contact);
  if (s_conv_has_contact) memcpy(s_conv_peer, s_conv_contact.id.pub_key, 6);
  else memset(s_conv_peer, 0, 6);
  int* u = dm_unread_slot(s_conv_peer, false); if (u) *u = 0;   // mark read
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
extern "C" int lvd_chat_chan_unread(int i) {
  chan_build();
  if (i < 0 || i >= g_chan_n) return 0;
  int slot = g_chan_idx[i];
  return (slot >= 0 && slot < MAX_GROUP_CHANNELS) ? s_ch_unread[slot] : 0;
}
extern "C" const char* lvd_chat_chan_time(int i) {
  static char t[8]; t[0] = 0;
  chan_build();
  if (i < 0 || i >= g_chan_n) return t;
  int slot = g_chan_idx[i];
  for (int k = s_msg_n - 1; k >= 0; k--) {
    int idx = (s_msg_head - s_msg_n + k + MSG_LOG * 2) % MSG_LOG;
    if (s_msg[idx].is_ch && s_msg[idx].ch == slot) { fmt_hhmm(s_msg[idx].ts, t, sizeof(t)); break; }
  }
  return t;
}

extern "C" int  lvd_chat_count(void) { return count_idx(in_active_conv); }
// "HH:MM" from an RTC epoch, or "" if the clock isn't set
static void fmt_hhmm(uint32_t e, char* out, int len) {
  if (e < 1000000) { out[0] = 0; return; }
  snprintf(out, len, "%02u:%02u", (unsigned)((e / 3600) % 24), (unsigned)((e / 60) % 60));
}
extern "C" bool lvd_chat_get(int i, lvd_msg_t* out) {
  int idx = nth_idx(i, in_active_conv);
  if (idx < 0) return false;
  const MsgRec& m = s_msg[idx];
  strncpy(out->sender, m.who,  sizeof(out->sender) - 1); out->sender[sizeof(out->sender) - 1] = 0;
  strncpy(out->text,   m.text, sizeof(out->text)  - 1); out->text[sizeof(out->text)   - 1] = 0;
  out->outgoing = m.out ? 1 : 0;
  fmt_hhmm(m.ts, out->time, sizeof(out->time));
  // status: 0 none, 1 sent, 2 pending, 3 delivered, 4 failed
  if (!m.out)            out->status = 0;
  else if (m.state == 3) out->status = 3;
  else if (m.state == 2) out->status = (millis() - m.sent_ms > ACK_TIMEOUT_MS) ? 4 : 2;
  else                   out->status = 1;
  out->can_resend = (out->status == 4 && !m.is_ch) ? 1 : 0;
  // route tag: how an inbound message reached us (low 6 bits of path_len = hops)
  out->route[0] = 0;
  if (!m.out) {
    int hops = (m.path_len == 0xFF) ? 0 : (m.path_len & 63);
    if (hops <= 0)      snprintf(out->route, sizeof(out->route), "direct");
    else if (hops == 1) snprintf(out->route, sizeof(out->route), "1 hop");
    else                snprintf(out->route, sizeof(out->route), "%d hops", hops);
  }
  return true;
}
// re-send message i's text into the active conversation (used for failed DMs)
extern "C" void lvd_chat_resend(int i) {
  int idx = nth_idx(i, in_active_conv);
  if (idx < 0) return;
  char text[124]; strncpy(text, s_msg[idx].text, sizeof(text) - 1); text[sizeof(text) - 1] = 0;
  s_msg[idx].deleted = true;    // drop the failed copy; send creates a fresh one
  lvd_chat_send(text);
}
extern "C" void lvd_chat_delete(int i) {
  int idx = nth_idx(i, in_active_conv);
  if (idx < 0) return;
  s_msg[idx].deleted = true;
  s_msg_total++;   // refresh
}
// true while any outbound DM in the active conversation is still awaiting an ack
// (so the conversation keeps refreshing until it flips to delivered/failed)
extern "C" int lvd_chat_has_pending(void) {
  for (int i = 0; i < s_msg_n; i++) {
    int idx = (s_msg_head - s_msg_n + i + MSG_LOG * 2) % MSG_LOG;
    if (s_msg[idx].state == 2 && in_active_conv(idx)) return 1;
  }
  return 0;
}
extern "C" unsigned lvd_chat_total(void) { return s_msg_total; }
// Emergency position share: plain text so EVERY client renders it (there is no
// public GRP_DATA location type in docs/number_allocations.md; the dev-only
// 0xFFxx range would be invisible to other people's apps).
extern "C" bool lvd_chat_send_location(void) {
  if (sensors.node_lat == 0.0 && sensors.node_lon == 0.0) return false;   // no GPS fix or manual position
  char msg[80];
  snprintf(msg, sizeof(msg), "EMERGENCY - my position: %.6f, %.6f", sensors.node_lat, sensors.node_lon);
  lvd_chat_send(msg);
  return true;
}
extern "C" void lvd_chat_send(const char* text) {
  if (!text || !text[0]) return;
  if (s_conv_ch >= 0) {
    if (the_mesh.sendChannelText(s_conv_ch, text)) ui_store_message(true, s_conv_ch, NULL, "You", text, true);
  } else if (s_conv_has_contact) {
    uint32_t ack, timeout;
    if (the_mesh.sendTextTo(s_conv_contact, text, ack, timeout)) {
      ui_store_message(false, -1, s_conv_peer, "You", text, true);
      if (ack) {
        s_our_acks[s_our_ack_head] = ack; s_our_ack_head = (s_our_ack_head + 1) % OUR_ACKS;
        MsgRec& m = s_msg[last_msg_idx()]; m.ack = ack; m.state = 2;   // awaiting delivery ack
      }
    }
  }
}
// delivery confirmation: an ack for one of our outbound DMs arrived
void ui_msg_confirmed(uint32_t ack) {
  for (int i = 0; i < s_msg_n; i++) {
    int idx = (s_msg_head - s_msg_n + i + MSG_LOG * 2) % MSG_LOG;
    if (s_msg[idx].state == 2 && s_msg[idx].ack == ack) { s_msg[idx].state = 3; s_msg_total++; return; }
  }
}

// ---- DM threads (distinct peers we have message history with) ---------------
static uint8_t g_dm_peer[16][6];
static int     g_dm_n = 0;
static bool find_contact_by_pubkey6(const uint8_t* p6, ContactInfo& out) {
  for (int i = MAX_ANON_CONTACTS, n = the_mesh.getTotalContactSlots(); i < n; i++) {
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
  out->preview[0] = 0; out->time[0] = 0;   // last message in this thread + its time
  for (int k = s_msg_n - 1; k >= 0; k--) {
    int idx = (s_msg_head - s_msg_n + k + MSG_LOG * 2) % MSG_LOG;
    const MsgRec& m = s_msg[idx];
    if (!m.is_ch && memcmp(m.peer6, g_dm_peer[i], 6) == 0) {
      strncpy(out->preview, m.text, sizeof(out->preview) - 1); out->preview[sizeof(out->preview) - 1] = 0;
      fmt_hhmm(m.ts, out->time, sizeof(out->time));
      break;
    }
  }
  int* u = dm_unread_slot(g_dm_peer[i], false); out->unread = u ? *u : 0;
  return true;
}
extern "C" void lvd_dm_open(int i) {
  if (i < 0 || i >= g_dm_n) return;
  s_conv_ch = -1;
  memcpy(s_conv_peer, g_dm_peer[i], 6);
  s_conv_has_contact = find_contact_by_pubkey6(g_dm_peer[i], s_conv_contact);
  int* u = dm_unread_slot(s_conv_peer, false); if (u) *u = 0;   // mark read
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
// a contact trace uses the firmware's learned path (not the manual s_tpath chain);
// remember the target so Repeat can re-run it.
static bool     s_tr_ct = false;
static uint8_t  s_tr_ct_pk[6];
static uint32_t s_tr_sent_ms = 0;        // when the in-flight trace was sent
static uint32_t s_tr_rtt_ms = 0;         // round-trip time of the last completed trace
// timeout scales with path length: base + per-hop (long flood-returns are slow)
#define TRACE_TIMEOUT_MS (6000 + 3000 * s_tpath_n)

void ui_store_trace(uint32_t tag, const uint8_t* hashes, const uint8_t* snrs,
                    uint8_t path_len, uint8_t path_sz, int8_t final_snr_q) {
  if (tag != s_tr_tag) return;   // only our own trace (also accepts a late result after timeout)
  int n = path_len >> path_sz;                      // SNR entries (== hops)
  if (n > TRACE_MAX) n = TRACE_MAX;
  s_tr_hops = n;
  for (int i = 0; i < n; i++) { s_tr_hash[i] = hashes[i]; s_tr_snr[i] = snrs[i]; }
  s_tr_final = final_snr_q;
  s_tr_rtt_ms = millis() - s_tr_sent_ms;
  s_tr_state = 2;
  s_tr_seq++;
}

static int trace_contact(const ContactInfo& c);   // defined below (contact trace)

// index (0..hops) of the weakest SNR across the whole path (incl. final "to you")
static int trace_weakest_idx(void) {
  int wi = 0; int8_t w = 127;
  for (int i = 0; i < s_tr_hops; i++) if (s_tr_snr[i] < w) { w = s_tr_snr[i]; wi = i; }
  if (s_tr_final < w) wi = s_tr_hops;
  return wi;
}

// ---- trace path builder ----
extern "C" void lvd_trace_path_clear(void) { s_tpath_n = 0; s_tr_ct = false; s_tr_state = 0; s_tr_seq++; }
extern "C" void lvd_trace_path_add(int i) {   // i = index in the saved repeater/room list
  if (s_tpath_n >= TRACE_MAX) return;
  s_tr_ct = false;                            // building a manual path
  ContactInfo c;
  if (rep_nth(0, i, c)) { s_tpath[s_tpath_n++] = c.id.pub_key[0]; }
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
  if (s_tr_ct) {   // Repeat of a contact trace: re-run over the contact's learned path
    ContactInfo c;
    if (find_contact_by_pubkey6(s_tr_ct_pk, c)) trace_contact(c);
    return;
  }
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
  out->weakest = (i == trace_weakest_idx()) ? 1 : 0;
  int pct = (v10 / 10 + 20) * 100 / 30;   // -20..+10 dB -> 0..100
  out->snr_pct = pct < 0 ? 0 : (pct > 100 ? 100 : pct);
  return true;
}
extern "C" unsigned lvd_trace_elapsed_ms(void) {
  return (s_tr_state == 1) ? (millis() - s_tr_sent_ms) : 0;
}
extern "C" const char* lvd_trace_summary(void) {
  static char b[64];
  if (s_tr_state != 2) { b[0] = 0; return b; }
  int wi = trace_weakest_idx();
  int8_t w = (wi < s_tr_hops) ? s_tr_snr[wi] : s_tr_final;
  int v10 = w * 10 / 4, a = v10 < 0 ? -v10 : v10;
  int total = s_tr_hops + 1;   // hops + final leg to us
  if (s_tr_rtt_ms >= 1000)
    snprintf(b, sizeof(b), "%d hop%s \xC2\xB7 RTT %.1f s \xC2\xB7 weakest %s%d.%d dB",
             total, total == 1 ? "" : "s", s_tr_rtt_ms / 1000.0, v10 < 0 ? "-" : "+", a / 10, a % 10);
  else
    snprintf(b, sizeof(b), "%d hop%s \xC2\xB7 RTT %u ms \xC2\xB7 weakest %s%d.%d dB",
             total, total == 1 ? "" : "s", (unsigned)s_tr_rtt_ms, v10 < 0 ? "-" : "+", a / 10, a % 10);
  return b;
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
#define CLI_LINES 24
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
    for (int k = MAX_ANON_CONTACTS, n = the_mesh.getTotalContactSlots(); k < n && g_replist_n < REPLIST_MAX; k++) {
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
static void rep_set_active(const ContactInfo& c) {
  s_rep_active = true;
  memcpy(s_rep_peer, c.id.pub_key, 6);
  strncpy(s_rep_name, c.name, sizeof(s_rep_name) - 1); s_rep_name[sizeof(s_rep_name) - 1] = 0;
  s_rep_login = 0; s_rep_have_status = false; s_cli_n = 0;
  s_rep_seq++;
}
extern "C" void lvd_rep_open(int scan, int i) {
  ContactInfo c;
  if (rep_nth(scan, i, c)) rep_set_active(c);
}
// open the admin target straight from a contact (peer card). Returns 0 if the
// contact is a repeater/room and was selected, 1 if not found, 2 if wrong type.
extern "C" int lvd_rep_open_contact(const char* name) {
  ContactInfo c;
  if (!find_contact_by_name(name, c)) return 1;
  if (!is_rep_type(c.type)) return 2;
  rep_set_active(c);
  return 0;
}

extern "C" const char* lvd_rep_name(void)        { return s_rep_name; }
extern "C" int         lvd_rep_login_state(void) { return s_rep_login; }
static char s_rep_pending_pw[16] = "";   // password of the in-flight login, saved on success
extern "C" void        lvd_rep_login(const char* password) {
  if (!s_rep_active) return;
  strncpy(s_rep_pending_pw, password ? password : "", sizeof(s_rep_pending_pw) - 1);
  s_rep_pending_pw[sizeof(s_rep_pending_pw) - 1] = 0;
  uint32_t timeout;
  if (the_mesh.uiLogin(s_rep_peer, password ? password : "", timeout)) s_rep_login = 1;  // pending
  else s_rep_login = 3;
  s_rep_seq++;
}
// auto-login: if this repeater has a remembered password, log in with it (no keyboard).
// Returns 1 if a login was started, 0 if nothing remembered.
extern "C" int lvd_rep_login_remembered(void) {
  if (!s_rep_active) return 0;
  char pw[16];
  if (!the_mesh.getRepPassword(s_rep_peer, pw, sizeof(pw))) return 0;
  lvd_rep_login(pw);
  return 1;
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
  if (s_rep_active && memcmp(pk6, s_rep_peer, 6) == 0) {
    s_rep_login = ok ? 2 : 3; s_rep_seq++;
    if (ok) {
      if (s_rep_pending_pw[0]) the_mesh.rememberRepPassword(pk6, s_rep_pending_pw);  // remember on success
      uint32_t timeout; the_mesh.uiRequestStatus(s_rep_peer, timeout);   // auto-fetch status on login
    }
    s_rep_pending_pw[0] = 0;
  }
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
  else                                    { snprintf(out->path, sizeof(out->path), "routed"); snprintf(out->hops, sizeof(out->hops), "%d", c.out_path_len & 63); }  // low 6 bits = hop count

  for (int i = 0; i < 32; i++) snprintf(out->pubkey + i * 2, 3, "%02x", c.id.pub_key[i]);

  int32_t lat = c.gps_lat, lon = c.gps_lon;
  AdvertPath h; bool hh = find_heard_by_pubkey(c.id.pub_key, h);
  if ((lat == 0 && lon == 0) && hh) { lat = h.gps_lat; lon = h.gps_lon; }
  if (lat == 0 && lon == 0) { snprintf(out->lat, sizeof(out->lat), "--"); snprintf(out->lon, sizeof(out->lon), "--"); }
  else { snprintf(out->lat, sizeof(out->lat), "%.4f", lat / 1e6); snprintf(out->lon, sizeof(out->lon), "%.4f", lon / 1e6); }

  if (hh && h.rssi)  snprintf(out->rssi, sizeof(out->rssi), "%d dBm", h.rssi); else snprintf(out->rssi, sizeof(out->rssi), "--");
  if (hh && h.snr_q) { int v10 = h.snr_q * 10 / 4, a = v10 < 0 ? -v10 : v10; snprintf(out->snr, sizeof(out->snr), "%s%d.%d dB", v10 < 0 ? "-" : "", a / 10, a % 10); }
  else snprintf(out->snr, sizeof(out->snr), "--");
  // Distance needs both a peer fix (lat/lon above) and our own position.
  if (!fmt_distance(lat, lon, out->dist, sizeof(out->dist))) snprintf(out->dist, sizeof(out->dist), "--");

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
extern "C" bool lvd_peer_add(const char* name) {   // save a heard-only node as a contact
  ContactInfo cand[16];
  int n = the_mesh.getHeardCandidates(cand, 16);
  for (int i = 0; i < n; i++) {
    if (ci_strcmp(cand[i].name, name) == 0) return the_mesh.addHeardContact(cand[i].id.pub_key);
  }
  return false;
}

// ---- remote telemetry (peer card "Telemetry" button) ------------------------
// Single in-flight request; the CayenneLPP reply arrives via onTelemetryResponse.
static uint8_t  s_ptel_key[6];
static uint8_t  s_ptel_raw[96];
static int      s_ptel_len = 0;
static int      s_ptel_state = 0;        // 0 idle, 1 waiting, 2 have, 3 timed out
static uint32_t s_ptel_sent_ms = 0;
#define PTEL_TIMEOUT_MS 30000

void ui_peer_on_telemetry(const uint8_t* pk6, const uint8_t* data, uint8_t len) {
  if (s_ptel_state != 1 || memcmp(pk6, s_ptel_key, 6) != 0) return;
  if (len > sizeof(s_ptel_raw)) len = sizeof(s_ptel_raw);
  memcpy(s_ptel_raw, data, len);
  s_ptel_len = len;
  s_ptel_state = 2;
}
extern "C" bool lvd_peer_telem_request(const char* name) {
  ContactInfo c;
  if (!find_contact_by_name(name, c)) return false;
  uint32_t est;
  if (!the_mesh.uiRequestTelemetry(c.id.pub_key, est)) return false;
  memcpy(s_ptel_key, c.id.pub_key, 6);
  s_ptel_len = 0; s_ptel_state = 1; s_ptel_sent_ms = millis();
  return true;
}
extern "C" int lvd_peer_telem_state(const char* name) {
  ContactInfo c;   // results are for one peer; another card sees idle
  if (!find_contact_by_name(name, c) || memcmp(c.id.pub_key, s_ptel_key, 6) != 0) return 0;
  if (s_ptel_state == 1 && millis() - s_ptel_sent_ms > PTEL_TIMEOUT_MS) s_ptel_state = 3;
  return s_ptel_state;
}
// Minimal CayenneLPP decode (big-endian): [channel][type][payload]...
static int lpp_size(uint8_t t) {
  switch (t) {
    case LPP_DIGITAL_INPUT: case LPP_DIGITAL_OUTPUT: case LPP_PRESENCE:
    case LPP_RELATIVE_HUMIDITY: case LPP_PERCENTAGE: case LPP_SWITCH: return 1;
    case LPP_ANALOG_INPUT: case LPP_ANALOG_OUTPUT: case LPP_TEMPERATURE:
    case LPP_VOLTAGE: case LPP_CURRENT: case LPP_BAROMETRIC_PRESSURE:
    case LPP_ALTITUDE: case LPP_DIRECTION: case LPP_POWER: return 2;
    case LPP_GENERIC_SENSOR: case LPP_FREQUENCY: case LPP_UNIXTIME:
    case LPP_DISTANCE: case LPP_ENERGY: return 4;
    case LPP_GPS: return 9;
    default: return -1;   // unknown size: must stop parsing
  }
}
static uint16_t be16(const uint8_t* p) { return ((uint16_t)p[0] << 8) | p[1]; }
static int32_t  be24s(const uint8_t* p) {
  int32_t v = ((int32_t)p[0] << 16) | ((int32_t)p[1] << 8) | p[2];
  return (v & 0x800000) ? v - 0x1000000 : v;
}
extern "C" int lvd_peer_telem_get(lvd_kv_t* out, int max) {
  if (s_ptel_state != 2) return 0;
  int n = 0, i = 0;
  while (i + 2 <= s_ptel_len && n < max) {
    uint8_t ch = s_ptel_raw[i], ty = s_ptel_raw[i + 1];
    int sz = lpp_size(ty);
    if (sz < 0 || i + 2 + sz > s_ptel_len) {
      snprintf(out[n].label, sizeof(out[n].label), "(more)");
      snprintf(out[n].value, sizeof(out[n].value), "+%d bytes (type %u)", s_ptel_len - i, ty);
      n++;
      break;
    }
    const uint8_t* p = &s_ptel_raw[i + 2];
    lvd_kv_t* o = &out[n];
    o->label[0] = o->value[0] = 0;
    switch (ty) {
      case LPP_VOLTAGE:
        snprintf(o->label, sizeof(o->label), ch == TELEM_CHANNEL_SELF ? "Battery" : "Voltage %u", ch);
        snprintf(o->value, sizeof(o->value), "%.2f V", be16(p) / 100.0f);
        break;
      case LPP_TEMPERATURE:
        snprintf(o->label, sizeof(o->label), "Temp %u", ch);
        snprintf(o->value, sizeof(o->value), "%.1f C", (int16_t)be16(p) / 10.0f);
        break;
      case LPP_RELATIVE_HUMIDITY:
        snprintf(o->label, sizeof(o->label), "Humidity %u", ch);
        snprintf(o->value, sizeof(o->value), "%.1f %%", p[0] / 2.0f);
        break;
      case LPP_BAROMETRIC_PRESSURE:
        snprintf(o->label, sizeof(o->label), "Pressure %u", ch);
        snprintf(o->value, sizeof(o->value), "%.1f hPa", be16(p) / 10.0f);
        break;
      case LPP_GPS:
        snprintf(o->label, sizeof(o->label), "Position");
        snprintf(o->value, sizeof(o->value), "%.4f, %.4f  alt %dm",
                 be24s(p) / 10000.0, be24s(p + 3) / 10000.0, (int)(be24s(p + 6) / 100));
        break;
      case LPP_UNIXTIME: {
        uint32_t e = ((uint32_t)be16(p) << 16) | be16(p + 2);
        snprintf(o->label, sizeof(o->label), "Clock");
        snprintf(o->value, sizeof(o->value), "%u", (unsigned)e);
        break;
      }
      case LPP_PERCENTAGE:
        snprintf(o->label, sizeof(o->label), "Level %u", ch);
        snprintf(o->value, sizeof(o->value), "%u %%", p[0]);
        break;
      default:
        snprintf(o->label, sizeof(o->label), "Type %u", ty);
        snprintf(o->value, sizeof(o->value), "ch %u (%d bytes)", ch, sz);
        break;
    }
    n++;
    i += 2 + sz;
  }
  return n;
}

// ---- one-tap trace over a contact's learned path ----------------------------
// start a trace over a contact's learned path; seeds the trace-screen state
static int trace_contact(const ContactInfo& c) {   // 0 sent, 2 no routed path / failed
  if (c.out_path_len == OUT_PATH_UNKNOWN || c.out_path_len == 0) return 2;  // flood or direct: nothing to trace
  // The firmware traces over the contact's own learned out_path; we don't build a
  // manual hop chain here (out_path is byte-encoded, not one hash per hop). Show the
  // contact name while tracing and remember it so Repeat can re-run.
  s_tpath_n = 0;
  s_tr_ct = true; memcpy(s_tr_ct_pk, c.id.pub_key, 6);
  strncpy(s_tr_target, c.name, sizeof(s_tr_target) - 1); s_tr_target[sizeof(s_tr_target) - 1] = 0;
  s_tr_hops = 0; s_tr_seq++;
  uint32_t tag;
  if (the_mesh.sendTrace((ContactInfo&)c, tag)) { s_tr_tag = tag; s_tr_state = 1; s_tr_sent_ms = millis(); return 0; }
  s_tr_state = 3;
  return 2;
}
extern "C" int lvd_peer_trace(const char* name) {  // 0 sent, 1 unknown contact, 2 no routed path
  ContactInfo c;
  if (!find_contact_by_name(name, c)) return 1;
  return trace_contact(c);
}

// ---- traceable contacts: those we hold a multi-hop (routed) path to ----------
// Lets the Trace screen target a contact directly, not just a hand-built
// repeater chain. Rebuilt each time the count is queried.
static uint16_t g_trc_idx[MAX_CONTACTS];
static int      g_trc_n = 0;
static void trc_build(void) {
  g_trc_n = 0;
  for (int i = MAX_ANON_CONTACTS, n = the_mesh.getTotalContactSlots(); i < n && g_trc_n < MAX_CONTACTS; i++) {
    ContactInfo c;
    if (the_mesh.getContactByIdx((uint32_t)i, c) &&
        c.out_path_len != OUT_PATH_UNKNOWN && c.out_path_len > 0)
      g_trc_idx[g_trc_n++] = (uint16_t)i;
  }
}
extern "C" int lvd_trace_contact_count(void) { trc_build(); return g_trc_n; }
extern "C" bool lvd_trace_contact_get(int i, char* name, int len, int* hops) {
  if (i < 0 || i >= g_trc_n) return false;
  ContactInfo c;
  if (!the_mesh.getContactByIdx(g_trc_idx[i], c)) return false;
  strncpy(name, c.name, len - 1); name[len - 1] = 0;
  if (hops) *hops = c.out_path_len & 63;   // low 6 bits = hop count (top 2 = hash size)
  return true;
}
extern "C" int lvd_trace_contact_go(int i) {   // 0 sent, 2 failed/unknown
  if (i < 0 || i >= g_trc_n) return 2;
  ContactInfo c;
  if (!the_mesh.getContactByIdx(g_trc_idx[i], c)) return 2;
  return trace_contact(c);
}

// ---- signal coverage (saved repeaters/rooms we've actually heard) ----------
// Only include repeaters with a recent heard advert (skip stale ones). Keep a
// filtered index into the sorted g_replist, rebuilt on each count.
struct SigEnt { uint16_t idx; int rssi; int32_t dist_m; };  // dist_m: -1 if unknown
static SigEnt g_sig[REPLIST_MAX];
static int    g_sig_n = 0;
static int    g_sig_sort = 0;   // 0 = signal, 1 = distance, 2 = name
extern "C" void lvd_signal_set_sort(int m) { g_sig_sort = (m < 0 || m > 2) ? 0 : m; }
extern "C" int  lvd_signal_sort(void) { return g_sig_sort; }

// per-repeater RSSI history (keyed by pubkey prefix) for the trend arrow
struct SigHist { uint8_t pk6[6]; int8_t rssi; bool valid; };
static SigHist g_sig_hist[REPLIST_MAX];
static int     g_sig_hist_n = 0;

static int cmp_sig_rssi(const void* a, const void* b) {
  return ((const SigEnt*)b)->rssi - ((const SigEnt*)a)->rssi;   // strongest first
}
static int cmp_sig_dist(const void* a, const void* b) {   // nearest first, unknowns last
  int32_t da = ((const SigEnt*)a)->dist_m, db = ((const SigEnt*)b)->dist_m;
  if (da < 0) da = 0x7FFFFFFF;
  if (db < 0) db = 0x7FFFFFFF;
  return (da > db) - (da < db);
}
static int cmp_sig_name(const void* a, const void* b) {
  return ci_strcmp(g_replist[((const SigEnt*)a)->idx].name, g_replist[((const SigEnt*)b)->idx].name);
}
// straight-line distance (m) to a peer fix, or -1 if either side lacks a position
static int32_t sig_distance_m(int32_t lat_e6, int32_t lon_e6) {
  if ((lat_e6 == 0 && lon_e6 == 0) || (sensors.node_lat == 0.0 && sensors.node_lon == 0.0)) return -1;
  const double R = 6371000.0, D2R = M_PI / 180.0;
  double la1 = sensors.node_lat * D2R, la2 = (lat_e6 / 1e6) * D2R;
  double dla = la2 - la1, dlo = ((lon_e6 / 1e6) - sensors.node_lon) * D2R;
  double a = sin(dla / 2) * sin(dla / 2) + cos(la1) * cos(la2) * sin(dlo / 2) * sin(dlo / 2);
  return (int32_t)(R * 2.0 * atan2(sqrt(a), sqrt(1.0 - a)));
}
static void sig_build(void) {
  rep_build(0);
  g_sig_n = 0;
  for (int i = 0; i < g_replist_n; i++) {
    AdvertPath h;
    if (find_heard_by_pubkey(g_replist[i].id.pub_key, h) && h.rssi) {
      ContactInfo& c = g_replist[i];
      int32_t lat = c.gps_lat ? c.gps_lat : h.gps_lat, lon = c.gps_lon ? c.gps_lon : h.gps_lon;
      g_sig[g_sig_n].idx = (uint16_t)i; g_sig[g_sig_n].rssi = h.rssi;
      g_sig[g_sig_n].dist_m = sig_distance_m(lat, lon);
      g_sig_n++;
    }
  }
  qsort(g_sig, g_sig_n, sizeof(g_sig[0]),
        g_sig_sort == 1 ? cmp_sig_dist : g_sig_sort == 2 ? cmp_sig_name : cmp_sig_rssi);
}
// trend vs the RSSI recorded at the previous refresh (>=3 dB deadband), then update
static int sig_trend_update(const uint8_t* pk6, int rssi) {
  SigHist* e = NULL;
  for (int k = 0; k < g_sig_hist_n; k++) if (memcmp(g_sig_hist[k].pk6, pk6, 6) == 0) { e = &g_sig_hist[k]; break; }
  int trend = 0;
  if (e && e->valid) { int d = rssi - e->rssi; trend = d >= 3 ? 1 : d <= -3 ? -1 : 0; }
  if (!e && g_sig_hist_n < REPLIST_MAX) { e = &g_sig_hist[g_sig_hist_n++]; memcpy(e->pk6, pk6, 6); }
  if (e) { e->rssi = (int8_t)rssi; e->valid = true; }
  return trend;
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
  // link margin: SNR above the SF demod floor (Semtech, x10 dB), like the analyzer
  int sf = the_mesh.getNodePrefs()->sf;
  static const int LIMIT_X10[] = { -75, -100, -125, -150, -175, -200 };  // SF7..SF12
  if (sf >= 7 && sf <= 12) {
    int m10 = (h.snr_q * 10 / 4) - LIMIT_X10[sf - 7], ma = m10 < 0 ? -m10 : m10;
    snprintf(out->info, sizeof(out->info), "%d dBm   SNR %s%d.%d dB   %s%d.%d dB margin",
             h.rssi, v10 < 0 ? "-" : "", a / 10, a % 10, m10 < 0 ? "-" : "+", ma / 10, ma % 10);
  } else {
    snprintf(out->info, sizeof(out->info), "%d dBm   SNR %s%d.%d dB", h.rssi, v10 < 0 ? "-" : "", a / 10, a % 10);
  }

  // route (direct vs relayed) + distance/bearing
  char rt[16];
  int hops = h.path_len & 63;   // low 6 bits = hop count (top 2 bits are the hash size)
  if (hops == 0)      snprintf(rt, sizeof(rt), "direct");
  else if (hops == 1) snprintf(rt, sizeof(rt), "1 hop");
  else                snprintf(rt, sizeof(rt), "%d hops", hops);
  int32_t lat = c.gps_lat ? c.gps_lat : h.gps_lat, lon = c.gps_lon ? c.gps_lon : h.gps_lon;
  char db[12];
  if (fmt_distance(lat, lon, db, sizeof(db))) {
    const char* brg = heard_bearing(lat, lon);
    snprintf(out->sub, sizeof(out->sub), "%s \xC2\xB7 %s%s%s", rt, db, brg[0] ? " " : "", brg);
  } else snprintf(out->sub, sizeof(out->sub), "%s", rt);

  out->trend = sig_trend_update(c.id.pub_key, h.rssi);

  uint32_t now = rtc_clock.getCurrentTime();
  uint32_t age = now > h.recv_timestamp ? now - h.recv_timestamp : 0;
  if (age < 60)        snprintf(out->age, sizeof(out->age), "%us", (unsigned)age);
  else if (age < 3600) snprintf(out->age, sizeof(out->age), "%um", (unsigned)(age / 60));
  else                 snprintf(out->age, sizeof(out->age), "%uh", (unsigned)(age / 3600));
  return true;
}
