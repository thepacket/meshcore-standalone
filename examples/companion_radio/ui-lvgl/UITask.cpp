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

extern MyMesh the_mesh;   // global mesh instance (main.cpp)

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
  void lv_peer_create(lv_obj_t* scr);
  void lv_trace_create(lv_obj_t* scr);
  void lv_terminal_create(lv_obj_t* scr);
  void lv_terminal_set_tab(int t);
  void lv_stats_create(lv_obj_t* scr);
  void lv_discover_create(lv_obj_t* scr);
  void lv_disc_create(lv_obj_t* scr);
  void lv_contacts_create(lv_obj_t* scr);
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
  else if (!strcmp(name, "settings")) lv_settings_create(s);
  else if (!strcmp(name, "edit")) lv_settings_edit_create(s);
  else if (name[0] == 's' && name[1] == 'g') lv_settings_group_create(s, atoi(name + 2));
  else if (!strcmp(name, "noise")) lv_noise_create(s);
  else if (!strcmp(name, "signal")) lv_signal_create(s);
  else if (!strcmp(name, "heard")) lv_heard_create(s);
  else if (!strcmp(name, "repeaters")) { lv_repeaters_set_tab(0); lv_repeaters_create(s); }
  else if (!strcmp(name, "scan")) { lv_repeaters_set_tab(1); lv_repeaters_create(s); }
  else if (!strcmp(name, "repeater_detail")) lv_repeater_detail_create(s);
  else if (!strcmp(name, "peer")) lv_peer_create(s);
  else if (!strcmp(name, "trace")) lv_trace_create(s);
  else if (!strcmp(name, "terminal")) { lv_terminal_set_tab(0); lv_terminal_create(s); }
  else if (!strcmp(name, "packets")) { lv_terminal_set_tab(1); lv_terminal_create(s); }
  else if (!strcmp(name, "stats")) lv_stats_create(s);
  else if (!strcmp(name, "discover")) lv_discover_create(s);
  else if (!strcmp(name, "disc")) lv_disc_create(s);
  else lv_home_create(s);
}

static char g_nav_stack[16][16];
static int  g_nav_sp = 0;

static void nav(const char* dest) {
  if (!strcmp(dest, "back")) {
    if (g_nav_sp > 1) g_nav_sp--;
  } else if (g_nav_sp < 16) {
    strncpy(g_nav_stack[g_nav_sp], dest, 15);
    g_nav_stack[g_nav_sp][15] = 0;
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

static void flush_cb(lv_display_t* d, const lv_area_t* area, uint8_t* px_map) {
  if (g_gfx) {
    int w = area->x2 - area->x1 + 1;
    int h = area->y2 - area->y1 + 1;
    g_gfx->startWrite();
    g_gfx->pushImage(area->x1, area->y1, w, h, (lgfx::rgb565_t*)px_map);
    g_gfx->endWrite();
    radio_spi_claim();   // hand the shared output pins back to the radio
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

  lv_nav_cb = nav;       // screens route taps through here
  g_nav_sp = 0;
  nav("home");

  // drive live screens: every 1s, call the active screen's refresh hook (if any)
  lv_timer_create(refresh_timer_cb, 1000, nullptr);

  _lvgl_ready = true;
}

void UITask::loop() {
  if (_lvgl_ready) lv_timer_handler();
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
extern "C" int lvd_contact_count(void) {
  return the_mesh.getNumContacts();
}
extern "C" bool lvd_contact_get(int i, lvd_contact_t* out) {
  ContactInfo c;
  if (i < 0 || !the_mesh.getContactByIdx((uint32_t)i, c)) return false;
  strncpy(out->name, c.name, sizeof(out->name) - 1); out->name[sizeof(out->name) - 1] = 0;
  out->type = c.type;

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
extern "C" unsigned lvd_pkt_recv(void)    { return radio_driver.getPacketsRecv(); }

// ---- packet monitor --------------------------------------------------------
#define PKT_LOG 32
struct PktRec { uint8_t header; int rssi; int snr_q; int len; uint32_t t; };
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
  s_pkt_head = (s_pkt_head + 1) % PKT_LOG;
  if (s_pkt_n < PKT_LOG) s_pkt_n++;
  s_pkt_total++;
}

extern "C" int      lvd_packet_count(void) { return s_pkt_n; }
extern "C" unsigned lvd_packet_total(void) { return s_pkt_total; }

extern "C" bool lvd_packet_get(int i, lvd_packet_t* out) {
  if (i < 0 || i >= s_pkt_n) return false;
  int idx = (s_pkt_head - 1 - i + PKT_LOG * 2) % PKT_LOG;   // newest first
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

// ---- discover (heard-but-unsaved nodes) ------------------------------------
static const char* adv_type_name(int t) {
  return t == ADV_TYPE_CHAT     ? "Companion" :
         t == ADV_TYPE_REPEATER ? "Repeater"  :
         t == ADV_TYPE_ROOM     ? "Room"      :
         t == ADV_TYPE_SENSOR   ? "Sensor"    : "Node";
}

static ContactInfo g_disc[16];
static int         g_disc_n = 0;

extern "C" int lvd_disc_count(void) {
  g_disc_n = the_mesh.getHeardCandidates(g_disc, 16);
  return g_disc_n;
}
extern "C" bool lvd_disc_get(int i, lvd_disc_t* out) {
  if (i < 0 || i >= g_disc_n) return false;
  ContactInfo& c = g_disc[i];
  strncpy(out->name, c.name, sizeof(out->name) - 1); out->name[sizeof(out->name) - 1] = 0;
  out->type = c.type;
  snprintf(out->subtitle, sizeof(out->subtitle), "%s  -  tap to add", adv_type_name(c.type));
  return true;
}
extern "C" void lvd_disc_add(int i) {
  if (i >= 0 && i < g_disc_n) the_mesh.addHeardContact(g_disc[i].id.pub_key);
}
extern "C" void lvd_disc_announce(void) { the_mesh.advert(); }

// ---- chat store (Public channel + DMs) -------------------------------------
#define MSG_LOG 32
struct MsgRec { bool is_ch; int ch; uint8_t peer6[6]; char who[24]; char text[124]; bool out; };
static MsgRec s_msg[MSG_LOG];
static int s_msg_head = 0, s_msg_n = 0;
static unsigned s_msg_total = 0;

// active conversation: Public channel, or a DM with a specific contact
static bool        s_conv_public = true;
static uint8_t     s_conv_peer[6];
static ContactInfo s_conv_contact;
static bool        s_conv_has_contact = false;

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
  if (s_conv_public) return m.is_ch && m.ch == 0;
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

extern "C" void lvd_chat_open_public(void) { s_conv_public = true; }
extern "C" void lvd_chat_open_dm(const char* contact_name) {
  s_conv_public = false;
  s_conv_has_contact = find_contact_by_name(contact_name, s_conv_contact);
  if (s_conv_has_contact) memcpy(s_conv_peer, s_conv_contact.id.pub_key, 6);
  else memset(s_conv_peer, 0, 6);
}
extern "C" const char* lvd_chat_title(void) {
  return s_conv_public ? "Public" : (s_conv_has_contact ? s_conv_contact.name : "Direct");
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
  if (s_conv_public) {
    if (the_mesh.sendChannelText(0, text)) ui_store_message(true, 0, NULL, "You", text, true);
  } else if (s_conv_has_contact) {
    uint32_t ack, timeout;
    if (the_mesh.sendTextTo(s_conv_contact, text, ack, timeout))
      ui_store_message(false, -1, s_conv_peer, "You", text, true);
  }
}

// ---- trace route -----------------------------------------------------------
#define TRACE_MAX 16
static int      s_tr_state = 0;          // 0 idle, 1 tracing, 2 done, 3 no-path
static char     s_tr_target[24] = "";
static uint32_t s_tr_tag = 0;
static int      s_tr_hops = 0;           // intermediate hops
static uint8_t  s_tr_hash[TRACE_MAX];
static int8_t   s_tr_snr[TRACE_MAX];
static int8_t   s_tr_final = 0;
static unsigned s_tr_seq = 0;

void ui_store_trace(uint32_t tag, const uint8_t* hashes, const uint8_t* snrs,
                    uint8_t path_len, uint8_t path_sz, int8_t final_snr_q) {
  if (s_tr_state == 1 && tag != s_tr_tag) return;   // not the trace we asked for
  int n = path_len >> path_sz;                      // SNR entries (== hops)
  if (n > TRACE_MAX) n = TRACE_MAX;
  s_tr_hops = n;
  for (int i = 0; i < n; i++) { s_tr_hash[i] = hashes[i]; s_tr_snr[i] = snrs[i]; }
  s_tr_final = final_snr_q;
  s_tr_state = 2;
  s_tr_seq++;
}

extern "C" void lvd_trace_start(const char* contact_name) {
  s_tr_seq++;
  ContactInfo c;
  if (!find_contact_by_name(contact_name, c)) { s_tr_state = 3; s_tr_target[0] = 0; return; }
  strncpy(s_tr_target, c.name, sizeof(s_tr_target) - 1); s_tr_target[sizeof(s_tr_target) - 1] = 0;
  uint32_t tag;
  if (the_mesh.sendTrace(c, tag)) { s_tr_tag = tag; s_tr_hops = 0; s_tr_state = 1; }
  else { s_tr_state = 3; }   // no known path
}
extern "C" int         lvd_trace_state(void)  { return s_tr_state; }
extern "C" const char* lvd_trace_target(void) { return s_tr_target; }
extern "C" unsigned    lvd_trace_seq(void)    { return s_tr_seq; }
extern "C" int         lvd_trace_count(void)  { return s_tr_state == 2 ? s_tr_hops + 1 : 0; }
extern "C" bool        lvd_trace_get(int i, lvd_hop_t* out) {
  if (s_tr_state != 2 || i < 0 || i > s_tr_hops) return false;
  int8_t q = (i < s_tr_hops) ? s_tr_snr[i] : s_tr_final;
  if (i < s_tr_hops) snprintf(out->left, sizeof(out->left), "%d.  id %02X", i + 1, s_tr_hash[i]);
  else               snprintf(out->left, sizeof(out->left), "%s  to you", LV_SYMBOL_DOWN);
  int v10 = q * 10 / 4, a = v10 < 0 ? -v10 : v10;
  snprintf(out->snr, sizeof(out->snr), "%s%d.%d dB", v10 < 0 ? "-" : "+", a / 10, a % 10);
  out->quality = q >= 20 ? 2 : (q >= 0 ? 1 : 0);
  return true;
}
